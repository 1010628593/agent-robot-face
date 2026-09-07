/* ui.c — screen manager, touch->gesture pump, router effects, SIM cycler. */
#include "bot_ui.h"
#include "bot_face.h"
#include <string.h>
#include "bsp/esp-bsp.h"
#include "lvgl.h"

void picker_build(lv_obj_t *scr);
void picker_refresh(void);
void stats_build(lv_obj_t *scr);
void stats_refresh(void);
bot_ui_model_t g_ui;
static lv_obj_t *s_screen;
static bot_gesture_t s_gesture;
static lv_indev_t *s_indev;
static bool s_was_pressed;
static uint32_t s_down_since;
#define COL_BG 0x000000
#define COL_SIM 0x83949F
static uint32_t now_ms(void) { return lv_tick_get(); }

static void sim_init(void)
{
    memset(&g_ui, 0, sizeof(g_ui));
    static const int16_t spark_codex[24] = {
        2, 4, 3, 6, 8, 5, 9, 12, 7, 10, 14, 11,
        16, 13, 18, 22, 17, 20, 25, 21, 26, 24, 28, 30
    };
    static const int16_t spark_sparse[24] = {
        -1, -1, 3, -1, 5, -1, -1, 8, -1, 4, -1, -1,
        9, -1, -1, 6, -1, 10, -1, -1, 7, -1, -1, 12
    };
    g_ui.agents[BOT_AGENT_CODEX] = (bot_sim_agent_t){
        .id = "codex", .label = "Codex", .accent = 0x66D8DF,
        .state = BOT_STATE_WORKING, .active_sessions = 1,
        .turns = 12, .total_tokens = 83412, .active_time_ms = 1800000,
        .quota = {
            { "SHORT WINDOW", false, false, 27, "RESET 2h" },
            { "LONG WINDOW", false, false, 59, "RESET 2d" },
        },
    };
    memcpy(g_ui.agents[BOT_AGENT_CODEX].spark, spark_codex, sizeof(spark_codex));
    g_ui.agents[BOT_AGENT_WORKBUDDY] = (bot_sim_agent_t){
        .id = "workbuddy", .label = "WorkBuddy", .accent = 0x7CDDA7,
        .state = BOT_STATE_UNKNOWN, .active_sessions = 0,
        .turns = 0, .total_tokens = 0, .active_time_ms = 0,
        .quota = {
            { "BALANCE", false, true, 0, "N/A" },
            { "", false, true, 0, "" },
        },
    };
    memcpy(g_ui.agents[BOT_AGENT_WORKBUDDY].spark, spark_sparse, sizeof(spark_sparse));
    g_ui.agents[BOT_AGENT_CURSOR] = (bot_sim_agent_t){
        .id = "cursor", .label = "Cursor", .accent = 0xB4A3F5,
        .state = BOT_STATE_WAITING, .active_sessions = 2,
        .turns = 7, .total_tokens = 41208, .active_time_ms = 924000,
        .quota = {
            { "MONTH BUDGET", false, false, 104, "OVER BUDGET" },
            { "", false, true, 0, "" },
        },
    };
    memcpy(g_ui.agents[BOT_AGENT_CURSOR].spark, spark_sparse, sizeof(spark_sparse));
    g_ui.agents[BOT_AGENT_HERMES] = (bot_sim_agent_t){
        .id = "hermes", .label = "Hermes", .accent = 0xF2B36C,
        .state = BOT_STATE_IDLE, .active_sessions = 0,
        .turns = 3, .total_tokens = 9021, .active_time_ms = 240000,
        .quota = {
            { "CREDITS", true, false, 0, "UNLIMITED" },
            { "", false, true, 0, "" },
        },
    };
    memcpy(g_ui.agents[BOT_AGENT_HERMES].spark, spark_sparse, sizeof(spark_sparse));
    g_ui.selected = BOT_AGENT_CODEX;
    g_ui.screen = BOT_SCR_FACE;
    g_ui.stats_tab = 0;
    g_ui.sim_cycle_ms = now_ms();
}
static const bot_state_t SIM_CYCLE[] = {
    BOT_STATE_IDLE, BOT_STATE_WORKING, BOT_STATE_TOOL, BOT_STATE_WAITING,
    BOT_STATE_DONE, BOT_STATE_ERROR, BOT_STATE_CANCELLED, BOT_STATE_UNKNOWN,
};
static void sim_tick(uint32_t now)
{
    if (now - g_ui.sim_cycle_ms < 7000) return;
    g_ui.sim_cycle_ms = now;
    bot_sim_agent_t *a = &g_ui.agents[g_ui.selected];
    int idx = 0;
    for (size_t i=0;i<sizeof(SIM_CYCLE)/sizeof(SIM_CYCLE[0]);i++) {
        if (SIM_CYCLE[i] == a->state) { idx=(int)i; break; }
    }
    a->state = SIM_CYCLE[(idx+1) % (int)(sizeof(SIM_CYCLE)/sizeof(SIM_CYCLE[0]))];
    a->transition_id++;
    if (g_ui.screen == BOT_SCR_PICKER) picker_refresh();
}
static void sim_badge(lv_obj_t *parent)
{
    lv_obj_t *sim = lv_label_create(parent);
    lv_label_set_text(sim, "SIM");
    lv_obj_set_style_text_font(sim, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(sim, lv_color_hex(COL_SIM), 0);
    lv_obj_set_size(sim, 48, 24);
    lv_obj_set_style_text_align(sim, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(sim, 209, 24);
}
static void screen_switch(bot_screen_t scr)
{
    if (g_ui.screen == BOT_SCR_FACE) face_suspend(now_ms());
    g_ui.screen = scr;
    switch (scr) {
    case BOT_SCR_FACE: bot_ui_show_face(); break;
    case BOT_SCR_PICKER: bot_ui_show_picker(); break;
    case BOT_SCR_STATS: bot_ui_show_stats(); break;
    }
}
static void apply_effect(bot_effect_t e)
{
    if (e.nav == BOT_NAV_TO_STATS) screen_switch(BOT_SCR_STATS);
    else if (e.nav == BOT_NAV_TO_FACE) screen_switch(BOT_SCR_FACE);
    else if (e.nav == BOT_NAV_TO_PICKER) {
        g_ui.picker_return = g_ui.screen;
        g_ui.picker_preview = g_ui.selected;
        g_ui.picker_selecting = false;
        screen_switch(BOT_SCR_PICKER);
    } else if (e.nav == BOT_NAV_PICKER_CANCEL) {
        g_ui.picker_selecting = false;
        screen_switch(g_ui.picker_return);
    }
    if (e.picker_delta != 0 && g_ui.screen == BOT_SCR_PICKER) {
        int p = (int)g_ui.picker_preview + e.picker_delta;
        g_ui.picker_preview = (uint8_t)((p+BOT_AGENT_COUNT) % BOT_AGENT_COUNT);
        picker_refresh();
    }
    if (e.picker_confirm && g_ui.screen == BOT_SCR_PICKER && !g_ui.picker_selecting) {
        g_ui.picker_selecting = true;
        picker_refresh();
        /* Existing SIM transaction only, not a real Bridge acknowledgment. */
        g_ui.selected = g_ui.picker_preview;
        g_ui.picker_selecting = false;
        g_ui.sim_cycle_ms = now_ms();
        screen_switch(BOT_SCR_FACE);
    }
    if (e.stats_toggle && g_ui.screen == BOT_SCR_STATS) {
        g_ui.stats_tab ^= 1;
        stats_refresh();
    }
    /* Face touches only animate eyes. No hidden command execution or approval. */
}
static bool picker_center(int16_t x, int16_t y)
{
    /* Matches the central 144x144 card in picker.c, not the whole screen. */
    return x >= 161 && x < 305 && y >= 154 && y < 298;
}
static void touch_pump(uint32_t now)
{
    if (!s_indev) return;
    lv_point_t p={0,0};
    lv_indev_get_point(s_indev, &p);
    bool pressed=lv_indev_get_state(s_indev)==LV_INDEV_STATE_PRESSED;
    bot_gesture_kind_t ev=BOT_GESTURE_NONE;
    if (pressed && !s_was_pressed) {
        ev=bot_gesture_feed(&s_gesture, BOT_TOUCH_DOWN, now, (int16_t)p.x, (int16_t)p.y);
        s_down_since=now;
    } else if (pressed && s_was_pressed) {
        bot_gesture_feed(&s_gesture, BOT_TOUCH_MOVE, now, (int16_t)p.x, (int16_t)p.y);
        ev=bot_gesture_feed(&s_gesture, BOT_TOUCH_TICK, now, (int16_t)p.x, (int16_t)p.y);
    } else if (!pressed && s_was_pressed) {
        bot_gesture_feed(&s_gesture, BOT_TOUCH_MOVE, now, (int16_t)p.x, (int16_t)p.y);
        ev=bot_gesture_feed(&s_gesture, BOT_TOUCH_UP, now, (int16_t)p.x, (int16_t)p.y);
    }
    if (g_ui.screen == BOT_SCR_FACE) {
        bool tracking=pressed && s_gesture.state==BOT_GS_PRESSED;
        float hold=tracking && !s_gesture.moved_beyond_slop ? (float)(now-s_down_since)/BOT_HOLD_MS : 0;
        face_touch(tracking, (int16_t)p.x, (int16_t)p.y, hold, now);
    }
    if (ev != BOT_GESTURE_NONE) {
        bot_effect_t effect=bot_route(g_ui.screen, ev);
        if (effect.picker_confirm &&
            (!picker_center(s_gesture.down_x, s_gesture.down_y) ||
             !picker_center((int16_t)p.x, (int16_t)p.y))) effect.picker_confirm=false;
        apply_effect(effect);
    }
    s_was_pressed=pressed;
}
void bot_ui_init(void)
{
    sim_init();
    bot_gesture_init(&s_gesture);
    s_indev=bsp_display_get_input_dev();
    s_was_pressed=false;
    s_screen=lv_obj_create(NULL);
    lv_obj_remove_style_all(s_screen);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_scr_load(s_screen);
    bot_ui_show_face();
}
void bot_ui_poll(void)
{
    uint32_t now=now_ms();
    touch_pump(now);
    sim_tick(now);
    if (g_ui.screen == BOT_SCR_FACE) face_tick(now);
}
lv_obj_t *bot_ui_screen(void) { return s_screen; }
void bot_ui_show_face(void)
{
    lv_obj_clean(s_screen);sim_badge(s_screen);face_build(s_screen);
}
void bot_ui_show_picker(void)
{
    lv_obj_clean(s_screen);sim_badge(s_screen);picker_build(s_screen);
}
void bot_ui_show_stats(void)
{
    lv_obj_clean(s_screen);sim_badge(s_screen);stats_build(s_screen);
}
