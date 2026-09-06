/* face.c — FACE screen (02_UI_UX §2 + design/ui_tokens.json).
 *
 * Layout: agent title (113,57,240,32) + accent dot (101,73,r4);
 * ring r=199 stroke 3 state-colored; eyes 66x92 at centers (178,225)/(288,225);
 * state label (91,322); aux label (109,358); hint (153,409) HOLD TO SWITCH.
 * Expressions (§2 表情参数初值) — SIM cycler drives all seven states.
 */
#include "bot_ui.h"

#include <stdlib.h>

#include "lvgl.h"

#define COL_EYES 0xDDEAF2
#define COL_SECONDARY 0x83949F
#define COL_WORKING 0x5A9BFF
#define COL_TOOL 0xAE8CFF
#define COL_WAITING 0xFFBE55
#define COL_DONE 0x6DE1A3
#define COL_ERROR 0xFF707C
#define COL_UNKNOWN 0x647680

static lv_obj_t *s_title;
static lv_obj_t *s_dot;
static lv_obj_t *s_ring;
static lv_obj_t *s_arc; /* working activity arc */
static lv_obj_t *s_eye_l;
static lv_obj_t *s_eye_r;
static lv_obj_t *s_mark; /* waiting "?" / offline mark */
static lv_obj_t *s_state;
static lv_obj_t *s_aux;
static lv_obj_t *s_hint;

static uint32_t s_next_blink;
static uint32_t s_blink_end;
static uint32_t s_next_gaze;
static int s_gaze_dx;
static int s_gaze_dy;
static uint32_t s_state_entered;

static uint32_t state_color(bot_state_t st)
{
    switch (st) {
    case BOT_STATE_WORKING: return COL_WORKING;
    case BOT_STATE_TOOL: return COL_TOOL;
    case BOT_STATE_WAITING: return COL_WAITING;
    case BOT_STATE_DONE: return COL_DONE;
    case BOT_STATE_ERROR: return COL_ERROR;
    case BOT_STATE_UNKNOWN: return COL_UNKNOWN;
    default: return COL_SECONDARY; /* idle / cancelled */
    }
}

static const char *state_text(bot_state_t st)
{
    switch (st) {
    case BOT_STATE_IDLE: return "IDLE";
    case BOT_STATE_WORKING: return "WORKING";
    case BOT_STATE_TOOL: return "TOOL";
    case BOT_STATE_WAITING: return "WAITING";
    case BOT_STATE_DONE: return "DONE";
    case BOT_STATE_ERROR: return "ERROR";
    case BOT_STATE_CANCELLED: return "CANCELLED";
    case BOT_STATE_UNKNOWN: return "UNKNOWN";
    default: return "?";
    }
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *txt, int x, int y,
                            int w, int h, const lv_font_t *font, uint32_t color,
                            lv_text_align_t align)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_size(l, w, h);
    lv_obj_set_pos(l, x, y);
    lv_obj_set_style_text_align(l, align, 0);
    return l;
}

static lv_obj_t *make_eye(lv_obj_t *parent, int cx, int cy)
{
    lv_obj_t *e = lv_obj_create(parent);
    lv_obj_set_size(e, 66, 92);
    lv_obj_set_pos(e, cx - 33, cy - 46);
    lv_obj_set_style_radius(e, 30, 0);
    lv_obj_set_style_bg_color(e, lv_color_hex(COL_EYES), 0);
    lv_obj_set_style_bg_opa(e, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(e, 0, 0);
    lv_obj_remove_flag(e, LV_OBJ_FLAG_SCROLLABLE);
    return e;
}

void face_build(lv_obj_t *scr)
{
    const bot_sim_agent_t *a = &g_ui.agents[g_ui.selected];
    uint32_t col = state_color(a->state);

    s_dot = lv_obj_create(scr);
    lv_obj_set_size(s_dot, 8, 8);
    lv_obj_set_pos(s_dot, 101 - 4, 73 - 4);
    lv_obj_set_style_radius(s_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_dot, lv_color_hex(a->accent), 0);
    lv_obj_set_style_bg_opa(s_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_dot, 0, 0);

    s_title = make_label(scr, a->label, 113, 57, 240, 32,
                         &lv_font_montserrat_24, a->accent, LV_TEXT_ALIGN_LEFT);

    /* state ring r=199, stroke 3 */
    s_ring = lv_arc_create(scr);
    lv_obj_set_size(s_ring, 398, 398);
    lv_obj_center(s_ring);
    lv_arc_set_range(s_ring, 0, 360);
    lv_arc_set_value(s_ring, 360);
    lv_arc_set_bg_angles(s_ring, 0, 360);
    lv_obj_set_style_arc_width(s_ring, 3, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_ring, 3, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_ring, lv_color_hex(col), LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_ring, lv_color_hex(col), LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(s_ring, LV_OPA_40, LV_PART_MAIN);
    lv_obj_remove_style(s_ring, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(s_ring, LV_OBJ_FLAG_CLICKABLE);

    /* working activity arc: rotating segment (2.4s cycle, not progress) */
    s_arc = lv_arc_create(scr);
    lv_obj_set_size(s_arc, 398, 398);
    lv_obj_center(s_arc);
    lv_arc_set_range(s_arc, 0, 360);
    lv_arc_set_bg_angles(s_arc, 0, 360);
    lv_arc_set_angles(s_arc, 0, 60);
    lv_obj_set_style_arc_width(s_arc, 3, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_arc, 3, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(s_arc, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc, lv_color_hex(col), LV_PART_INDICATOR);
    lv_obj_remove_style(s_arc, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(s_arc, LV_OBJ_FLAG_CLICKABLE);

    s_eye_l = make_eye(scr, 178, 225);
    s_eye_r = make_eye(scr, 288, 225);

    s_mark = make_label(scr, "", 208, 180, 50, 60,
                        &lv_font_montserrat_48, col, LV_TEXT_ALIGN_CENTER);

    s_state = make_label(scr, state_text(a->state), 91, 322, 284, 32,
                         &lv_font_montserrat_24, col, LV_TEXT_ALIGN_CENTER);

    char aux[48];
    if (a->state == BOT_STATE_UNKNOWN) {
        lv_snprintf(aux, sizeof(aux), "SOURCE MISSING");
    } else if (a->active_sessions > 0) {
        lv_snprintf(aux, sizeof(aux), "%u ACTIVE", (unsigned)a->active_sessions);
    } else {
        lv_snprintf(aux, sizeof(aux), "NO ACTIVE RUN");
    }
    s_aux = make_label(scr, aux, 109, 358, 248, 24,
                       &lv_font_montserrat_20, COL_SECONDARY, LV_TEXT_ALIGN_CENTER);

    s_hint = make_label(scr, "HOLD TO SWITCH", 153, 409, 160, 24,
                        &lv_font_montserrat_20, COL_SECONDARY, LV_TEXT_ALIGN_CENTER);

    /* expression initial shape */
    switch (a->state) {
    case BOT_STATE_WORKING:
        lv_obj_set_height(s_eye_l, 76); /* slightly narrowed */
        lv_obj_set_height(s_eye_r, 76);
        break;
    case BOT_STATE_TOOL:
        lv_obj_set_height(s_eye_l, 60); /* focused narrow */
        lv_obj_set_height(s_eye_r, 60);
        break;
    case BOT_STATE_WAITING:
        lv_obj_set_height(s_eye_l, 100); /* slightly wider open */
        lv_obj_set_height(s_eye_r, 100);
        lv_label_set_text(s_mark, "?");
        break;
    case BOT_STATE_DONE:
        lv_obj_set_height(s_eye_l, 34); /* smiling closed eyes */
        lv_obj_set_height(s_eye_r, 34);
        break;
    case BOT_STATE_ERROR:
        lv_obj_set_height(s_eye_l, 40); /* narrowed */
        lv_obj_set_height(s_eye_r, 40);
        lv_obj_set_style_transform_rotation(s_eye_l, 450, 0); /* X eyes */
        lv_obj_set_style_transform_rotation(s_eye_r, -450, 0);
        break;
    case BOT_STATE_CANCELLED:
        lv_obj_set_height(s_eye_l, 46); /* relaxed half-closed */
        lv_obj_set_height(s_eye_r, 46);
        break;
    case BOT_STATE_UNKNOWN:
        lv_obj_set_height(s_eye_l, 56);
        lv_obj_set_height(s_eye_r, 56);
        lv_label_set_text(s_mark, "?");
        break;
    default:
        break; /* idle: neutral */
    }

    s_state_entered = (uint32_t)(lv_tick_get());
    s_next_blink = s_state_entered + 2800 + (uint32_t)(rand() % 3700);
    s_blink_end = 0;
    s_next_gaze = s_state_entered + 3000;
    s_gaze_dx = 0;
    s_gaze_dy = 0;
}

void face_tick(uint32_t now)
{
    const bot_sim_agent_t *a = &g_ui.agents[g_ui.selected];

    /* idle blink: 2.8-6.5s random, 120ms closed (design animation tokens) */
    if (s_blink_end == 0 && now >= s_next_blink && a->state == BOT_STATE_IDLE) {
        s_blink_end = now + 120;
        lv_obj_set_height(s_eye_l, 8);
        lv_obj_set_height(s_eye_r, 8);
        s_next_blink = now + 2800 + (uint32_t)(rand() % 3700);
    }
    if (s_blink_end != 0 && now >= s_blink_end) {
        s_blink_end = 0;
        lv_obj_set_height(s_eye_l, 92);
        lv_obj_set_height(s_eye_r, 92);
    }

    /* gaze wander: +/-9px every 3-6s while idle */
    if (a->state == BOT_STATE_IDLE && now >= s_next_gaze) {
        s_next_gaze = now + 3000 + (uint32_t)(rand() % 3000);
        s_gaze_dx = (rand() % 19) - 9;
        s_gaze_dy = (rand() % 19) - 9;
        lv_obj_set_pos(s_eye_l, 178 - 33 + s_gaze_dx, 225 - 46 + s_gaze_dy);
        lv_obj_set_pos(s_eye_r, 288 - 33 + s_gaze_dx, 225 - 46 + s_gaze_dy);
    }

    /* working: 2.4s rotating arc segment */
    if (a->state == BOT_STATE_WORKING) {
        uint32_t phase = (now - s_state_entered) % 2400;
        int start = (int)(phase * 360 / 2400);
        lv_arc_set_angles(s_arc, (uint16_t)start, (uint16_t)((start + 60) % 360));
    } else {
        lv_arc_set_angles(s_arc, 0, 0);
    }

    /* waiting: 2.2s low-amplitude breathing on the "?" mark */
    if (a->state == BOT_STATE_WAITING || a->state == BOT_STATE_UNKNOWN) {
        uint32_t phase = (now - s_state_entered) % 2200;
        lv_opa_t opa = (phase < 1100)
            ? (lv_opa_t)(LV_OPA_60 + phase * (LV_OPA_COVER - LV_OPA_60) / 1100)
            : (lv_opa_t)(LV_OPA_COVER - (phase - 1100) * (LV_OPA_COVER - LV_OPA_60) / 1100);
        lv_obj_set_style_text_opa(s_mark, opa, 0);
    }
}
