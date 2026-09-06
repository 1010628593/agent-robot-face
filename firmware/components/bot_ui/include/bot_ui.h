/* bot_ui.h — three-screen UI for Bot Status (T06–T08).
 *
 * Screens (02_UI_UX): FACE / AGENT_PICKER / STATS(usage|quota).
 * All coordinates from design/ui_tokens.json; colors from the same file.
 *
 * SIM MODE: this build has no USB bridge yet (T10+). All data comes from the
 * built-in simulator and is visibly badged "SIM" on every screen — simulated
 * state must never be presented as real agent integration (project rule).
 */
#ifndef BOT_UI_H
#define BOT_UI_H

#include <stdbool.h>
#include <stdint.h>

#include "bot_gesture.h"
#include "bot_router.h"
#include "bot_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- simulated host model (SIM data, quality=simulated) ----------------- */

typedef struct {
    const char *id;       /* codex/workbuddy/cursor/hermes */
    const char *label;    /* Codex / WorkBuddy / Cursor / Hermes */
    uint32_t accent;      /* theme color (agent_accents) */
    bot_state_t state;
    uint8_t active_sessions;
    /* usage (simulated) */
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
} bot_sim_agent_t;

typedef struct {
    bot_sim_agent_t agents[BOT_AGENT_COUNT];
    uint8_t selected;      /* index into agents */
    bot_screen_t screen;
    uint8_t stats_tab;     /* 0=usage 1=quota */
    uint8_t picker_preview;
    bool picker_selecting;
    bot_screen_t picker_return;
    uint32_t sim_cycle_ms; /* last SIM state advance */
    uint32_t screen_on_ms;
} bot_ui_model_t;

extern bot_ui_model_t g_ui;

/* Called once with the LVGL lock held (after bsp_display_start). */
void bot_ui_init(void);

/* Called every ~10 ms with the LVGL lock held: pumps touch samples into the
 * gesture FSM, applies router effects, drives animations and the SIM cycler. */
void bot_ui_poll(void);

/* Screen builders (internal, exposed for unit inspection) */
void bot_ui_show_face(void);
void bot_ui_show_picker(void);
void bot_ui_show_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* BOT_UI_H */
