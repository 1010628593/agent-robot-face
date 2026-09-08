#pragma once
#include <stddef.h>
#include "bot_audio_types.h"
#ifdef __cplusplus
extern "C" {
#endif
#define BOT_AUDIO_SAMPLE_RATE 22050u
#define BOT_AUDIO_WINDOW_FRAMES 441u
/* Single-owner bounded context. No PCM or detector history is shared globally. */
typedef struct {
    bot_audio_config_t config;
    bot_audio_view_t view;
    float window[BOT_AUDIO_WINDOW_FRAMES];
    uint8_t clipped[BOT_AUDIO_WINDOW_FRAMES];
    unsigned window_pos, window_count;
    float hp_x, hp_y;
    uint64_t sample_count;
    uint32_t started_ms, previous_ms, aggregate_ms, stable_ms;
    bool time_valid, stable_valid, calibrated, low_confidence;
    float aggregate_sum, aggregate_min, aggregate_max;
    unsigned aggregate_count;
    float background[30];
    unsigned background_pos, background_count;
    float recent_db[5];
    unsigned recent_pos, recent_count;
    bool armed, below_valid, onset_valid, activity_valid, release_valid;
    uint32_t below_ms, activity_ms, release_ms;
    uint32_t onsets[6];
    unsigned onset_count;
    float onset_level_db;
    uint16_t pending_quality;
} bot_audio_core_t;
void bot_audio_core_init(bot_audio_core_t *ctx, uint32_t epoch, const bot_audio_config_t *config);
/* Caller schedules alternating 220,221 frames at 22050 Hz; either length
 * is accepted after recovery. sampled_ms is trusted capture-window
 * end time, never merely the time an old DMA buffer was read. Returns true only
 * after the 441-frame window is populated. False also means invalid input/gap. */
bool bot_audio_core_feed(bot_audio_core_t *ctx, const int16_t *pcm, size_t frames,
                         uint32_t sampled_ms, bool suppress_background);
/* Clears PCM/filter and continuity, preserves epoch and monotonic event keys. */
void bot_audio_core_gap(bot_audio_core_t *ctx, uint16_t quality_flags);
const bot_audio_view_t *bot_audio_core_view(const bot_audio_core_t *ctx);
/* Internal pure-C feature stage; declared to permit separate translation units. */
void bot_audio_features_push(bot_audio_core_t *ctx, const int16_t *pcm, size_t frames);
#ifdef __cplusplus
}
#endif
