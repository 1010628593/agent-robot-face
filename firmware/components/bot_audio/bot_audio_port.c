#include "bot_audio_port.h"
#include "bsp/esp-bsp.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"
#include <string.h>

static const char *TAG = "bot_audio_port";

#if CONFIG_BOT_AUDIO_ENABLE

// Audio port state
static struct {
    esp_codec_dev_handle_t codec_dev;
    bot_audio_port_config_t config;
    bot_audio_port_stats_t stats;
    SemaphoreHandle_t mutex;
    bool initialized;
    bool opened;
    bool running;
} s_port = {0};

// Buffer for audio data
#define MAX_BUFFER_SIZE 4096
static uint8_t s_audio_buffer[MAX_BUFFER_SIZE];

static uint32_t clock_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

esp_err_t bot_audio_port_init(const bot_audio_port_config_t *config) {
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (s_port.initialized) {
        ESP_LOGW(TAG, "Audio port already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Create mutex for thread safety
    s_port.mutex = xSemaphoreCreateMutex();
    if (s_port.mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }
    
    // Store configuration
    memcpy(&s_port.config, config, sizeof(bot_audio_port_config_t));
    
    // Validate configuration
    if (s_port.config.sample_rate == 0) {
        s_port.config.sample_rate = 22050; // Default
    }
    if (s_port.config.bits_per_sample == 0) {
        s_port.config.bits_per_sample = 16; // Default
    }
    if (s_port.config.channels == 0) {
        s_port.config.channels = 1; // Default mono
    }
    if (s_port.config.buffer_ms == 0) {
        s_port.config.buffer_ms = 10; // Default 10ms
    }
    
    // Calculate buffer size
    size_t buffer_size = (s_port.config.sample_rate * s_port.config.buffer_ms / 1000) *
                         (s_port.config.bits_per_sample / 8) * s_port.config.channels;
    
    if (buffer_size > MAX_BUFFER_SIZE) {
        ESP_LOGE(TAG, "Buffer too large: %d > %d", buffer_size, MAX_BUFFER_SIZE);
        vSemaphoreDelete(s_port.mutex);
        return ESP_ERR_INVALID_SIZE;
    }
    
    // Initialize statistics
    memset(&s_port.stats, 0, sizeof(bot_audio_port_stats_t));
    
    s_port.initialized = true;
    ESP_LOGI(TAG, "Audio port initialized: %d Hz, %d bits, %d ch, %d ms buffer",
             s_port.config.sample_rate, s_port.config.bits_per_sample,
             s_port.config.channels, s_port.config.buffer_ms);
    
    return ESP_OK;
}

esp_err_t bot_audio_port_open(void) {
    if (!s_port.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(s_port.mutex, portMAX_DELAY);
    
    if (s_port.opened) {
        xSemaphoreGive(s_port.mutex);
        return ESP_OK;
    }
    
    // Initialize I2C and audio codec
    esp_err_t ret = bsp_i2c_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2C: %d", ret);
        xSemaphoreGive(s_port.mutex);
        return ret;
    }
    
    // Initialize microphone codec
    s_port.codec_dev = bsp_audio_codec_microphone_init();
    if (s_port.codec_dev == NULL) {
        ESP_LOGE(TAG, "Failed to initialize microphone codec");
        xSemaphoreGive(s_port.mutex);
        return ESP_ERR_NOT_FOUND;
    }
    
    // Open codec for reading
    esp_codec_dev_sample_info_t fs = {
        .sample_rate = s_port.config.sample_rate,
        .channel = s_port.config.channels,
        .bits_per_sample = s_port.config.bits_per_sample,
    };
    
    ret = esp_codec_dev_open(s_port.codec_dev, &fs);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open codec: %d", ret);
        // Clean up codec
        esp_codec_dev_close(s_port.codec_dev);
        s_port.codec_dev = NULL;
        xSemaphoreGive(s_port.mutex);
        return ret;
    }
    
    s_port.opened = true;
    s_port.running = true;
    s_port.stats.last_read_ms = clock_ms();
    
    ESP_LOGI(TAG, "Audio port opened");
    xSemaphoreGive(s_port.mutex);
    
    return ESP_OK;
}

esp_err_t bot_audio_port_read(void *data, size_t size, size_t *bytes_read, uint32_t timeout_ms) {
    if (!s_port.initialized || !s_port.opened) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (data == NULL || bytes_read == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(s_port.mutex, portMAX_DELAY);
    
    if (!s_port.running) {
        xSemaphoreGive(s_port.mutex);
        return ESP_ERR_INVALID_STATE;
    }
    
    // Update statistics
    s_port.stats.total_reads++;
    
    // Read audio data
    int ret = esp_codec_dev_read(s_port.codec_dev, data, size);
    
    if (ret == ESP_CODEC_DEV_OK) {
        *bytes_read = size;
        s_port.stats.bytes_read += size;
        s_port.stats.last_read_ms = clock_ms();
    } else {
        *bytes_read = 0;
        s_port.stats.failed_reads++;
        s_port.stats.last_error_ms = clock_ms();
        s_port.stats.last_error = ret;
        
        ESP_LOGW(TAG, "Audio read failed: %d", ret);
    }
    
    xSemaphoreGive(s_port.mutex);
    
    return ret == ESP_CODEC_DEV_OK ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
}

esp_err_t bot_audio_port_close(void) {
    if (!s_port.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(s_port.mutex, portMAX_DELAY);
    
    if (!s_port.opened) {
        xSemaphoreGive(s_port.mutex);
        return ESP_OK;
    }
    
    s_port.running = false;
    
    // Close codec
    if (s_port.codec_dev != NULL) {
        esp_codec_dev_close(s_port.codec_dev);
        s_port.codec_dev = NULL;
    }
    
    s_port.opened = false;
    
    ESP_LOGI(TAG, "Audio port closed");
    xSemaphoreGive(s_port.mutex);
    
    return ESP_OK;
}

esp_err_t bot_audio_port_deinit(void) {
    if (!s_port.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Close port if open
    if (s_port.opened) {
        bot_audio_port_close();
    }
    
    // Clean up I2S channels
    // Note: BSP doesn't provide a deinit function, we need to manage I2S channels manually
    // This is a bit tricky because channels are shared between TX and RX
    // For now, we'll just mark as deinitialized
    
    if (s_port.mutex != NULL) {
        vSemaphoreDelete(s_port.mutex);
        s_port.mutex = NULL;
    }
    
    s_port.initialized = false;
    
    ESP_LOGI(TAG, "Audio port deinitialized");
    return ESP_OK;
}

esp_err_t bot_audio_port_get_stats(bot_audio_port_stats_t *stats) {
    if (stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!s_port.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(s_port.mutex, portMAX_DELAY);
    memcpy(stats, &s_port.stats, sizeof(bot_audio_port_stats_t));
    xSemaphoreGive(s_port.mutex);
    
    return ESP_OK;
}

bool bot_audio_port_is_open(void) {
    return s_port.opened;
}

#else /* !CONFIG_BOT_AUDIO_ENABLE */

esp_err_t bot_audio_port_init(const bot_audio_port_config_t *config) {
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bot_audio_port_open(void) {
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bot_audio_port_read(void *data, size_t size, size_t *bytes_read, uint32_t timeout_ms) {
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bot_audio_port_close(void) {
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bot_audio_port_deinit(void) {
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bot_audio_port_get_stats(bot_audio_port_stats_t *stats) {
    return ESP_ERR_NOT_SUPPORTED;
}

bool bot_audio_port_is_open(void) {
    return false;
}

#endif /* CONFIG_BOT_AUDIO_ENABLE */