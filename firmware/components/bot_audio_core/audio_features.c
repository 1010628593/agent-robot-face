#include "bot_audio_core.h"
#include <math.h>

void bot_audio_features_push(bot_audio_core_t *c, const int16_t *pcm, size_t frames)
{
    /* 80 Hz one-pole high-pass, actual fixed sample rate. */
    const float a = 0.9777119f; /* 1 / (1 + 2*pi*80/22050) */
    for (size_t i = 0; i < frames; ++i) {
        int32_t raw = pcm[i];
        int32_t magnitude = raw < 0 ? -raw : raw;
        float x = (float)raw / 32768.0f;
        float y = a * (c->hp_y + x - c->hp_x);
        c->hp_x = x;
        c->hp_y = y;
        c->window[c->window_pos] = y;
        c->clipped[c->window_pos] = magnitude >= 32760;
        c->window_pos = (c->window_pos + 1u) % BOT_AUDIO_WINDOW_FRAMES;
        if (c->window_count < BOT_AUDIO_WINDOW_FRAMES) ++c->window_count;
    }
    c->sample_count += frames;
    if (c->window_count != BOT_AUDIO_WINDOW_FRAMES) return;
    float energy = 0, peak = 0;
    unsigned clipping = 0;
    for (unsigned i = 0; i < BOT_AUDIO_WINDOW_FRAMES; ++i) {
        float x = c->window[i];
        energy += x*x;
        if (fabsf(x) > peak) peak = fabsf(x);
        clipping += c->clipped[i];
    }
    c->view.energy = energy / BOT_AUDIO_WINDOW_FRAMES;
    c->view.rms = sqrtf(c->view.energy);
    c->view.peak = peak;
    c->view.rms_dbfs = 20.0f * log10f(fmaxf(c->view.rms, 1e-6f));
    /* A single rail sample still flags quality conservatively. */
    if (clipping) c->view.quality_flags |= BOT_AUDIO_QUALITY_CLIPPING;
}
