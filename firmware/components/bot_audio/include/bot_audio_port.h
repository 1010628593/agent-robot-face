#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_codec_dev.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Audio port configuration
 */
typedef struct {
    uint32_t sample_rate;       // Sample rate in Hz
    uint8_t bits_per_sample;    // Bits per sample (16)
    uint8_t channels;           // Number of channels (1 for mono)
    uint32_t buffer_ms;         // Buffer size in ms
} bot_audio_port_config_t;

/**
 * @brief Audio port statistics
 */
typedef struct {
    uint32_t total_reads;       // Total number of reads
    uint32_t failed_reads;      // Number of failed reads
    uint32_t timeout_reads;     // Number of timeout reads
    uint32_t bytes_read;        // Total bytes read
    uint32_t last_read_ms;      // Last read timestamp
    uint32_t last_error_ms;     // Last error timestamp
    esp_err_t last_error;       // Last error code
} bot_audio_port_stats_t;

/**
 * @brief Initialize audio port
 * 
 * This function initializes the audio hardware and codec.
 * It should be called once at startup.
 * 
 * @param config Audio configuration
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t bot_audio_port_init(const bot_audio_port_config_t *config);

/**
 * @brief Open audio port for reading
 * 
 * This function opens the audio codec for reading.
 * Must be called before bot_audio_port_read().
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t bot_audio_port_open(void);

/**
 * @brief Read audio data
 * 
 * This function reads audio data from the codec.
 * It blocks until data is available or timeout occurs.
 * 
 * @param data Buffer to store audio data
 * @param size Size of buffer in bytes
 * @param bytes_read Number of bytes actually read
 * @param timeout_ms Timeout in milliseconds
 * @return ESP_OK on success, ESP_ERR_TIMEOUT on timeout, error code otherwise
 */
esp_err_t bot_audio_port_read(void *data, size_t size, size_t *bytes_read, uint32_t timeout_ms);

/**
 * @brief Close audio port
 * 
 * This function closes the audio codec and releases resources.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t bot_audio_port_close(void);

/**
 * @brief Deinitialize audio port
 * 
 * This function releases all audio resources.
 * Should be called at shutdown.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t bot_audio_port_deinit(void);

/**
 * @brief Get audio port statistics
 * 
 * @param stats Output statistics structure
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t bot_audio_port_get_stats(bot_audio_port_stats_t *stats);

/**
 * @brief Check if audio port is open
 * 
 * @return true if port is open, false otherwise
 */
bool bot_audio_port_is_open(void);

#ifdef __cplusplus
}
#endif