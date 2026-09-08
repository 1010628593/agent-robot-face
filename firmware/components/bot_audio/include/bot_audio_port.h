#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Owner-thread-only API. Never call read/close concurrently; UI uses service snapshots. */
typedef struct {
    uint32_t sample_rate;
    uint8_t bits_per_sample;
    uint8_t channels;
    uint32_t buffer_ms;
} bot_audio_port_config_t;
typedef struct {
    uint32_t total_reads, failed_reads, timeout_reads, bytes_read;
    uint32_t last_read_ms, last_error_ms;
    esp_err_t last_error;
} bot_audio_port_stats_t;
typedef struct {
    /* Sample end time anchored to the first RX DMA completion, never dequeue time.
     * ISR latency is included; physical clock accuracy remains a G0 measurement. */
    uint64_t capture_end_us;
    uint32_t generation;
    uint32_t overflow_count;
    bool gap;
    bool reconfigured;
    bool clock_uncertain;
} bot_audio_port_read_meta_t;
esp_err_t bot_audio_port_init(const bot_audio_port_config_t *config);
esp_err_t bot_audio_port_open(void);
/* Always initializes bytes_read, including errors; lengths and timeout are native I2S results. */
esp_err_t bot_audio_port_read(void *data, size_t size, size_t *bytes_read, uint32_t timeout_ms);
esp_err_t bot_audio_port_get_last_read(bot_audio_port_read_meta_t *meta);
/* Success means RX disabled, reserved channels deleted, ADC shutdown writes checked,
 * and this port's I2C device removed. Shared I2C bus remains owned by BSP. */
esp_err_t bot_audio_port_close(void);
esp_err_t bot_audio_port_deinit(void);
esp_err_t bot_audio_port_get_stats(bot_audio_port_stats_t *stats);
bool bot_audio_port_is_open(void);
#ifdef __cplusplus
}
#endif
