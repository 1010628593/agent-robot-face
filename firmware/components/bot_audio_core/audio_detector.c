#include "bot_audio_core.h"
#include <math.h>
#include <string.h>

static float clampf(float x, float lo, float hi) { return fminf(hi, fmaxf(lo, x)); }
static float percentile(const float *values, unsigned n, float fraction)
{
    float sorted[30];
    memcpy(sorted, values, n * sizeof(float));
    for (unsigned i = 1; i < n; ++i) {
        float x = sorted[i]; unsigned j = i;
        while (j && sorted[j-1] > x) { sorted[j] = sorted[j-1]; --j; }
        sorted[j] = x;
    }
    return sorted[(unsigned)((n-1)*fraction)];
}
static void event(bot_audio_core_t *c, bot_audio_event_t kind, uint32_t now, float strength)
{
    /* LOUD retains priority over nearby lower-priority state edges. */
    if (c->view.event == BOT_AUDIO_EVENT_LOUD && kind != BOT_AUDIO_EVENT_LOUD &&
        (uint32_t)(now-c->view.event_ms) < 600u) return;
    c->view.event = kind;
    ++c->view.event_id;
    c->view.event_ms = now;
    c->view.event_ttl_ms = 150;
    c->view.event_strength = clampf(strength, 0, 1);
}
static void clear_rhythm(bot_audio_core_t *c)
{
    c->onset_count = 0;
    c->view.rhythm_locked = false;
    c->view.period_ms = 0;
    c->view.rhythm_confidence = 0;
}
void bot_audio_core_init(bot_audio_core_t *c, uint32_t epoch, const bot_audio_config_t *config)
{
    if (!c) return;
    memset(c, 0, sizeof(*c));
    c->config = config ? *config : (bot_audio_config_t){BOT_AUDIO_MODE_OFF, BOT_AUDIO_SENSITIVITY_MEDIUM};
    c->view.stream_epoch = epoch;
    c->view.event_ttl_ms = 150;
    c->view.rms_dbfs = c->view.noise_floor_dbfs = -120;
    c->view.service_state = c->config.mode == BOT_AUDIO_MODE_OFF ? BOT_AUDIO_DISABLED : BOT_AUDIO_CALIBRATING;
}
void bot_audio_core_gap(bot_audio_core_t *c, uint16_t quality)
{
    if (!c) return;
    uint32_t event_id = c->view.event_id, seq = c->view.snapshot_seq, beat_seq = c->view.beat_seq;
    bot_audio_config_t config = c->config;
    bot_audio_core_init(c, c->view.stream_epoch, &config);
    c->view.event_id = event_id;
    c->view.beat_seq = beat_seq;
    c->view.snapshot_seq = seq;
    c->pending_quality = quality | BOT_AUDIO_QUALITY_GAP;
    c->view.quality_flags = c->pending_quality;
}
const bot_audio_view_t *bot_audio_core_view(const bot_audio_core_t *c) { return c ? &c->view : NULL; }

static void onset(bot_audio_core_t *c, uint32_t now)
{
    /* An onset at least 12 dB above established beats, above -24 dBFS,
     * breaks rhythm and becomes a fresh LOUD candidate. This is digital
     * contrast only, not a claim about acoustic loudness or sound class. */
    bool exceptional = c->view.rhythm_locked && c->view.rms_dbfs > -24 &&
                       c->view.rms_dbfs > c->onset_level_db + 12;
    if (exceptional) clear_rhythm(c);
    if (!c->onset_count) c->onset_level_db = c->view.rms_dbfs;
    else c->onset_level_db += 0.2f * (c->view.rms_dbfs-c->onset_level_db);
    if (c->onset_valid && (uint32_t)(now-c->view.last_onset_ms) > 1500u) clear_rhythm(c);
    c->view.last_onset_ms = now;
    c->onset_valid = true;
    ++c->view.beat_seq;
    if (c->onset_count == 6) {
        memmove(c->onsets, c->onsets+1, 5*sizeof(uint32_t));
        c->onset_count = 5;
    }
    c->onsets[c->onset_count++] = now;
    bool was_locked = c->view.rhythm_locked;
    if (c->config.mode == BOT_AUDIO_MODE_RHYTHM && c->onset_count >= 5) {
        float intervals[5], deviations[5];
        unsigned n = c->onset_count-1, good = 0;
        for (unsigned i = 0; i < n; ++i) intervals[i] = (float)(uint32_t)(c->onsets[i+1]-c->onsets[i]);
        float period = percentile(intervals, n, 0.5f);
        for (unsigned i = 0; i < n; ++i) {
            deviations[i] = fabsf(intervals[i]-period);
            /* One hop of tolerance at the 60/180 BPM boundaries. */
            if (intervals[i] >= 323 && intervals[i] <= 1010 && deviations[i] <= period*0.15f) ++good;
        }
        float mad = percentile(deviations, n, 0.5f);
        c->view.rhythm_locked = period >= 323 && period <= 1010 && good >= n-1 && mad <= period*0.12f;
        c->view.period_ms = c->view.rhythm_locked ? period : 0;
        c->view.rhythm_confidence = c->view.rhythm_locked ? clampf(1-mad/period,0,1) : 0;
    }
    if (c->view.rhythm_locked && !was_locked) event(c, BOT_AUDIO_EVENT_RHYTHM, now, c->view.rhythm_confidence);
    else if (!c->view.rhythm_locked) event(c, BOT_AUDIO_EVENT_LOUD, now, c->view.level_norm);
}

static void background(bot_audio_core_t *c, uint32_t now, bool freeze)
{
    float db = c->view.rms_dbfs;
    if (freeze) {
        c->aggregate_count = 0;
        c->aggregate_sum = 0;
        c->aggregate_ms = now;
        c->stable_valid = false;
        return;
    }
    if (!c->aggregate_count) c->aggregate_min = c->aggregate_max = db;
    c->aggregate_sum += db;
    ++c->aggregate_count;
    c->aggregate_min = fminf(c->aggregate_min,db);
    c->aggregate_max = fmaxf(c->aggregate_max,db);
    if ((uint32_t)(now-c->aggregate_ms) < 100) return;
    float mean = c->aggregate_sum / c->aggregate_count;
    bool stable = c->aggregate_max-c->aggregate_min < 6;
    if (c->background_count) {
        unsigned last = (c->background_pos+29)%30;
        stable = stable && fabsf(mean-c->background[last]) < 3;
    }
    if (!stable) c->stable_valid = false;
    else if (!c->stable_valid) { c->stable_valid = true; c->stable_ms = now; }
    c->background[c->background_pos] = mean;
    c->background_pos = (c->background_pos+1)%30;
    if (c->background_count < 30) ++c->background_count;
    float candidate = percentile(c->background,c->background_count,0.2f);
    float dt = (uint32_t)(now-c->aggregate_ms)*0.001f;
    if (!c->calibrated) c->view.noise_floor_dbfs = candidate;
    else c->view.noise_floor_dbfs += clampf(candidate-c->view.noise_floor_dbfs,-3*dt,dt);
    c->aggregate_ms = now;
    c->aggregate_sum = 0;
    c->aggregate_count = 0;
    if (!c->calibrated && c->stable_valid && (uint32_t)(now-c->stable_ms) >= 2000) {
        c->calibrated = true;
        c->low_confidence = false;
        c->view.service_state = BOT_AUDIO_RUNNING;
    }
}

bool bot_audio_core_feed(bot_audio_core_t *c, const int16_t *pcm, size_t frames,
                         uint32_t now, bool suppress_background)
{
    if (!c) return false;
    if (!pcm || (frames != 220 && frames != 221)) {
        bot_audio_core_gap(c, BOT_AUDIO_QUALITY_GAP);
        return false;
    }
    if (c->config.mode == BOT_AUDIO_MODE_OFF) return false;
    if (c->time_valid && (uint32_t)(now-c->previous_ms) > 30u) bot_audio_core_gap(c,BOT_AUDIO_QUALITY_GAP);
    uint32_t elapsed = c->time_valid ? (uint32_t)(now-c->previous_ms) : 10;
    if (!c->time_valid) {
        c->started_ms = c->aggregate_ms = now;
        c->time_valid = true;
    }
    c->previous_ms = now;
    c->view.sampled_ms = now;
    c->view.quality_flags = c->pending_quality;
    bot_audio_features_push(c,pcm,frames);
    if (c->window_count != BOT_AUDIO_WINDOW_FRAMES) return false;
    c->pending_quality = 0;
    ++c->view.snapshot_seq;
    c->view.available = isfinite(c->view.rms) && isfinite(c->view.peak);
    if (!c->view.available) { bot_audio_core_gap(c,BOT_AUDIO_QUALITY_GAP); return false; }
    if (c->view.event != BOT_AUDIO_EVENT_NONE && (uint32_t)(now-c->view.event_ms) >= 600) c->view.event = BOT_AUDIO_EVENT_NONE;
    float db = c->view.rms_dbfs;
    float rise_from = db;
    for (unsigned i = 0; i < c->recent_count; ++i) rise_from = fminf(rise_from,c->recent_db[i]);
    c->recent_db[c->recent_pos] = db;
    c->recent_pos = (c->recent_pos+1)%4;
    if (c->recent_count < 4) ++c->recent_count;
    float threshold = c->config.sensitivity == BOT_AUDIO_SENSITIVITY_LOW ? 16 : c->config.sensitivity == BOT_AUDIO_SENSITIVITY_HIGH ? 9 : 12;
    float gate = fmaxf(c->view.noise_floor_dbfs+threshold,-55);
    bool candidate = db > gate && db-rise_from >= 6;
    bool bad = c->view.quality_flags != 0;
    background(c,now,suppress_background || bad || (c->calibrated && candidate));
    if (!c->calibrated && (uint32_t)(now-c->started_ms) >= 8000) c->low_confidence = true;
    if (c->low_confidence) c->view.quality_flags |= BOT_AUDIO_QUALITY_LOW_CONFIDENCE;
    c->view.snr_db = db-c->view.noise_floor_dbfs;
    float target = clampf((c->view.snr_db-3)/24,0,1);
    float tau = target > c->view.level_norm ? 30 : 240;
    c->view.level_norm += (1-expf(-(float)elapsed/tau))*(target-c->view.level_norm);
    c->view.level = (uint8_t)lroundf(c->view.level_norm*10);
    if (!c->calibrated || bad || suppress_background) {
        c->armed = c->below_valid = c->activity_valid = c->release_valid = false;
        c->view.sustained = false;
        clear_rhythm(c);
        return true;
    }
    if (c->view.rhythm_locked && (uint32_t)(now-c->view.last_onset_ms) > c->view.period_ms*1.5f) clear_rhythm(c);
    if (db < gate-6) {
        if (!c->below_valid) { c->below_valid = true; c->below_ms = now; }
        if ((uint32_t)(now-c->below_ms) >= 60) c->armed = true;
    } else c->below_valid = false;
    if (candidate && c->armed && (!c->onset_valid || (uint32_t)(now-c->view.last_onset_ms) >= 120)) {
        c->armed = false;
        onset(c,now);
    }
    if (!c->view.sustained) {
        if (c->view.snr_db >= 8) {
            if (!c->activity_valid) { c->activity_valid = true; c->activity_ms = now; }
            if ((uint32_t)(now-c->activity_ms) >= 1200) {
                c->view.sustained = true;
                event(c,BOT_AUDIO_EVENT_SUSTAINED,now,c->view.level_norm);
                c->activity_valid = false;
            }
        } else c->activity_valid = false;
    } else if (c->view.snr_db < 5) {
        if (!c->release_valid) { c->release_valid = true; c->release_ms = now; }
        if ((uint32_t)(now-c->release_ms) >= 500) {
            c->view.sustained = false;
            c->release_valid = false;
            event(c,BOT_AUDIO_EVENT_QUIET,now,0.3f);
        }
    } else c->release_valid = false;
    return true;
}
