/* Production UI projects immutable snapshots from the single-owner bot_model.
 * Synthetic presentation is compiled only with CONFIG_BOT_DEV_SIM. */
#ifndef BOT_UI_H
#define BOT_UI_H

#include <stdbool.h>
#include <stdint.h>

#include "bot_gesture.h"
#include "bot_audio_types.h"
#include "bot_navigation.h"
#include "bot_motion.h"
#include "bot_router.h"
#include "bot_types.h"
#include "bot_model.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Per-agent face animation projection; authoritative data lives in model. */

typedef struct {
    const char *id;       /* codex/workbuddy/cursor/hermes */
    const char *label;    /* Codex / WorkBuddy / Cursor / Hermes */
    uint32_t accent;      /* theme color (agent_accents) */
    bot_state_t state;
    /* Change only for a new logical transition/run, never for a heartbeat. */
    uint32_t transition_id;
    uint8_t active_sessions;
#if defined(CONFIG_BOT_DEV_SIM) || !defined(ESP_PLATFORM)
    /* Development-only legacy fixture fields. */
    uint32_t turns;
    uint32_t total_tokens;
    uint32_t active_time_ms;
    /* sparkline: 24 buckets, -1 = uncovered (leave empty, never interpolate) */
    int16_t spark[24];
    /* quota (simulated; UI must mark SIM, never claim real balance) */
    struct {
        const char *label;
        bool unlimited;
        bool na;
        int used_pct; /* 0..100 when !unlimited && !na */
        const char *reset_label;
    } quota[2];
#endif
} bot_ui_agent_view_t;

typedef struct {
    bot_model_t model;
    bool dev_sim;
    uint32_t stats_received_ms;
    bot_ui_agent_view_t agents[BOT_AGENT_COUNT];
    uint8_t selected;      /* index into agents */
    bot_screen_t screen;
    uint8_t stats_tab;     /* production: 0=current task, 1=today, 2=quota */
    uint8_t picker_preview;
    bool picker_selecting;
    bot_screen_t picker_return;
    uint32_t sim_cycle_ms; /* last SIM state advance */
    uint32_t screen_on_ms;
} bot_ui_model_t;

extern bot_ui_model_t g_ui;

/* Called once with the LVGL lock held (after bsp_display_start). */
void bot_ui_init(void);
const char *bot_ui_agent_name(unsigned agent);
const char *bot_ui_state_name(bot_state_t state);
const char *bot_ui_health(unsigned agent);
void bot_ui_project(uint32_t now);
extern const lv_font_t bot_font_22;
extern const lv_font_t bot_font_24;
/* Called by the UI owner only; the sensor thread publishes via its mailbox. */
void bot_ui_set_motion(const bot_motion_view_t *view);
void bot_ui_set_audio(const bot_audio_view_t *view);
bool bot_ui_audio_background_suppressed(void);
bool bot_ui_take_audio_config(bot_audio_config_t *out);
/* UI owner only: settings and diagnostic projection. */
void bot_ui_audio_request(bot_audio_config_t config);
const bot_audio_config_t *bot_ui_audio_config(void);
const bot_audio_view_t *bot_ui_audio_view(void);
const char *bot_ui_audio_suppression(void);

/* Called every ~10 ms with the LVGL lock held: pumps touch samples into the
 * gesture FSM, applies router effects, drives animations and the SIM cycler. */
void bot_ui_poll(void);
void bot_ui_touch_frame(const bot_touch_frame_t *frame);
const bot_navigation_t *bot_ui_navigation(void);

/* Screen builders (internal, exposed for unit inspection) */
void bot_ui_show_face(void);
void bot_ui_show_picker(void);
void bot_ui_show_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* BOT_UI_H */
