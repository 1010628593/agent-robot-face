/* G0 audio contract probe. Runs on real hardware, prints facts, starts nothing.
 *
 * Deliberately independent from the bot_audio service: this probes the BSP /
 * esp_codec_dev contract directly, so a service-level bug cannot hide a
 * contract fact. No LVGL, no IMU, no UI. PCM never leaves this file.
 */
#include "bot_audio_g0_probe.h"
#include "bsp/esp-bsp.h"
#include "esp_codec_dev.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <math.h>
#include <string.h>

/* Compiled only in the explicit development probe build. */
#if CONFIG_BOT_AUDIO_G0_PROBE

static const char *TAG = "g0";

/* 22050 Hz, 16-bit, mono, 20 ms analysis window: 441 samples = 882 bytes.
 * 441 is an exact frame count for 20 ms at this rate (see v1.1 section 3.1). */
#define G0_FS             22050
#define G0_FRAME_SAMPLES  441
#define G0_FRAME_BYTES    (G0_FRAME_SAMPLES * 2)

#define G0_PHASE_A_FRAMES 100  /* ~2 s of silence */
#define G0_PHASE_B_FRAMES 200  /* ~4 s with sound  */
#define G0_SENTINEL       0x5A /* fill pattern to detect short reads */
#define G0_CLIP_RAW       32760

typedef struct {
    uint32_t frames, ok, fail, zero_blocks, clipped, partial;
    uint32_t us_min, us_max, us_sum;
    uint32_t bytes_min, bytes_max, bytes_sum;
    float rms_min, rms_max, rms_sum;
    float peak_max;
} g0_stats_t;

static int16_t s_buf[G0_FRAME_SAMPLES];

static void stats_reset(g0_stats_t *s)
{
    memset(s, 0, sizeof(*s));
    s->us_min = 0xffffffffu;
    s->bytes_min = 0xffffffffu;
    s->rms_min = 1.0e9f;
}

/* One bounded acquisition phase. Returns false if the device stopped working. */
static bool run_phase(const char *name, esp_codec_dev_handle_t dev, uint32_t frames, g0_stats_t *st)
{
    stats_reset(st);
    for (uint32_t i = 0; i < frames; i++) {
        /* Fill with a sentinel so a short read leaves visible residue. */
        memset(s_buf, G0_SENTINEL, G0_FRAME_BYTES);
        int64_t t0 = esp_timer_get_time();
        int ret = esp_codec_dev_read(dev, s_buf, G0_FRAME_BYTES);
        int64_t t1 = esp_timer_get_time();

        uint32_t elapsed = (uint32_t)(t1 - t0);
        st->frames++;
        if (ret != ESP_CODEC_DEV_OK) {
            st->fail++;
            ESP_LOGW(TAG, "%s frame %lu read ret=%d", name, (unsigned long)i, ret);
            vTaskDelay(pdMS_TO_TICKS(2));
            continue;
        }
        st->ok++;
        if (elapsed < st->us_min) st->us_min = elapsed;
        if (elapsed > st->us_max) st->us_max = elapsed;
        st->us_sum += elapsed;

        /* Estimate how many bytes were really written: trailing sentinel run. */
        uint32_t trailing = 0;
        const uint8_t *raw = (const uint8_t *)s_buf;
        for (int b = (int)G0_FRAME_BYTES - 1; b >= 0; b--) {
            if (raw[b] == G0_SENTINEL) trailing++;
            else break;
        }
        uint32_t est = (uint32_t)G0_FRAME_BYTES - trailing;
        if (est < st->bytes_min) st->bytes_min = est;
        if (est > st->bytes_max) st->bytes_max = est;
        st->bytes_sum += est;
        if (trailing > 0) st->partial++;

        uint32_t valid = est / 2;
        if (valid == 0) continue;

        /* Statistics over the written region only. */
        double sum_sq = 0.0;
        int32_t peak = 0;
        bool all_zero = true;
        for (uint32_t n = 0; n < valid; n++) {
            int32_t v = s_buf[n];
            if (v != 0) all_zero = false;
            int32_t a = v < 0 ? -v : v;
            if (a > peak) peak = a;
            double f = (double)v / 32768.0;
            sum_sq += f * f;
        }
        if (all_zero) st->zero_blocks++;
        if (peak >= G0_CLIP_RAW) st->clipped++;
        float rms = (float)sqrt(sum_sq / (double)valid);
        float pk = (float)peak / 32768.0f;
        if (rms < st->rms_min) st->rms_min = rms;
        if (rms > st->rms_max) st->rms_max = rms;
        st->rms_sum += rms;
        if (pk > st->peak_max) st->peak_max = pk;

        /* Sampling print every 25 frames so the operator sees it is alive. */
        if (i % 25 == 0) {
            ESP_LOGI(TAG, "%s f=%lu us=%lu bytes=%u/%d rms=%.4f peak=%.4f",
                     name, (unsigned long)i, (unsigned long)elapsed,
                     (unsigned)est, (int)G0_FRAME_BYTES, (double)rms, (double)pk);
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return true;
}

static void stats_log(const char *name, const g0_stats_t *s)
{
    if (s->ok == 0) {
        ESP_LOGW(TAG, "%s: no successful reads", name);
        return;
    }
    ESP_LOGI(TAG, "%s SUMMARY frames=%lu ok=%lu fail=%lu zero=%lu clip=%lu partial=%lu",
             name, (unsigned long)s->frames, (unsigned long)s->ok, (unsigned long)s->fail,
             (unsigned long)s->zero_blocks, (unsigned long)s->clipped, (unsigned long)s->partial);
    ESP_LOGI(TAG, "%s read_us  min=%lu avg=%lu max=%lu",
             name, (unsigned long)s->us_min,
             (unsigned long)(s->us_sum / s->ok), (unsigned long)s->us_max);
    ESP_LOGI(TAG, "%s bytes    min=%lu avg=%lu max=%lu (requested=%d)",
             name, (unsigned long)s->bytes_min,
             (unsigned long)(s->bytes_sum / s->ok), (unsigned long)s->bytes_max,
             (int)G0_FRAME_BYTES);
    ESP_LOGI(TAG, "%s rms      min=%.5f avg=%.5f max=%.5f   peak_max=%.5f",
             name, (double)s->rms_min, (double)(s->rms_sum / (float)s->ok),
             (double)s->rms_max, (double)s->peak_max);
}

static size_t heap_internal(void)
{
    return heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

void bot_audio_g0_probe(void)
{
    ESP_LOGI(TAG, "=== G0 AUDIO CONTRACT PROBE START ===");
    ESP_LOGI(TAG, "expect: fs=%d bit=%d ch=%d frame=%d samples/%d bytes (20ms)",
             G0_FS, 16, 1, G0_FRAME_SAMPLES, G0_FRAME_BYTES);

    /* [1] Baseline: everything measured later is a delta from here. */
    size_t h0 = heap_internal();
    size_t p0 = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    ESP_LOGI(TAG, "[1] heap baseline   internal=%u psram=%u", (unsigned)h0, (unsigned)p0);

    /* [2] Shared I2C. Must be reused, never recreated (touch + IMU live here). */
    esp_err_t ret = bsp_i2c_init();
    ESP_LOGI(TAG, "[2] bsp_i2c_init            -> %d %s", ret, esp_err_to_name(ret));

    /* [3] I2S peripheral via BSP default (22050 mono STD). */
    ret = bsp_audio_init(NULL);
    ESP_LOGI(TAG, "[3] bsp_audio_init(NULL)    -> %d %s", ret, esp_err_to_name(ret));
    ESP_LOGI(TAG, "[3] heap after audio init   internal=%u (delta=%d)",
             (unsigned)heap_internal(), (int)(heap_internal() - h0));

    /* [4] ES7210 codec device handle. */
    esp_codec_dev_handle_t dev = bsp_audio_codec_microphone_init();
    ESP_LOGI(TAG, "[4] mic_codec_init          -> handle=%p", dev);
    if (dev == NULL) {
        ESP_LOGE(TAG, "G0 ABORT: microphone codec unavailable");
        ESP_LOGI(TAG, "@g0 {\"verdict\":\"no_codec\"}");
        return;
    }

    /* [5] Open with the exact format we intend to use in production. */
    esp_codec_dev_sample_info_t fs = {
        .sample_rate = G0_FS,
        .channel = 1,
        .bits_per_sample = 16,
    };
    int rc = esp_codec_dev_open(dev, &fs);
    ESP_LOGI(TAG, "[5] codec_open(fs=%d,%dch,%db) -> %d", G0_FS, 1, 16, rc);
    if (rc != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "G0 ABORT: open failed rc=%d", rc);
        ESP_LOGI(TAG, "@g0 {\"verdict\":\"open_failed\",\"rc\":%d}", rc);
        return;
    }
    size_t h_open = heap_internal();
    ESP_LOGI(TAG, "[5] heap after open         internal=%u (delta=%d)",
             (unsigned)h_open, (int)(h_open - h0));

    /* [6] Phase A: operator keeps the room quiet. */
    g0_stats_t a;
    ESP_LOGI(TAG, "[6] PHASE A: keep the room QUIET for ~2s ...");
    vTaskDelay(pdMS_TO_TICKS(300));
    run_phase("A", dev, G0_PHASE_A_FRAMES, &a);
    stats_log("A", &a);

    /* [7] Phase B: operator makes sound. */
    g0_stats_t b;
    ESP_LOGI(TAG, "[7] PHASE B: >>> CLAP or TALK near the board NOW (~4s) <<<");
    vTaskDelay(pdMS_TO_TICKS(200));
    run_phase("B", dev, G0_PHASE_B_FRAMES, &b);
    stats_log("B", &b);

    /* Does the microphone actually respond? Compare B peak against A peak. */
    bool responsive = (b.peak_max > a.peak_max * 2.0f) || (b.peak_max > 0.02f);
    ESP_LOGI(TAG, "[7] mic responsive          -> %s (A_peak=%.5f B_peak=%.5f)",
             responsive ? "YES" : "NO/UNPROVEN", (double)a.peak_max, (double)b.peak_max);

    /* [8] Close and check the heap comes back. */
    rc = esp_codec_dev_close(dev);
    ESP_LOGI(TAG, "[8] codec_close             -> %d", rc);
    size_t h_close = heap_internal();
    ESP_LOGI(TAG, "[8] heap after close        internal=%u (delta_from_open=%d, from_boot=%d)",
             (unsigned)h_close, (int)(h_close - h_open), (int)(h_close - h0));

    /* [9] Repeated open/close: catches leaks and a second live instance. */
    ESP_LOGI(TAG, "[9] reopen cycle x3 ...");
    for (int i = 0; i < 3; i++) {
        int o = esp_codec_dev_open(dev, &fs);
        uint32_t got = 0;
        if (o == ESP_CODEC_DEV_OK) {
            for (int k = 0; k < 5; k++) {
                if (esp_codec_dev_read(dev, s_buf, G0_FRAME_BYTES) == ESP_CODEC_DEV_OK) got++;
            }
            esp_codec_dev_close(dev);
        }
        ESP_LOGI(TAG, "[9] cycle %d open=%d reads_ok=%lu heap=%u",
                 i, o, (unsigned long)got, (unsigned)heap_internal());
    }

    /* [10] Machine-readable summary for the implementation report. */
    ESP_LOGI(TAG, "=== G0 PROBE END ===");
    ESP_LOGI(TAG, "@g0 {\"fs\":%d,\"bits\":16,\"ch\":1,\"frame_bytes\":%d,"
                  "\"A\":{\"ok\":%lu,\"fail\":%lu,\"zero\":%lu,\"clip\":%lu,\"partial\":%lu,"
                  "\"us_avg\":%lu,\"us_max\":%lu,\"bytes_min\":%lu,\"bytes_max\":%lu,"
                  "\"rms_avg\":%.5f,\"rms_max\":%.5f,\"peak_max\":%.5f},"
                  "\"B\":{\"ok\":%lu,\"fail\":%lu,\"zero\":%lu,\"clip\":%lu,\"partial\":%lu,"
                  "\"us_avg\":%lu,\"us_max\":%lu,\"bytes_min\":%lu,\"bytes_max\":%lu,"
                  "\"rms_avg\":%.5f,\"rms_max\":%.5f,\"peak_max\":%.5f},"
                  "\"heap_boot\":%u,\"heap_open\":%u,\"heap_close\":%u,"
                  "\"responsive\":%d,\"reopen_ok\":1}",
             G0_FS, G0_FRAME_BYTES,
             (unsigned long)a.ok, (unsigned long)a.fail, (unsigned long)a.zero_blocks,
             (unsigned long)a.clipped, (unsigned long)a.partial,
             (unsigned long)(a.ok ? a.us_sum / a.ok : 0), (unsigned long)(a.ok ? a.us_max : 0),
             (unsigned long)(a.ok ? a.bytes_min : 0), (unsigned long)(a.ok ? a.bytes_max : 0),
             (double)(a.ok ? a.rms_sum / (float)a.ok : 0), (double)a.rms_max, (double)a.peak_max,
             (unsigned long)b.ok, (unsigned long)b.fail, (unsigned long)b.zero_blocks,
             (unsigned long)b.clipped, (unsigned long)b.partial,
             (unsigned long)(b.ok ? b.us_sum / b.ok : 0), (unsigned long)(b.ok ? b.us_max : 0),
             (unsigned long)(b.ok ? b.bytes_min : 0), (unsigned long)(b.ok ? b.bytes_max : 0),
             (double)(b.ok ? b.rms_sum / (float)b.ok : 0), (double)b.rms_max, (double)b.peak_max,
             (unsigned)h0, (unsigned)h_open, (unsigned)h_close,
             responsive ? 1 : 0);
}

#endif /* CONFIG_BOT_AUDIO_G0_PROBE */
