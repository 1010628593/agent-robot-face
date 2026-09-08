#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "bot_audio_types.h"

/**
 * @brief Start audio service
 * 
 * This function requests audio service start. It does not guarantee immediate PCM availability.
 * Check service_state in bot_audio_view_t for actual status.
 * 
 * @return ESP_OK on success, error code otherwise
 */
/* Startup allocates the owner only; it never enables microphone capture. */
esp_err_t bot_audio_init(void);
esp_err_t bot_audio_request_config(const bot_audio_config_t *config);
void bot_audio_set_environment(bool suppress_background, bool motion_available);
esp_err_t bot_audio_start(void);

/**
 * @brief Stop audio service
 * 
 * @param timeout_ms Timeout in milliseconds to wait for stop
 * @return ESP_OK on success, ESP_ERR_TIMEOUT if stop timed out
 */
esp_err_t bot_audio_stop(uint32_t timeout_ms);

/**
 * @brief Get latest audio view
 * 
 * This function is thread-safe and can be called from UI thread.
 * It copies the latest snapshot to the output structure.
 * 
 * @param ui_now Current UI timestamp in ms
 * @param out Output structure to fill
 * @return true if valid data is available, false otherwise
 */
bool bot_audio_latest(uint32_t ui_now, bot_audio_view_t *out);

/**
 * @brief Check if audio service is enabled
 * 
 * @return true if audio service is enabled, false otherwise
 */
bool bot_audio_is_enabled(void);

#ifdef __cplusplus
}
#endif