/* ui.c — screen manager, touch->gesture pump, router effects, SIM cycler. */
#include "bot_ui.h"
#include "bot_face.h"
#include "bot_navigation.h"
#include "bot_link.h"
#ifdef ESP_PLATFORM
#include "bot_touch_input.h"
#include "esp_log.h"
#endif
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "bsp/esp-bsp.h"
#include "lvgl.h"

void picker_build(lv_obj_t *scr);
void picker_refresh(void);
void picker_animate(int direction);
void picker_press(bool pressed);
void stats_press(int tab,bool pressed);
void stats_build(lv_obj_t *scr);
void stats_refresh(void);
uint8_t stats_depth(void);void stats_reset(void);int stats_hit(int,int);void stats_tap(int);void stats_swipe(bot_gesture_kind_t);
bot_ui_model_t g_ui;
static lv_obj_t *s_screen;
static bot_gesture_t s_gesture;
static uint32_t s_touch_taps;
static lv_indev_t *s_indev;
#ifndef ESP_PLATFORM
static bool s_was_pressed;
#endif
static bot_touch_frame_t s_touch;
static bot_navigation_t s_nav;
static lv_obj_t *s_face_layer,*s_panels[2],*s_handles[2],*s_face_hints[2];
static bool s_nav_owned,s_wake_session;
static uint32_t s_picker_generation,s_request_generation,s_motion_ms;
static bool s_selection_dismiss;
static float s_handle_gain;
static int s_pressed_tab=-1;
static bool s_pressed_picker;
static void sync_navigation(uint32_t now);
static uint32_t s_down_since;
#define COL_BG 0x000000
#define COL_SIM 0x83949F
static uint32_t now_ms(void) { return lv_tick_get(); }

#ifdef CONFIG_BOT_DEV_SIM
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
    g_ui.agents[BOT_AGENT_CODEX] = (bot_ui_agent_view_t){
        .id = "codex", .label = "Codex", .accent = 0x66D8DF,
        .state = BOT_STATE_WORKING, .active_sessions = 1,
        .turns = 12, .total_tokens = 83412, .active_time_ms = 1800000,
        .quota = {
            { "SHORT WINDOW", false, false, 27, "RESET 2h" },
            { "LONG WINDOW", false, false, 59, "RESET 2d" },
        },
    };
    memcpy(g_ui.agents[BOT_AGENT_CODEX].spark, spark_codex, sizeof(spark_codex));
    g_ui.agents[BOT_AGENT_WORKBUDDY] = (bot_ui_agent_view_t){
        .id = "workbuddy", .label = "WorkBuddy", .accent = 0x7CDDA7,
        .state = BOT_STATE_UNKNOWN, .active_sessions = 0,
        .turns = 0, .total_tokens = 0, .active_time_ms = 0,
        .quota = {
            { "BALANCE", false, true, 0, "N/A" },
            { "", false, true, 0, "" },
        },
    };
    memcpy(g_ui.agents[BOT_AGENT_WORKBUDDY].spark, spark_sparse, sizeof(spark_sparse));
    g_ui.agents[BOT_AGENT_CURSOR] = (bot_ui_agent_view_t){
        .id = "cursor", .label = "Cursor", .accent = 0xB4A3F5,
        .state = BOT_STATE_WAITING, .active_sessions = 2,
        .turns = 7, .total_tokens = 41208, .active_time_ms = 924000,
        .quota = {
            { "MONTH BUDGET", false, false, 104, "OVER BUDGET" },
            { "", false, true, 0, "" },
        },
    };
    memcpy(g_ui.agents[BOT_AGENT_CURSOR].spark, spark_sparse, sizeof(spark_sparse));
    g_ui.agents[BOT_AGENT_HERMES] = (bot_ui_agent_view_t){
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
    bot_ui_agent_view_t *a = &g_ui.agents[g_ui.selected];
    int idx = 0;
    for (size_t i=0;i<sizeof(SIM_CYCLE)/sizeof(SIM_CYCLE[0]);i++) {
        if (SIM_CYCLE[i] == a->state) { idx=(int)i; break; }
    }
    a->state = SIM_CYCLE[(idx+1) % (int)(sizeof(SIM_CYCLE)/sizeof(SIM_CYCLE[0]))];
    a->transition_id++;
    if (g_ui.screen == BOT_SCR_PICKER) picker_refresh();
}
static lv_obj_t *s_sim_badge;
static void sim_badge(lv_obj_t *parent)
{
    lv_obj_t *sim = lv_label_create(parent);s_sim_badge=sim;
    lv_label_set_text(sim, "SIM");
    lv_obj_set_style_text_font(sim, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(sim, lv_color_hex(COL_SIM), 0);
    lv_obj_set_size(sim, 48, 24);
    lv_obj_set_style_text_align(sim, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(sim, 209, 24);
}
#endif
const char *bot_ui_agent_name(unsigned i) { static const char *n[]={"Codex","WorkBuddy","Cursor","Hermes"};return i<4?n[i]:"自动"; }
const char *bot_ui_state_name(bot_state_t s) {static const char *n[]={"空闲","工作中","调用工具","等待确认","已完成","发生错误","已取消","状态未知"};return s<=BOT_STATE_UNKNOWN?n[s]:"状态未知";}
const char *bot_ui_health(unsigned i) {
    if(g_ui.model.state!=BOT_MS_ONLINE)return bot_link_version_mismatch()?"请升级 Bridge 至 v3":"连接已断开";
    if(!g_ui.model.has_catalog)return "等待来源信息";
    for(unsigned k=0;k<g_ui.model.catalog.count;k++)if(g_ui.model.catalog.agents[k].id==i) {
        static const char *h[]={"来源已连接","来源数据不完整","来源不可用","来源需要登录","来源已停用"};
        return h[g_ui.model.catalog.agents[k].health];
    }
    return "来源不可用";
}
void bot_ui_project(uint32_t now) {
    (void)now;
    static char run_ids[4][BOT_RUN_ID_MAX+1];
    bot_model_t *m=&g_ui.model;g_ui.selected=m->selected_agent;
    for(unsigned i=0;i<4;i++) {
        bot_state_t s=BOT_STATE_UNKNOWN;
        if(m->state==BOT_MS_ONLINE && m->has_catalog)for(unsigned k=0;k<m->catalog.count;k++)
            if(m->catalog.agents[k].id==i && m->catalog.agents[k].health==BOT_HEALTH_READY)s=m->catalog.agents[k].state;
        if(i==m->selected_agent)s=(m->state==BOT_MS_ONLINE && m->has_focus && !m->focus.stale && m->focus.quality!=BOT_QUALITY_SIMULATED)?m->focus.state:BOT_STATE_UNKNOWN;
        bool run_changed=i==m->selected_agent && m->has_focus && strcmp(run_ids[i],m->focus.run_id);
        if(run_changed)memcpy(run_ids[i],m->focus.run_id,sizeof(run_ids[i]));
        if(g_ui.agents[i].state!=s || run_changed){g_ui.agents[i].state=s;g_ui.agents[i].transition_id++;}
    }
}
static lv_obj_t *plain_layer(lv_obj_t *parent,int x,int y,int w,int h)
{
    lv_obj_t *o=lv_obj_create(parent);lv_obj_remove_style_all(o);
    lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
    lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    return o;
}
static lv_obj_t *handle(lv_obj_t *parent,int x,int y)
{
    lv_obj_t *o=plain_layer(parent,x,y,36,4);
    lv_obj_set_style_radius(o,2,0);lv_obj_set_style_bg_color(o,lv_color_hex(BOT_OS_HANDLE_ACTIVE),0);
    lv_obj_set_style_bg_opa(o,160,0);return o;
}
static void ensure_panel(bot_screen_t card)
{
    unsigned i=card==BOT_SCR_PICKER?0:1;
    if(s_panels[i])return;
    lv_obj_t *panel=plain_layer(s_screen,BOT_OS_INSET,12,BOT_OS_CARD_SIZE,BOT_OS_CARD_SIZE);
    s_panels[i]=panel;
    lv_obj_set_style_radius(panel,BOT_OS_RADIUS,0);lv_obj_set_style_clip_corner(panel,true,0);
    lv_obj_set_style_bg_color(panel,lv_color_hex(BOT_OS_CARD_COLOR),0);
    lv_obj_set_style_bg_opa(panel,LV_OPA_COVER,0);
    lv_obj_set_style_border_width(panel,0,0);
    lv_obj_set_style_border_color(panel,lv_color_hex(BOT_OS_BORDER_COLOR),0);
    /* Existing page layouts remain in screen coordinates at the dock. */
    lv_obj_t *content=plain_layer(panel,-12,-12,466,466);
    if(card==BOT_SCR_PICKER)picker_build(content);else stats_build(content);
    s_handles[i]=handle(panel,203,card==BOT_SCR_PICKER?416:22);
    lv_obj_add_flag(panel,LV_OBJ_FLAG_HIDDEN);
}
static void prepare_panel(bot_screen_t card)
{
    if(card==BOT_SCR_PICKER) {
        ++s_picker_generation;s_selection_dismiss=false;
        g_ui.picker_preview=g_ui.dev_sim?g_ui.selected:(g_ui.model.mode==BOT_SELECTION_AUTO?4:g_ui.selected);
        g_ui.picker_selecting=g_ui.model.action_pending;g_ui.picker_return=BOT_SCR_FACE;
    } else {g_ui.stats_tab=0;if(!g_ui.dev_sim)stats_reset();}
    ensure_panel(card);
    if(card==BOT_SCR_PICKER)picker_refresh();else stats_refresh();
}
static void sync_navigation(uint32_t now)
{
    bot_navigation_tick(&s_nav,now);
    bool visible=s_nav.card!=BOT_SCR_FACE && (s_nav.phase!=BOT_NAV_FACE || s_nav.position>0);
    g_ui.screen=visible?s_nav.card:BOT_SCR_FACE;
    float dt=fminf((float)(now-s_motion_ms),100);s_motion_ms=now;
    float target=s_nav.candidate?1:0;
    s_handle_gain+=fmaxf(-dt/BOT_OS_HANDLE_MS,fminf(dt/BOT_OS_HANDLE_MS,target-s_handle_gain));
    for(unsigned i=0;i<2;i++) {
        bool active=s_nav.card==(i==0?BOT_SCR_PICKER:BOT_SCR_STATS);
        if(s_panels[i]) {
            if(visible && active) {
                if(lv_obj_has_flag(s_panels[i],LV_OBJ_FLAG_HIDDEN))lv_obj_remove_flag(s_panels[i],LV_OBJ_FLAG_HIDDEN);
                int32_t y=(int32_t)lroundf(bot_navigation_y(&s_nav));
                if(lv_obj_get_y(s_panels[i])!=y)lv_obj_set_y(s_panels[i],y);
            } else if(!lv_obj_has_flag(s_panels[i],LV_OBJ_FLAG_HIDDEN))lv_obj_add_flag(s_panels[i],LV_OBJ_FLAG_HIDDEN);
            lv_opa_t opacity=(lv_opa_t)(160+95*s_handle_gain*active);
            if(lv_obj_get_style_bg_opa(s_handles[i],0)!=opacity)lv_obj_set_style_bg_opa(s_handles[i],opacity,0);
        }
        if(s_face_hints[i]) {
            lv_opa_t opacity=(lv_opa_t)(!visible && active?180*s_handle_gain:0);
            if(lv_obj_get_style_bg_opa(s_face_hints[i],0)!=opacity)lv_obj_set_style_bg_opa(s_face_hints[i],opacity,0);
        }
    }
}
static void screen_switch(bot_screen_t scr)
{
    if(scr==BOT_SCR_FACE)bot_navigation_dismiss(&s_nav,now_ms());
    /* Edge navigation is the only route that opens a card. */
    sync_navigation(now_ms());
}
static void apply_effect(bot_effect_t e)
{
    if (e.picker_delta != 0 && g_ui.screen == BOT_SCR_PICKER) {
        int p = (int)g_ui.picker_preview + e.picker_delta;
        int count=g_ui.dev_sim?BOT_AGENT_COUNT:BOT_AGENT_COUNT+1;
        g_ui.picker_preview = (uint8_t)((p+count) % count);
        picker_refresh();picker_animate(e.picker_delta);
    }
    if (e.picker_confirm && g_ui.screen == BOT_SCR_PICKER && !g_ui.picker_selecting && !g_ui.model.action_pending) {
        g_ui.picker_selecting = true;s_selection_dismiss=true;s_request_generation=s_picker_generation;
        picker_refresh();
#ifdef CONFIG_BOT_DEV_SIM
        if(g_ui.dev_sim) {
            g_ui.selected=g_ui.picker_preview;g_ui.picker_selecting=false;
            g_ui.sim_cycle_ms=now_ms();screen_switch(BOT_SCR_FACE);
        } else
#endif
        {
            bool automatic=g_ui.picker_preview==4;
            if(!bot_link_select(&g_ui.model,automatic?BOT_SELECTION_AUTO:BOT_SELECTION_PINNED,
                automatic?g_ui.model.selected_agent:(bot_agent_id_t)g_ui.picker_preview,now_ms()))g_ui.picker_selecting=false;
            picker_refresh();
        }
    }
    /* Face touches never execute task actions. */
}
static bool picker_center(int16_t x, int16_t y)
{
    return x >= 161 && x < 305 && y >= 154 && y < 298;
}
static int stats_tab_at(int16_t x,int16_t y)
{
    if(!g_ui.dev_sim)return stats_hit(x,y);
    if(y<99 || y>=135)return -1;
    if(g_ui.dev_sim)return x>=93 && x<373?(x<233?0:1):-1;
    if(x<71 || x>=395)return -1;
    return (x-71)/108;
}
/* Panel-only tap tolerance. This never changes face/pet recognition. The
 * maximum excursion is latched, and a replaced/multiple/cancelled contact
 * stays ineligible until UP. No hold or long-press navigation is introduced. */
static int stats_forgiving_tap(const bot_touch_frame_t *f)
{
    static bool session,eligible;
    static uint8_t id;
    static int16_t x,y,last_x,last_y;
    static uint32_t started;
    static int hit;
    if(!session && f->count && !g_ui.dev_sim && g_ui.screen==BOT_SCR_STATS && bot_navigation_content_enabled(&s_nav)) {
        session=true;eligible=f->count==1&&!f->cancelled;id=f->points[0].id;
        x=last_x=f->points[0].x;y=last_y=f->points[0].y;started=f->time_ms;hit=stats_hit(x,y);
        if(hit<0)eligible=false;
    }
    if(!session)return -1;
    if(f->cancelled||f->count>1||(f->count==1&&f->points[0].id!=id))eligible=false;
    if(f->count==1 || (!f->count&&f->final_position)) {
        last_x=f->points[0].x;last_y=f->points[0].y;
        if(last_x-x>18||x-last_x>18||last_y-y>18||y-last_y>18)eligible=false;
    }
    if(!f->count) {
        int accepted=eligible&&f->time_ms-started<=500&&stats_hit(last_x,last_y)==hit?hit:-1;
        session=false;return accepted;
    }
    return -1;
}
/* Complete-frame input is also usable by offline integration/replay tools. */
void bot_ui_touch_frame(const bot_touch_frame_t *raw)
{
    bot_touch_frame_t f=*raw;
    if(f.count>2){f.count=0;f.cancelled=true;}
    /* Edge arbitration always sees physical display coordinates, before
     * the expression layer applies its frozen inverse rotation. */
    if(s_gesture.wake_contact && f.count && !s_gesture.frame_active)s_wake_session=true;
    if(s_wake_session) {
        bot_gesture_feed_frame(&s_gesture,&f);
        if(!f.count && !f.cancelled)s_wake_session=false;
        return;
    }
    int forgiven_hit=stats_forgiving_tap(&f);
    /* Nested usage pages own the complete contact before the global panel
     * recognizer. A downward drag pops once on UP; multi/replaced IDs cancel. */
    static bool nested_session;
    if(!g_ui.dev_sim && !nested_session && f.count && !s_nav.session &&
       g_ui.screen==BOT_SCR_STATS && stats_depth()>0 && bot_navigation_content_enabled(&s_nav)) {
        nested_session=true;bot_gesture_set_face_mode(&s_gesture,false);
        s_pressed_tab=f.count==1?stats_hit(f.points[0].x,f.points[0].y):-1;
        if(s_pressed_tab>=0)stats_press(s_pressed_tab,true);
    }
    if(nested_session) {
        bot_gesture_kind_t event=bot_gesture_feed_frame(&s_gesture,&f);
        if(event==BOT_GESTURE_NONE&&forgiven_hit>=0)event=BOT_GESTURE_TAP;
        if(!f.count||f.count>1||f.cancelled||s_gesture.moved_beyond_slop){if(s_pressed_tab>=0)stats_press(s_pressed_tab,false);s_pressed_tab=-1;}
        memset(&s_touch,0,sizeof(s_touch));
        if(!f.count){
            if(event==BOT_GESTURE_TAP){int a=stats_hit(s_gesture.down_x,s_gesture.down_y),b=stats_hit(s_gesture.last_x,s_gesture.last_y);if(a>=0&&a==b)stats_tap(a);}
            else stats_swipe(event);
            nested_session=false;
        }return;
    }
    bool fresh=!s_nav.session;
    bool owned=bot_navigation_feed(&s_nav,&f);
    if(fresh && s_nav.candidate && s_nav.phase==BOT_NAV_FACE)prepare_panel(s_nav.card);
    if(s_nav.candidate && s_nav.card==BOT_SCR_PICKER && !s_nav.opening)s_selection_dismiss=false;
    sync_navigation(f.time_ms);
    if(owned) {
        if(!s_nav_owned)face_suspend(f.time_ms);
        s_nav_owned=f.count>0;
        if(s_pressed_picker)picker_press(false);
        if(s_pressed_tab>=0)stats_press(s_pressed_tab,false);
        s_pressed_picker=false;s_pressed_tab=-1;
        bot_touch_frame_t cancel=f;cancel.cancelled=f.count>0 || f.cancelled;
        bot_gesture_feed_frame(&s_gesture,&cancel);
        memset(&s_touch,0,sizeof(s_touch));return;
    }
    s_nav_owned=false;
    if(s_nav.phase!=BOT_NAV_FACE && !bot_navigation_content_enabled(&s_nav)) {
        bot_touch_frame_t cancel=f;cancel.cancelled=f.count>0 || f.cancelled;
        bot_gesture_feed_frame(&s_gesture,&cancel);return;
    }
    if(g_ui.screen==BOT_SCR_FACE) {
        for(unsigned i=0;i<f.count;i++)
            face_map_input(true,f.points[i].x,f.points[i].y,&f.points[i].x,&f.points[i].y);
        if(!f.count) {
            int16_t x=f.final_position?f.points[0].x:233,y=f.final_position?f.points[0].y:233;
            face_map_input(false,x,y,&f.points[0].x,&f.points[0].y);
        }
    }
    if(f.count && !s_gesture.frame_active) {
        bot_gesture_set_face_mode(&s_gesture,g_ui.screen==BOT_SCR_FACE);
        s_down_since=f.time_ms;
        if(f.count==1 && bot_navigation_content_enabled(&s_nav)) {
            s_pressed_picker=g_ui.screen==BOT_SCR_PICKER && picker_center(f.points[0].x,f.points[0].y);
            if(s_pressed_picker)picker_press(true);
            s_pressed_tab=g_ui.screen==BOT_SCR_STATS?stats_tab_at(f.points[0].x,f.points[0].y):-1;
            if(s_pressed_tab>=0)stats_press(s_pressed_tab,true);
        }
    }
    bot_gesture_kind_t ev=bot_gesture_feed_frame(&s_gesture,&f);
    if(ev==BOT_GESTURE_NONE&&forgiven_hit>=0)ev=BOT_GESTURE_TAP;
    s_touch=f;
    if(ev==BOT_GESTURE_TAP)s_touch_taps++;
    if(!f.count || f.count>1 || f.cancelled || s_gesture.moved_beyond_slop) {
        if(s_pressed_picker)picker_press(false);
        if(s_pressed_tab>=0)stats_press(s_pressed_tab,false);
        s_pressed_picker=false;s_pressed_tab=-1;
    }
    if(g_ui.screen==BOT_SCR_FACE && s_gesture.pet_contact) {
        if(ev==BOT_GESTURE_TAP)face_poke(s_gesture.down_x,s_gesture.down_y,f.time_ms);
        if(ev==BOT_GESTURE_TAP || ev==BOT_GESTURE_STROKE)ev=BOT_GESTURE_NONE;
    }
    if(ev==BOT_GESTURE_TAP && g_ui.screen==BOT_SCR_STATS) {
        int down=stats_tab_at(s_gesture.down_x,s_gesture.down_y);
        int up=stats_tab_at(s_gesture.last_x,s_gesture.last_y);
        if(down>=0 && down==up) {
            if(g_ui.dev_sim){g_ui.stats_tab=(uint8_t)down;stats_refresh();}else stats_tap(down);
        }
        ev=BOT_GESTURE_NONE;
    }
    if(!g_ui.dev_sim && g_ui.screen==BOT_SCR_STATS && ev==BOT_GESTURE_SWIPE_DOWN){bot_navigation_dismiss(&s_nav,f.time_ms);ev=BOT_GESTURE_NONE;}
    if(!g_ui.dev_sim && g_ui.screen==BOT_SCR_STATS && (ev==BOT_GESTURE_SWIPE_LEFT || ev==BOT_GESTURE_SWIPE_RIGHT)){stats_swipe(ev);ev=BOT_GESTURE_NONE;}
    if(ev!=BOT_GESTURE_NONE && bot_navigation_content_enabled(&s_nav)) {
        bot_effect_t effect=bot_route(g_ui.screen,ev);
        if(effect.picker_confirm &&
           (!picker_center(s_gesture.down_x,s_gesture.down_y) ||
            !picker_center(s_gesture.last_x,s_gesture.last_y)))effect.picker_confirm=false;
        apply_effect(effect);
    }
}
static void touch_pump(uint32_t now)
{
    if(!s_indev)return;
#ifdef ESP_PLATFORM
    bot_touch_frame_t frame;
    while(bot_touch_input_pop(&frame))bot_ui_touch_frame(&frame);
#else
    /* Desktop pointer compatibility; production always uses complete frames. */
    lv_point_t p={0,0};lv_indev_get_point(s_indev,&p);
    bool pressed=lv_indev_get_state(s_indev)==LV_INDEV_STATE_PRESSED;
    if(pressed || s_was_pressed) {
        bot_touch_frame_t frame={.time_ms=now,.count=pressed?1:0,.final_position=!pressed,
            .points={{.id=0,.x=p.x,.y=p.y}}};
        bot_ui_touch_frame(&frame);
    }
    s_was_pressed=pressed;
#endif
    if(g_ui.screen==BOT_SCR_FACE) {
        bot_touch_frame_t feedback=s_touch;feedback.time_ms=now;
        bool tracking=s_gesture.pet_contact && !s_gesture.blocked && s_nav.phase==BOT_NAV_FACE && !s_nav.owned &&
                      s_gesture.state==BOT_GS_PRESSED;
        if(!tracking)feedback.count=0;
        float comfort=0;
        if(tracking && s_gesture.stroking)
            comfort=fminf(1,(float)(now-s_gesture.stroke_ms)/1500);
        face_interact(&feedback,comfort,now-s_down_since);
    }
}
void bot_ui_init(void)
{
    memset(&g_ui,0,sizeof(g_ui));
#ifdef CONFIG_BOT_DEV_SIM
    sim_init();g_ui.dev_sim=true;
#else
    bot_model_init(&g_ui.model);bot_link_start();
    for(unsigned i=0;i<4;i++){g_ui.agents[i].label=bot_ui_agent_name(i);g_ui.agents[i].state=BOT_STATE_UNKNOWN;}
#endif
    bot_gesture_init(&s_gesture);
    s_indev=bsp_display_get_input_dev();
#ifdef ESP_PLATFORM
    if(!bot_touch_input_start(s_indev)) {
        ESP_LOGE("bot_ui","two-point input unavailable; touch disabled");
        if(s_indev)lv_indev_enable(s_indev,false);
        s_indev=NULL;
    }
#else
    s_was_pressed=false;
#endif
    bot_navigation_init(&s_nav);memset(&s_touch,0,sizeof(s_touch));
    memset(s_panels,0,sizeof(s_panels));memset(s_handles,0,sizeof(s_handles));
    s_face_layer=NULL;s_nav_owned=s_wake_session=false;s_handle_gain=0;
    s_motion_ms=now_ms();s_pressed_picker=false;s_pressed_tab=-1;
    s_screen=lv_obj_create(NULL);
    lv_obj_remove_style_all(s_screen);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_scr_load(s_screen);
    bot_ui_show_face();
}
void bot_ui_set_motion(const bot_motion_view_t *view) { face_set_motion(view); }
void bot_ui_poll(void)
{
    uint32_t now=now_ms();
#ifdef CONFIG_BOT_DEV_SIM
    if(g_ui.dev_sim)sim_tick(now);
#endif
    if(!g_ui.dev_sim) {
        bool pending=g_ui.model.action_pending;
        uint64_t stamp=g_ui.model.stats.sent_at_ms;
        bool changed=bot_link_poll(&g_ui.model,now);bot_ui_project(now);
        if(g_ui.model.has_stats && stamp!=g_ui.model.stats.sent_at_ms)g_ui.stats_received_ms=now;
        if(pending && !g_ui.model.action_pending) {
            g_ui.picker_selecting=false;
            if(s_selection_dismiss && s_request_generation==s_picker_generation && s_nav.card==BOT_SCR_PICKER && !g_ui.model.action_rejected && g_ui.model.state==BOT_MS_ONLINE)screen_switch(BOT_SCR_FACE);
        }
        if(changed && g_ui.screen==BOT_SCR_PICKER)picker_refresh();
        static uint32_t last_stats_refresh;
        if(g_ui.screen==BOT_SCR_STATS && (changed || now-last_stats_refresh>=1000)){stats_refresh();last_stats_refresh=now;}
    }
    face_sync(now);
    touch_pump(now);sync_navigation(now);
    if(bot_navigation_progress(&s_nav)<1)face_tick(now);
#if defined(ESP_PLATFORM) && defined(CONFIG_BOT_IMU_DIAGNOSTICS)
    static uint32_t last_touch_diagnostic;
    if(now-last_touch_diagnostic>=500) {
        last_touch_diagnostic=now;
        bot_touch_input_debug_t d;bot_touch_input_debug(&d);
        bot_face_touch_debug_t f;face_touch_debug(&f,now);
        char line[640];
        snprintf(line,sizeof(line),"@touch {\"reads\":%lu,\"dual\":%lu,\"errors\":%lu,\"overflow\":%lu,\"raw_count\":%u,\"wire_count\":%u,\"record0\":%u,\"record1\":%u,\"count\":%u,\"gs\":%u,\"pet\":%u,\"blocked\":%u,\"edge\":%u,\"x\":%d,\"y\":%d,\"taps\":%lu,\"expression\":%d,\"motion_count\":%d,\"tapped\":%d,\"tx\":%.3f,\"ty\":%.3f,\"left\":%.2f,\"right\":%.2f,\"revision\":%lu}\n",
            (unsigned long)d.reads,(unsigned long)d.dual,(unsigned long)d.errors,(unsigned long)d.overflows,
            d.count,d.wire_count,d.record0,d.record1,s_touch.count,s_gesture.state,s_gesture.pet_contact,
            s_gesture.blocked,s_gesture.edge,s_gesture.last_x,s_gesture.last_y,(unsigned long)s_touch_taps,
            f.expression,f.count,f.tapped,f.x,f.y,f.left,f.right,(unsigned long)f.revision);
        bot_link_trace(line);
    }
#endif
}
lv_obj_t *bot_ui_screen(void) { return s_screen; }
const bot_navigation_t *bot_ui_navigation(void){return &s_nav;}
void bot_ui_show_face(void)
{
    if(!s_face_layer) {
        s_face_layer=plain_layer(s_screen,0,0,466,466);face_build(s_face_layer);
#ifdef CONFIG_BOT_DEV_SIM
        if(g_ui.dev_sim)sim_badge(s_face_layer);
#endif
        s_face_hints[0]=handle(s_face_layer,215,34);s_face_hints[1]=handle(s_face_layer,215,428);
    }
#ifdef CONFIG_BOT_DEV_SIM
    if(s_sim_badge) {
        if(g_ui.dev_sim)lv_obj_remove_flag(s_sim_badge,LV_OBJ_FLAG_HIDDEN);
        else {lv_obj_delete(s_sim_badge);s_sim_badge=NULL;}
    }
#endif
    bot_navigation_show(&s_nav,BOT_SCR_FACE);sync_navigation(now_ms());
}
void bot_ui_show_picker(void)
{
    ensure_panel(BOT_SCR_PICKER);picker_refresh();
    bot_navigation_show(&s_nav,BOT_SCR_PICKER);sync_navigation(now_ms());
}
void bot_ui_show_stats(void)
{
    ensure_panel(BOT_SCR_STATS);stats_refresh();
    bot_navigation_show(&s_nav,BOT_SCR_STATS);sync_navigation(now_ms());
}
