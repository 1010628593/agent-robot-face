/* face.c — FACE screen with 18 expressions (design/face_expressions.png).
 *
 * Layout: circular display 466x466, agent title + accent dot at top,
 * state ring r=199, eyes centered, state label, aux label, hint.
 * Protocol states (idle/working/tool/waiting/done/error/cancelled/unknown)
 * are mapped to visual expressions for display.
 */
#include "bot_ui.h"

#include <math.h>
#include <stdlib.h>

#include "lvgl.h"

/* ---- color palette ---- */
#define COL_EYES 0xDDEAF2
#define COL_SECONDARY 0x83949F
#define COL_WORKING 0x5A9BFF
#define COL_TOOL 0xAE8CFF
#define COL_WAITING 0xFFBE55
#define COL_DONE 0x6DE1A3
#define COL_ERROR 0xFF707C
#define COL_SLEEP 0x647680
#define COL_HAPPY 0x6DE1A3

/* ---- eye geometry constants ---- */
#define EYE_CX_LEFT 178
#define EYE_CX_RIGHT 288
#define EYE_CY 225
#define PILL_W 66
#define PILL_H 92
#define PILL_R 30

/* ---- UI objects ---- */
static lv_obj_t *s_title;
static lv_obj_t *s_dot;
static lv_obj_t *s_ring;
static lv_obj_t *s_arc;
static lv_obj_t *s_eye_l;
static lv_obj_t *s_eye_r;
static lv_obj_t *s_mark;
static lv_obj_t *s_state;
static lv_obj_t *s_aux;
static lv_obj_t *s_hint;

/* ---- animation state ---- */
static uint32_t s_next_blink;
static uint32_t s_blink_end;
static uint32_t s_next_gaze;
static int s_gaze_dx;
static int s_gaze_dy;
static uint32_t s_state_entered;
static bot_state_t s_current_state;

/* ---- helper: create a pill-shaped eye ---- */
static lv_obj_t *make_pill_eye(lv_obj_t *parent, int cx, int cy, int w, int h, int r)
{
    lv_obj_t *e = lv_obj_create(parent);
    lv_obj_set_size(e, w, h);
    lv_obj_set_pos(e, cx - w / 2, cy - h / 2);
    lv_obj_set_style_radius(e, r, 0);
    lv_obj_set_style_bg_color(e, lv_color_hex(COL_EYES), 0);
    lv_obj_set_style_bg_opa(e, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(e, 0, 0);
    lv_obj_remove_flag(e, LV_OBJ_FLAG_SCROLLABLE);
    return e;
}

/* ---- helper: create label ---- */
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

/* ---- helper: state color ---- */
static uint32_t state_color(bot_state_t st)
{
    switch (st) {
    case BOT_STATE_WORKING: return COL_WORKING;
    case BOT_STATE_TOOL: return COL_TOOL;
    case BOT_STATE_WAITING: return COL_WAITING;
    case BOT_STATE_DONE: return COL_DONE;
    case BOT_STATE_ERROR: return COL_ERROR;
    case BOT_STATE_UNKNOWN: return COL_SLEEP;
    default: return COL_SECONDARY;
    }
}

/* ---- helper: state text ---- */
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

/* ---- expression setters (mapped from protocol states) ---- */

/* IDLE: two vertical pill eyes */
static void set_idle(void)
{
    lv_obj_set_size(s_eye_l, PILL_W, PILL_H);
    lv_obj_set_pos(s_eye_l, EYE_CX_LEFT - PILL_W / 2, EYE_CY - PILL_H / 2);
    lv_obj_set_style_radius(s_eye_l, PILL_R, 0);
    lv_obj_set_size(s_eye_r, PILL_W, PILL_H);
    lv_obj_set_pos(s_eye_r, EYE_CX_RIGHT - PILL_W / 2, EYE_CY - PILL_H / 2);
    lv_obj_set_style_radius(s_eye_r, PILL_R, 0);
    lv_label_set_text(s_mark, "");
}

/* WORKING: focused angular eyes */
static void set_working(void)
{
    lv_obj_set_size(s_eye_l, 66, 76);
    lv_obj_set_pos(s_eye_l, EYE_CX_LEFT - 33, EYE_CY - 38);
    lv_obj_set_style_radius(s_eye_l, 20, 0);
    lv_obj_set_size(s_eye_r, 66, 76);
    lv_obj_set_pos(s_eye_r, EYE_CX_RIGHT - 33, EYE_CY - 38);
    lv_obj_set_style_radius(s_eye_r, 20, 0);
    lv_label_set_text(s_mark, "");
}

/* TOOL: narrow focused eyes */
static void set_tool(void)
{
    lv_obj_set_size(s_eye_l, 66, 60);
    lv_obj_set_pos(s_eye_l, EYE_CX_LEFT - 33, EYE_CY - 30);
    lv_obj_set_style_radius(s_eye_l, 25, 0);
    lv_obj_set_size(s_eye_r, 66, 60);
    lv_obj_set_pos(s_eye_r, EYE_CX_RIGHT - 33, EYE_CY - 30);
    lv_obj_set_style_radius(s_eye_r, 25, 0);
    lv_label_set_text(s_mark, "");
}

/* WAITING: pill eyes with "?" */
static void set_waiting(void)
{
    lv_obj_set_size(s_eye_l, PILL_W, PILL_H);
    lv_obj_set_pos(s_eye_l, EYE_CX_LEFT - PILL_W / 2, EYE_CY - PILL_H / 2);
    lv_obj_set_style_radius(s_eye_l, PILL_R, 0);
    lv_obj_set_size(s_eye_r, PILL_W, PILL_H);
    lv_obj_set_pos(s_eye_r, EYE_CX_RIGHT - PILL_W / 2, EYE_CY - PILL_H / 2);
    lv_obj_set_style_radius(s_eye_r, PILL_R, 0);
    lv_label_set_text(s_mark, "?");
}

/* DONE: happy curved eyes */
static void set_done(void)
{
    lv_obj_set_size(s_eye_l, 66, 40);
    lv_obj_set_pos(s_eye_l, EYE_CX_LEFT - 33, EYE_CY);
    lv_obj_set_style_radius(s_eye_l, 20, 0);
    lv_obj_set_size(s_eye_r, 66, 40);
    lv_obj_set_pos(s_eye_r, EYE_CX_RIGHT - 33, EYE_CY);
    lv_obj_set_style_radius(s_eye_r, 20, 0);
    lv_label_set_text(s_mark, "");
}

/* ERROR: X eyes */
static void set_error(void)
{
    lv_obj_set_size(s_eye_l, 0, 0);
    lv_obj_set_size(s_eye_r, 0, 0);
    lv_label_set_text(s_mark, "X X");
}

/* CANCELLED: half-closed relaxed eyes */
static void set_cancelled(void)
{
    lv_obj_set_size(s_eye_l, 66, 46);
    lv_obj_set_pos(s_eye_l, EYE_CX_LEFT - 33, EYE_CY - 10);
    lv_obj_set_style_radius(s_eye_l, 23, 0);
    lv_obj_set_size(s_eye_r, 66, 46);
    lv_obj_set_pos(s_eye_r, EYE_CX_RIGHT - 33, EYE_CY - 10);
    lv_obj_set_style_radius(s_eye_r, 23, 0);
    lv_label_set_text(s_mark, "");
}

/* UNKNOWN: pill eyes with "?" */
static void set_unknown(void)
{
    lv_obj_set_size(s_eye_l, 56, 56);
    lv_obj_set_pos(s_eye_l, EYE_CX_LEFT - 28, EYE_CY - 28);
    lv_obj_set_style_radius(s_eye_l, 28, 0);
    lv_obj_set_size(s_eye_r, 56, 56);
    lv_obj_set_pos(s_eye_r, EYE_CX_RIGHT - 28, EYE_CY - 28);
    lv_obj_set_style_radius(s_eye_r, 28, 0);
    lv_label_set_text(s_mark, "?");
}

/* ---- main build ---- */
void face_build(lv_obj_t *scr)
{
    const bot_sim_agent_t *a = &g_ui.agents[g_ui.selected];
    uint32_t col = state_color(a->state);
    s_current_state = a->state;

    /* agent dot */
    s_dot = lv_obj_create(scr);
    lv_obj_set_size(s_dot, 8, 8);
    lv_obj_set_pos(s_dot, 101 - 4, 73 - 4);
    lv_obj_set_style_radius(s_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_dot, lv_color_hex(a->accent), 0);
    lv_obj_set_style_bg_opa(s_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_dot, 0, 0);

    /* agent title */
    s_title = make_label(scr, a->label, 113, 57, 240, 32,
                         &lv_font_montserrat_24, a->accent, LV_TEXT_ALIGN_LEFT);

    /* state ring r=199 */
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

    /* working activity arc */
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

    /* eyes */
    s_eye_l = make_pill_eye(scr, EYE_CX_LEFT, EYE_CY, PILL_W, PILL_H, PILL_R);
    s_eye_r = make_pill_eye(scr, EYE_CX_RIGHT, EYE_CY, PILL_W, PILL_H, PILL_R);

    /* mark label */
    s_mark = make_label(scr, "", 208, 180, 50, 60,
                        &lv_font_montserrat_48, col, LV_TEXT_ALIGN_CENTER);

    /* state label */
    s_state = make_label(scr, state_text(a->state), 91, 322, 284, 32,
                         &lv_font_montserrat_24, col, LV_TEXT_ALIGN_CENTER);

    /* aux label */
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

    /* hint */
    s_hint = make_label(scr, "HOLD TO SWITCH", 153, 409, 160, 24,
                        &lv_font_montserrat_20, COL_SECONDARY, LV_TEXT_ALIGN_CENTER);

    /* set expression based on protocol state */
    switch (a->state) {
    case BOT_STATE_IDLE: set_idle(); break;
    case BOT_STATE_WORKING: set_working(); break;
    case BOT_STATE_TOOL: set_tool(); break;
    case BOT_STATE_WAITING: set_waiting(); break;
    case BOT_STATE_DONE: set_done(); break;
    case BOT_STATE_ERROR: set_error(); break;
    case BOT_STATE_CANCELLED: set_cancelled(); break;
    case BOT_STATE_UNKNOWN: set_unknown(); break;
    default: set_idle(); break;
    }

    /* init animation state */
    s_state_entered = (uint32_t)(lv_tick_get());
    s_next_blink = s_state_entered + 2800 + (uint32_t)(rand() % 3700);
    s_blink_end = 0;
    s_next_gaze = s_state_entered + 3000;
    s_gaze_dx = 0;
    s_gaze_dy = 0;
}

/* ---- tick: animations ---- */
void face_tick(uint32_t now)
{
    /* auto-blink for idle state */
    if (s_current_state == BOT_STATE_IDLE) {
        if (s_blink_end == 0 && now >= s_next_blink) {
            s_blink_end = now + 120;
            lv_obj_set_height(s_eye_l, 12);
            lv_obj_set_height(s_eye_r, 12);
            s_next_blink = now + 2800 + (uint32_t)(rand() % 3700);
        }
        if (s_blink_end != 0 && now >= s_blink_end) {
            s_blink_end = 0;
            lv_obj_set_height(s_eye_l, PILL_H);
            lv_obj_set_height(s_eye_r, PILL_H);
        }

        /* gaze wander */
        if (now >= s_next_gaze) {
            s_next_gaze = now + 3000 + (uint32_t)(rand() % 3000);
            s_gaze_dx = (rand() % 19) - 9;
            s_gaze_dy = (rand() % 19) - 9;
            lv_obj_set_pos(s_eye_l, EYE_CX_LEFT - PILL_W / 2 + s_gaze_dx,
                           EYE_CY - PILL_H / 2 + s_gaze_dy);
            lv_obj_set_pos(s_eye_r, EYE_CX_RIGHT - PILL_W / 2 + s_gaze_dx,
                           EYE_CY - PILL_H / 2 + s_gaze_dy);
        }
    }

    /* working: rotating arc */
    if (s_current_state == BOT_STATE_WORKING) {
        uint32_t phase = (now - s_state_entered) % 2400;
        int start = (int)(phase * 360 / 2400);
        lv_arc_set_angles(s_arc, (uint16_t)start, (uint16_t)((start + 60) % 360));
    } else {
        lv_arc_set_angles(s_arc, 0, 0);
    }

    /* waiting/unknown: breathing mark */
    if (s_current_state == BOT_STATE_WAITING || s_current_state == BOT_STATE_UNKNOWN) {
        uint32_t phase = (now - s_state_entered) % 2200;
        lv_opa_t opa = (phase < 1100)
            ? (lv_opa_t)(LV_OPA_60 + phase * (LV_OPA_COVER - LV_OPA_60) / 1100)
            : (lv_opa_t)(LV_OPA_COVER - (phase - 1100) * (LV_OPA_COVER - LV_OPA_60) / 1100);
        lv_obj_set_style_text_opa(s_mark, opa, 0);
    }
}
