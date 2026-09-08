#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum { BOT_AUDIO_MODE_OFF, BOT_AUDIO_MODE_NATURAL, BOT_AUDIO_MODE_RHYTHM } bot_audio_mode_t;
typedef enum { BOT_AUDIO_SENSITIVITY_LOW, BOT_AUDIO_SENSITIVITY_MEDIUM, BOT_AUDIO_SENSITIVITY_HIGH } bot_audio_sensitivity_t;
typedef struct { bot_audio_mode_t mode; bot_audio_sensitivity_t sensitivity; } bot_audio_config_t;

/**
 * @brief Audio event types
 */
typedef enum {
    BOT_AUDIO_EVENT_NONE = 0,
    BOT_AUDIO_EVENT_LOUD,      // Sudden loud sound
    BOT_AUDIO_EVENT_QUIET,     // Sudden quiet after activity
    BOT_AUDIO_EVENT_RHYTHM,    // Rhythm detected
    BOT_AUDIO_EVENT_SUSTAINED  // Sustained sound activity
} bot_audio_event_t;

/**
 * @brief Audio service state
 */
typedef enum {
    BOT_AUDIO_DISABLED = 0,
    BOT_AUDIO_STARTING,
    BOT_AUDIO_CALIBRATING,
    BOT_AUDIO_RUNNING,
    BOT_AUDIO_STOPPING,
    BOT_AUDIO_FAULT
} bot_audio_service_state_t;

/**
 * @brief Audio quality flags
 */
typedef enum {
    BOT_AUDIO_QUALITY_OK = 0,
    BOT_AUDIO_QUALITY_CLIPPING = (1 << 0),
    BOT_AUDIO_QUALITY_GAP = (1 << 1),
    BOT_AUDIO_QUALITY_LOW_CONFIDENCE = (1 << 2),
    BOT_AUDIO_QUALITY_CHANNEL_CHANGE = (1 << 3),
    BOT_AUDIO_QUALITY_OVERFLOW = (1 << 4),
    BOT_AUDIO_QUALITY_TIMING_UNCERTAIN = (1 << 5),
    BOT_AUDIO_QUALITY_MOTION_FILTER_DEGRADED = (1 << 6)
} bot_audio_quality_flags_t;

/**
 * @brief Audio view data structure
 *
 * This structure contains audio features and events for UI consumption.
 * All fields are read-only for UI thread.
 */
typedef struct {
    uint32_t stream_epoch;      // Incremented on stream rebuild, prevents event ID reuse
    uint32_t snapshot_seq;      // Feature snapshot sequence number
    uint32_t sampled_ms;        // Reliable analysis window end time
    uint32_t event_id;          // Unique event identifier
    uint32_t event_ms;          // Onset/state edge timestamp
    uint32_t event_ttl_ms;      // New acceptance deadline
    uint32_t beat_seq;          // Beat sequence for rhythm mode
    uint32_t last_onset_ms;     // Last onset timestamp
    float rms;                  // Root mean square (0.0 - 1.0)
    float peak;                 // Peak value (0.0 - 1.0)
    float energy;               // Mean square energy
    float rms_dbfs;             // RMS in dBFS (reference: digital full scale)
    float noise_floor_dbfs;     // Noise floor in dBFS
    float snr_db;               // Signal-to-noise ratio in dB
    float level_norm;           // Normalized level (0.0 - 1.0)
    float event_strength;       // Event strength (0.0 - 1.0)
    float period_ms;            // Rhythm period in ms
    float rhythm_confidence;    // Rhythm confidence (0.0 - 1.0)
    uint8_t level;              // Visual level (0-10)
    uint8_t active_mics;        // Number of active microphones (1 or 2)
    uint16_t quality_flags;     // Quality flags (bitmask)
    bot_audio_event_t event;    // Current event type
    bot_audio_service_state_t service_state; // Service state
    int32_t last_error;         // Service/port error, zero when healthy
    bool supported;            // Hardware service compiled
    bool verified;             // G0 physical input evidence gate
    bool available;             // Data is valid
    bool sustained;             // Sustained activity detected
    bool rhythm_locked;         // Rhythm mode locked
} bot_audio_view_t;
