/* Application-owned adapter for BSP 3.0.1 pins / ES7210 codec 1.5.11.
 * See docs/G0-audio-contract-verification.md for provenance and hardware gates.
 * No esp_codec_dev data wrapper: it discards bytes_read and ignores caller timeout.
 * No BSP audio allocation: its shared opaque handles cannot be safely torn down.
 */
#include "bot_audio_port.h"
#include "sdkconfig.h"
#include <string.h>
#if CONFIG_BOT_AUDIO_ENABLE
#include "bsp/esp-bsp.h"
#include "driver/i2s_std.h"
#include "driver/i2c_master.h"
#include "esp_codec_dev_defaults.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

#define SAMPLE_RATE 22050u
#define DMA_FRAMES 220u
#define DMA_COUNT 6u
static portMUX_TYPE s_clock_lock = portMUX_INITIALIZER_UNLOCKED;
static struct {
    bool initialized, opened, rx_enabled, adc_dirty;
    i2s_chan_handle_t rx, tx_reservation;
    i2c_master_dev_handle_t ctrl_device;
    const audio_codec_if_t *codec;
    bot_audio_port_stats_t stats;
    bot_audio_port_read_meta_t meta;
    uint32_t generation, overflows;
    uint64_t first_dma_end_us, first_dma_frames, consumed_frames;
    bool previous_gap;
} s;

static bool ctrl_opened(const audio_codec_ctrl_if_t *unused) { return s.ctrl_device != NULL; }
static int ctrl_read(const audio_codec_ctrl_if_t *unused, int reg, int reg_len, void *data, int len) {
    if (reg_len != 1 || len < 1 || !s.ctrl_device) return ESP_CODEC_DEV_INVALID_ARG;
    uint8_t addr = reg;
    return i2c_master_transmit_receive(s.ctrl_device, &addr, 1, data, len, 100) == ESP_OK
        ? ESP_CODEC_DEV_OK : ESP_CODEC_DEV_READ_FAIL;
}
static int ctrl_write(const audio_codec_ctrl_if_t *unused, int reg, int reg_len, void *data, int len) {
    if (reg_len != 1 || len != 1 || !s.ctrl_device) return ESP_CODEC_DEV_INVALID_ARG;
    uint8_t bytes[2] = {(uint8_t)reg, *(uint8_t *)data};
    return i2c_master_transmit(s.ctrl_device, bytes, sizeof(bytes), 100) == ESP_OK
        ? ESP_CODEC_DEV_OK : ESP_CODEC_DEV_WRITE_FAIL;
}
/* Static borrowed interface: never pass to audio_codec_delete_ctrl_if (which frees it). */
static const audio_codec_ctrl_if_t s_ctrl = {
    .is_open = ctrl_opened, .read_reg = ctrl_read, .write_reg = ctrl_write,
};
static bool IRAM_ATTR dma_received(i2s_chan_handle_t channel, i2s_event_data_t *event, void *arg) {
    uint64_t now = esp_timer_get_time();
    portENTER_CRITICAL_ISR(&s_clock_lock);
    if (!s.first_dma_end_us) {
        s.first_dma_end_us = now;
        s.first_dma_frames = event->size / sizeof(int16_t);
    }
    portEXIT_CRITICAL_ISR(&s_clock_lock);
    return false;
}
static bool IRAM_ATTR dma_overflow(i2s_chan_handle_t channel, i2s_event_data_t *event, void *arg) {
    portENTER_CRITICAL_ISR(&s_clock_lock);
    s.overflows++;
    portEXIT_CRITICAL_ISR(&s_clock_lock);
    return false;
}
/* Explicit checked shutdown also covers ES7210 constructor failing mid-register setup.
 * Register sequence from esp_codec_dev 1.5.11 es7210_stop, Apache-2.0 Espressif.
 * Unlike codec->close, no cached enabled bit can skip the power-down writes. */
static esp_err_t adc_shutdown(void) {
    if (!s.adc_dirty) return ESP_OK;
    static const uint8_t regs[][2] = {
        {0x47,0xff},{0x48,0xff},{0x49,0xff},{0x4a,0xff},
        {0x4b,0xff},{0x4c,0xff},{0x40,0xc0},{0x01,0x7f},{0x06,0x07}
    };
    esp_err_t result = ESP_OK;
    for (unsigned i=0; i<sizeof(regs)/sizeof(regs[0]); ++i) {
        esp_err_t err = i2c_master_transmit(s.ctrl_device, regs[i], 2, 100);
        if (err != ESP_OK) result = err;
    }
    if (result == ESP_OK) s.adc_dirty = false;
    return result;
}
esp_err_t bot_audio_port_init(const bot_audio_port_config_t *config) {
    if (!config) return ESP_ERR_INVALID_ARG;
    if ((config->sample_rate && config->sample_rate != SAMPLE_RATE) ||
        (config->bits_per_sample && config->bits_per_sample != 16) ||
        (config->channels && config->channels != 1)) return ESP_ERR_NOT_SUPPORTED;
    if (s.initialized) return ESP_ERR_INVALID_STATE;
    memset(&s.stats, 0, sizeof(s.stats));
    memset(&s.meta, 0, sizeof(s.meta));
    s.initialized = true;
    return ESP_OK;
}
esp_err_t bot_audio_port_close(void) {
    if (!s.initialized) return ESP_OK;
    esp_err_t err = adc_shutdown();
    /* Always try stopping RX even if ADC control fails, but preserve failure. */
    if (s.rx_enabled) {
        esp_err_t stop = i2s_channel_disable(s.rx);
        if (stop != ESP_OK) return stop;
        s.rx_enabled = false;
    }
    if (err != ESP_OK) return err; /* Keep owned resources for retry; never report disabled. */
    if (s.codec) {
        int rc = s.codec->enable(s.codec, false);
        if (rc != ESP_CODEC_DEV_OK) return ESP_FAIL;
        rc = audio_codec_delete_codec_if(s.codec);
        s.codec = NULL; /* delete frees even on failure */
        if (rc != ESP_CODEC_DEV_OK) return ESP_FAIL;
    }
    /* The source-controlled I2S free adaptation volatile-clears every DMA block,
     * including a partially filled block that has never reached a callback. */
    if (s.rx) {
        err = i2s_del_channel(s.rx);
        if (err != ESP_OK) return err;
        s.rx = NULL;
    }
    if (s.tx_reservation) {
        err = i2s_del_channel(s.tx_reservation);
        if (err != ESP_OK) return err;
        s.tx_reservation = NULL;
    }
    if (s.ctrl_device) {
        err = i2c_master_bus_rm_device(s.ctrl_device);
        if (err != ESP_OK) return err;
        s.ctrl_device = NULL;
    }
    s.opened = false;
    s.consumed_frames = 0;
    memset(&s.meta, 0, sizeof(s.meta));
    return ESP_OK;
}
esp_err_t bot_audio_port_open(void) {
    if (!s.initialized) return ESP_ERR_INVALID_STATE;
    if (s.opened) return ESP_ERR_INVALID_STATE;
    if (s.rx || s.tx_reservation || s.ctrl_device || s.codec) return ESP_ERR_INVALID_STATE;
    esp_err_t err = bsp_i2c_init();
    if (err != ESP_OK) return err;
    /* Reserve BOTH halves atomically: BSP/speaker ownership causes allocation failure.
     * TX remains REGISTERED, never initialized or enabled, never drives PA/DOUT. */
    i2s_chan_config_t chan = I2S_CHANNEL_DEFAULT_CONFIG(CONFIG_BSP_I2S_NUM, I2S_ROLE_MASTER);
    chan.dma_desc_num = DMA_COUNT;
    chan.dma_frame_num = DMA_FRAMES;
    err = i2s_new_channel(&chan, &s.tx_reservation, &s.rx);
    if (err != ESP_OK) return err;
    i2s_std_config_t std = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {.mclk=BSP_I2S_MCLK, .bclk=BSP_I2S_SCLK, .ws=BSP_I2S_LCLK,
                     .dout=I2S_GPIO_UNUSED, .din=BSP_I2S_DSIN},
    };
    err = i2s_channel_init_std_mode(s.rx, &std);
    if (err != ESP_OK) goto fail;
    i2s_event_callbacks_t callbacks = {.on_recv=dma_received, .on_recv_q_ovf=dma_overflow};
    err = i2s_channel_register_event_callback(s.rx, &callbacks, NULL);
    if (err != ESP_OK) goto fail;
    i2c_device_config_t i2c = {.dev_addr_length=I2C_ADDR_BIT_LEN_7,
        .device_address=(ES7210_CODEC_DEFAULT_ADDR >> 1), .scl_speed_hz=100000};
    err = i2c_master_bus_add_device(bsp_i2c_get_handle(), &i2c, &s.ctrl_device);
    if (err != ESP_OK) goto fail;
    s.adc_dirty = true;
    es7210_codec_cfg_t cfg = {.ctrl_if=&s_ctrl, .mic_selected=ES7210_SEL_MIC1, .mclk_div=256};
    s.codec = es7210_codec_new(&cfg);
    if (!s.codec) { err = ESP_FAIL; goto fail; }
    esp_codec_dev_sample_info_t fs = {.sample_rate=SAMPLE_RATE, .channel=1, .bits_per_sample=16};
    if (s.codec->set_fs(s.codec, &fs) != ESP_CODEC_DEV_OK ||
        s.codec->set_mic_gain(s.codec, 30.0f) != ESP_CODEC_DEV_OK ||
        s.codec->enable(s.codec, true) != ESP_CODEC_DEV_OK) { err=ESP_FAIL; goto fail; }
    portENTER_CRITICAL(&s_clock_lock);
    s.first_dma_end_us = 0;
    s.first_dma_frames = 0;
    s.overflows = 0;
    portEXIT_CRITICAL(&s_clock_lock);
    s.consumed_frames = 0;
    s.previous_gap = false;
    memset(&s.meta, 0, sizeof(s.meta));
    err = i2s_channel_enable(s.rx);
    if (err != ESP_OK) goto fail;
    s.rx_enabled = true;
    s.opened = true;
    ++s.generation;
    return ESP_OK;
fail:
    {
        esp_err_t cleanup = bot_audio_port_close();
        return cleanup == ESP_OK ? err : cleanup;
    }
}
esp_err_t bot_audio_port_read(void *data, size_t size, size_t *bytes_read, uint32_t timeout_ms) {
    if (bytes_read) *bytes_read = 0;
    if (!data || !bytes_read || !size || size % sizeof(int16_t)) return ESP_ERR_INVALID_ARG;
    if (!s.opened || !s.rx_enabled) return ESP_ERR_INVALID_STATE;
    ++s.stats.total_reads;
    esp_err_t err = i2s_channel_read(s.rx, data, size, bytes_read, timeout_ms);
    uint64_t anchor, initial_frames;
    uint32_t overflows;
    portENTER_CRITICAL(&s_clock_lock);
    anchor = s.first_dma_end_us;
    initial_frames = s.first_dma_frames;
    overflows = s.overflows;
    portEXIT_CRITICAL(&s_clock_lock);
    bool first = s.consumed_frames == 0;
    s.consumed_frames += *bytes_read / sizeof(int16_t);
    bool uncertain = !anchor || overflows != 0 || *bytes_read % sizeof(int16_t);
    s.meta = (bot_audio_port_read_meta_t){
        .capture_end_us=uncertain ? 0 : anchor - initial_frames * 1000000ULL / SAMPLE_RATE
                        + s.consumed_frames * 1000000ULL / SAMPLE_RATE,
        .generation=s.generation, .overflow_count=overflows,
        .gap=s.previous_gap || uncertain || err != ESP_OK || *bytes_read != size,
        .reconfigured=first, .clock_uncertain=uncertain,
    };
    s.previous_gap = err != ESP_OK || *bytes_read != size;
    s.stats.bytes_read += *bytes_read;
    s.stats.last_read_ms = (uint32_t)(esp_timer_get_time()/1000);
    if (err != ESP_OK) {
        ++s.stats.failed_reads;
        if (err == ESP_ERR_TIMEOUT) ++s.stats.timeout_reads;
        s.stats.last_error = err;
        s.stats.last_error_ms = s.stats.last_read_ms;
    }
    return err;
}
esp_err_t bot_audio_port_get_last_read(bot_audio_port_read_meta_t *meta) {
    if (!meta) return ESP_ERR_INVALID_ARG;
    *meta = s.meta;
    return ESP_OK;
}
esp_err_t bot_audio_port_deinit(void) {
    esp_err_t err = bot_audio_port_close();
    if (err == ESP_OK) s.initialized = false;
    return err;
}
esp_err_t bot_audio_port_get_stats(bot_audio_port_stats_t *stats) {
    if (!stats) return ESP_ERR_INVALID_ARG;
    *stats = s.stats;
    return ESP_OK;
}
bool bot_audio_port_is_open(void) { return s.opened; }
#else
esp_err_t bot_audio_port_init(const bot_audio_port_config_t *c) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t bot_audio_port_open(void) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t bot_audio_port_read(void *d,size_t n,size_t *got,uint32_t t) { if(got)*got=0; return ESP_ERR_NOT_SUPPORTED; }
esp_err_t bot_audio_port_close(void) { return ESP_OK; }
esp_err_t bot_audio_port_deinit(void) { return ESP_OK; }
esp_err_t bot_audio_port_get_last_read(bot_audio_port_read_meta_t *m) { if(!m)return ESP_ERR_INVALID_ARG; memset(m,0,sizeof(*m)); return ESP_ERR_NOT_SUPPORTED; }
esp_err_t bot_audio_port_get_stats(bot_audio_port_stats_t *st) { if(!st)return ESP_ERR_INVALID_ARG; memset(st,0,sizeof(*st)); return ESP_ERR_NOT_SUPPORTED; }
bool bot_audio_port_is_open(void) { return false; }
#endif
