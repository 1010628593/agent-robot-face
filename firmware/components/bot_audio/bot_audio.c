#include "bot_audio.h"
#include "bot_audio_port.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <math.h>
#include <string.h>

static const char *TAG = "bot_audio";

#if CONFIG_BOT_AUDIO_ENABLE

// Audio service state
static struct {
    QueueHandle_t view_queue;
    SemaphoreHandle_t control_mutex;
    TaskHandle_t worker_task;
    bot_audio_service_state_t state;
    uint32_t stream_epoch;
    uint32_t snapshot_seq;
    bool enabled;
    bool stop_requested;
    uint32_t stop_timeout_ms;
} s_service = {0};

// Audio processing constants
#define AUDIO_BUFFER_MS         10
#define AUDIO_SAMPLE_RATE       22050
#define AUDIO_BITS_PER_SAMPLE   16
#define AUDIO_CHANNELS          1
#define AUDIO_BUFFER_SIZE       (AUDIO_SAMPLE_RATE * AUDIO_BUFFER_MS / 1000 * AUDIO_BITS_PER_SAMPLE / 8 / AUDIO_CHANNELS)
#define MAX_WORKER_TASK_SIZE    4096

// Feature extraction constants
#define RMS_WINDOW_MS           20
#define ENERGY_DECAY            0.95f
#define LOUD_THRESHOLD          0.6f
#define QUIET_THRESHOLD         0.1f
#define RHYTHM_THRESHOLD        0.4f
#define NOISE_FLOOR_ADAPT_RATE  0.01f

// Static buffers
static int16_t s_audio_buffer[AUDIO_BUFFER_SIZE];
static float s_rms_history[10] = {0}; // Last 10 RMS values
static uint32_t s_rms_history_index = 0;

// Clock function
static uint32_t clock_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

// Calculate RMS from buffer
static float calculate_rms(const int16_t *buffer, size_t samples) {
    if (samples == 0) return 0.0f;
    
    float sum_sq = 0.0f;
    for (size_t i = 0; i < samples; i++) {
        float sample = (float)buffer[i] / 32768.0f; // Normalize to [-1, 1]
        sum_sq += sample * sample;
    }
    return sqrtf(sum_sq / samples);
}

// Calculate peak from buffer
static float calculate_peak(const int16_t *buffer, size_t samples) {
    if (samples == 0) return 0.0f;
    
    int16_t max_val = 0;
    for (size_t i = 0; i < samples; i++) {
        int16_t abs_val = abs(buffer[i]);
        if (abs_val > max_val) {
            max_val = abs_val;
        }
    }
    return (float)max_val / 32768.0f;
}

// Update RMS history
static void update_rms_history(float rms) {
    s_rms_history[s_rms_history_index] = rms;
    s_rms_history_index = (s_rms_history_index + 1) % 10;
}

// Calculate noise floor (simplified)
static float calculate_noise_floor(void) {
    // Use minimum of last 10 RMS values as noise floor estimate
    float min_rms = 1.0f;
    for (int i = 0; i < 10; i++) {
        if (s_rms_history[i] < min_rms && s_rms_history[i] > 0.0f) {
            min_rms = s_rms_history[i];
        }
    }
    return min_rms;
}

// Detect audio event (simplified)
static bot_audio_event_t detect_event(float rms, float prev_rms, uint32_t now, uint32_t *last_event_ms) {
    // Sudden loud detection
    if (rms > LOUD_THRESHOLD && prev_rms < LOUD_THRESHOLD * 0.7f) {
        *last_event_ms = now;
        return BOT_AUDIO_EVENT_LOUD;
    }
    
    // Sudden quiet detection (after sustained activity)
    static bool sustained_activity = false;
    static uint32_t activity_start_ms = 0;
    
    if (rms > QUIET_THRESHOLD * 2.0f) {
        if (!sustained_activity) {
            sustained_activity = true;
            activity_start_ms = now;
        } else if (now - activity_start_ms > 1000) {
            // Sustained activity detected
        }
    } else if (sustained_activity && rms < QUIET_THRESHOLD) {
        if (now - activity_start_ms > 1000) {
            sustained_activity = false;
            *last_event_ms = now;
            return BOT_AUDIO_EVENT_QUIET;
        }
    }
    
    // Rhythm detection (simplified)
    static uint32_t last_rhythm_ms = 0;
    static uint32_t rhythm_count = 0;
    
    if (rms > RHYTHM_THRESHOLD) {
        if (now - last_rhythm_ms > 200 && now - last_rhythm_ms < 1000) {
            rhythm_count++;
            if (rhythm_count >= 3) {
                rhythm_count = 0;
                *last_event_ms = now;
                return BOT_AUDIO_EVENT_RHYTHM;
            }
        }
        last_rhythm_ms = now;
    }
    
    return BOT_AUDIO_EVENT_NONE;
}

// Worker task
static void audio_worker_task(void *arg) {
    ESP_LOGI(TAG, "Audio worker task started");
    
    // Open audio port
    esp_err_t ret = bot_audio_port_open();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open audio port: %d", ret);
        s_service.state = BOT_AUDIO_FAULT;
        vTaskDelete(NULL);
        return;
    }
    
    s_service.state = BOT_AUDIO_RUNNING;
    ESP_LOGI(TAG, "Audio service running");
    
    float prev_rms = 0.0f;
    uint32_t last_event_ms = 0;
    uint32_t last_log_ms = 0;
    
    while (!s_service.stop_requested) {
        // Read audio data
        size_t bytes_read = 0;
        ret = bot_audio_port_read(s_audio_buffer, sizeof(s_audio_buffer), &bytes_read, 100);
        
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Audio read failed: %d", ret);
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        
        uint32_t now = clock_ms();
        
        // Calculate audio features
        size_t samples = bytes_read / sizeof(int16_t);
        float rms = calculate_rms(s_audio_buffer, samples);
        float peak = calculate_peak(s_audio_buffer, samples);
        float energy = rms * rms; // Mean square energy
        
        // Update history
        update_rms_history(rms);
        
        // Calculate noise floor and SNR
        float noise_floor = calculate_noise_floor();
        float rms_dbfs = 20.0f * log10f(fmaxf(rms, 1e-6f));
        float noise_floor_dbfs = 20.0f * log10f(fmaxf(noise_floor, 1e-6f));
        float snr_db = rms_dbfs - noise_floor_dbfs;
        
        // Calculate normalized level
        float level_norm = fmaxf(0.0f, fminf(1.0f, (snr_db - 3.0f) / 24.0f));
        uint8_t level = (uint8_t)(level_norm * 10.0f);
        if (level > 10) level = 10;
        
        // Detect events
        bot_audio_event_t event = detect_event(rms, prev_rms, now, &last_event_ms);
        prev_rms = rms;
        
        // Build view
        bot_audio_view_t view = {
            .stream_epoch = s_service.stream_epoch,
            .snapshot_seq = s_service.snapshot_seq++,
            .sampled_ms = now,
            .event_id = last_event_ms,
            .event_ms = last_event_ms,
            .event_ttl_ms = 150, // 150ms TTL
            .beat_seq = 0,
            .last_onset_ms = 0,
            .rms = rms,
            .peak = peak,
            .energy = energy,
            .rms_dbfs = rms_dbfs,
            .noise_floor_dbfs = noise_floor_dbfs,
            .snr_db = snr_db,
            .level_norm = level_norm,
            .event_strength = rms,
            .period_ms = 0,
            .rhythm_confidence = 0,
            .level = level,
            .active_mics = 1, // Single microphone for now
            .quality_flags = BOT_AUDIO_QUALITY_OK,
            .event = event,
            .service_state = s_service.state,
            .available = true,
            .sustained = false,
            .rhythm_locked = false,
        };
        
        // Send to queue (overwrite old data)
        xQueueOverwrite(s_service.view_queue, &view);
        
        // Diagnostic logging
        if (now - last_log_ms >= 1000) {
            last_log_ms = now;
            ESP_LOGI(TAG, "rms=%.3f peak=%.3f level=%u event=%d snr=%.1fdB",
                     (double)rms, (double)peak, level, event, (double)snr_db);
        }
        
        // Small delay to prevent CPU hogging
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    // Stop requested
    ESP_LOGI(TAG, "Audio worker stopping...");
    s_service.state = BOT_AUDIO_STOPPING;
    
    // Close audio port
    bot_audio_port_close();
    
    s_service.state = BOT_AUDIO_DISABLED;
    ESP_LOGI(TAG, "Audio worker stopped");
    
    vTaskDelete(NULL);
}

esp_err_t bot_audio_start(void) {
    if (s_service.enabled) {
        ESP_LOGW(TAG, "Audio service already enabled");
        return ESP_OK;
    }
    
    // Initialize audio port
    bot_audio_port_config_t config = {
        .sample_rate = AUDIO_SAMPLE_RATE,
        .bits_per_sample = AUDIO_BITS_PER_SAMPLE,
        .channels = AUDIO_CHANNELS,
        .buffer_ms = AUDIO_BUFFER_MS,
    };
    
    esp_err_t ret = bot_audio_port_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize audio port: %d", ret);
        return ret;
    }
    
    // Create queue for view data
    s_service.view_queue = xQueueCreate(1, sizeof(bot_audio_view_t));
    if (s_service.view_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create view queue");
        bot_audio_port_deinit();
        return ESP_ERR_NO_MEM;
    }
    
    // Create control mutex
    s_service.control_mutex = xSemaphoreCreateMutex();
    if (s_service.control_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create control mutex");
        vQueueDelete(s_service.view_queue);
        bot_audio_port_deinit();
        return ESP_ERR_NO_MEM;
    }
    
    // Reset state
    s_service.state = BOT_AUDIO_STARTING;
    s_service.stream_epoch++;
    s_service.snapshot_seq = 0;
    s_service.stop_requested = false;
    s_service.enabled = true;
    
    // Create worker task
    BaseType_t task_ret = xTaskCreate(
        audio_worker_task,
        "bot_audio",
        MAX_WORKER_TASK_SIZE,
        NULL,
        3, // Priority
        &s_service.worker_task
    );
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create worker task");
        vSemaphoreDelete(s_service.control_mutex);
        vQueueDelete(s_service.view_queue);
        bot_audio_port_deinit();
        s_service.enabled = false;
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "Audio service started (epoch: %lu)", s_service.stream_epoch);
    return ESP_OK;
}

esp_err_t bot_audio_stop(uint32_t timeout_ms) {
    if (!s_service.enabled) {
        return ESP_OK;
    }
    
    xSemaphoreTake(s_service.control_mutex, portMAX_DELAY);
    
    if (s_service.stop_requested) {
        xSemaphoreGive(s_service.control_mutex);
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Requesting audio stop...");
    s_service.stop_requested = true;
    s_service.stop_timeout_ms = timeout_ms;
    
    // Wait for worker to stop
    uint32_t start_ms = clock_ms();
    while (s_service.state != BOT_AUDIO_DISABLED && 
           s_service.state != BOT_AUDIO_FAULT) {
        if (clock_ms() - start_ms > timeout_ms) {
            ESP_LOGW(TAG, "Audio stop timed out");
            xSemaphoreGive(s_service.control_mutex);
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    // Clean up
    if (s_service.worker_task != NULL) {
        // Task should have deleted itself
        s_service.worker_task = NULL;
    }
    
    if (s_service.view_queue != NULL) {
        vQueueDelete(s_service.view_queue);
        s_service.view_queue = NULL;
    }
    
    bot_audio_port_deinit();
    
    s_service.enabled = false;
    
    ESP_LOGI(TAG, "Audio service stopped");
    xSemaphoreGive(s_service.control_mutex);
    
    return ESP_OK;
}

bool bot_audio_latest(uint32_t ui_now, bot_audio_view_t *out) {
    if (out == NULL) {
        return false;
    }
    
    memset(out, 0, sizeof(bot_audio_view_t));
    
    if (s_service.view_queue == NULL) {
        return false;
    }
    
    if (xQueuePeek(s_service.view_queue, out, 0) != pdTRUE) {
        return false;
    }
    
    // Adjust timestamps to UI time
    uint32_t now = clock_ms();
    uint32_t age = now - out->sampled_ms;
    uint32_t event_age = now - out->event_ms;
    
    out->sampled_ms = ui_now - age;
    out->event_ms = ui_now - event_age;
    
    // Check if data is too old (200ms)
    if (age > 200) {
        out->available = false;
        return false;
    }
    
    return true;
}

bool bot_audio_is_enabled(void) {
    return s_service.enabled;
}

#else /* !CONFIG_BOT_AUDIO_ENABLE */

esp_err_t bot_audio_start(void) {
    ESP_LOGI(TAG, "Audio disabled by configuration");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bot_audio_stop(uint32_t timeout_ms) {
    return ESP_OK;
}

bool bot_audio_latest(uint32_t ui_now, bot_audio_view_t *out) {
    if (out) memset(out, 0, sizeof(*out));
    return false;
}

bool bot_audio_is_enabled(void) {
    return false;
}

#endif /* CONFIG_BOT_AUDIO_ENABLE */