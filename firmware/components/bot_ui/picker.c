/* picker.c — AGENT_PICKER screen (02_UI_UX §3).
 *
 * Title SELECT AGENT (123,58,220,32); center card (161,154,144,144) with
 * agent initial; name y=314; state y=348; ~36px weakened neighbor cards;
 * bottom TAP TO SELECT / SELECTING... ; order fixed codex->workbuddy->
 * cursor->hermes, wraps. Confirm is a SIM transaction in this build.
 */
#include "bot_ui.h"

#include "lvgl.h"

void picker_refresh(void);

#define COL_SECONDARY 0x83949F
#define COL_CARD 0x10161B

static lv_obj_t *s_card_l;
static lv_obj_t *s_card_c;
static lv_obj_t *s_card_r;
static lv_obj_t *s_init_c;
static lv_obj_t *s_name;
static lv_obj_t *s_state;
static lv_obj_t *s_hint;

static lv_obj_t *make_card(lv_obj_t *parent, int x, int y, int size, uint32_t accent,
                           bool center)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_set_size(c, size, size);
    lv_obj_set_pos(c, x, y);
    lv_obj_set_style_radius(c, 24, 0);
    lv_obj_set_style_bg_color(c, lv_color_hex(COL_CARD), 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(c, center ? 3 : 1, 0);
    lv_obj_set_style_border_color(c, lv_color_hex(accent), 0);
    lv_obj_set_style_border_opa(c, center ? LV_OPA_COVER : LV_OPA_40, 0);
    lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    return c;
}

static void fill_card(lv_obj_t *card, lv_obj_t *init_label, uint8_t agent_idx,
                      bool center)
{
    const bot_sim_agent_t *a = &g_ui.agents[agent_idx];
    lv_obj_set_style_border_color(card, lv_color_hex(a->accent), 0);
    if (init_label) {
        char init[2] = { a->label[0], '\0' };
        lv_label_set_text(init_label, init);
        lv_obj_set_style_text_color(init_label, lv_color_hex(a->accent), 0);
    }
    (void)center;
}

static const char *state_text_short(bot_state_t st)
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

void picker_build(lv_obj_t *scr)
{
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "SELECT AGENT");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xDDEAF2), 0);
    lv_obj_set_size(title, 220, 32);
    lv_obj_set_pos(title, 123, 58);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

    /* neighbor previews ~36px visible at the edges */
    s_card_l = make_card(scr, 25, 190, 72, 0x000000, false);
    s_card_c = make_card(scr, 161, 154, 144, 0x000000, true);
    s_card_r = make_card(scr, 369, 190, 72, 0x000000, false);

    s_init_c = lv_label_create(s_card_c);
    lv_obj_set_style_text_font(s_init_c, &lv_font_montserrat_48, 0);
    lv_obj_center(s_init_c);

    s_name = lv_label_create(scr);
    lv_obj_set_style_text_font(s_name, &lv_font_montserrat_24, 0);
    lv_obj_set_size(s_name, 240, 32);
    lv_obj_set_pos(s_name, 113, 314);
    lv_obj_set_style_text_align(s_name, LV_TEXT_ALIGN_CENTER, 0);

    s_state = lv_label_create(scr);
    lv_obj_set_style_text_font(s_state, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_state, lv_color_hex(COL_SECONDARY), 0);
    lv_obj_set_size(s_state, 240, 24);
    lv_obj_set_pos(s_state, 113, 348);
    lv_obj_set_style_text_align(s_state, LV_TEXT_ALIGN_CENTER, 0);

    s_hint = lv_label_create(scr);
    lv_obj_set_style_text_font(s_hint, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_hint, lv_color_hex(COL_SECONDARY), 0);
    lv_obj_set_size(s_hint, 240, 24);
    lv_obj_set_pos(s_hint, 113, 409);
    lv_obj_set_style_text_align(s_hint, LV_TEXT_ALIGN_CENTER, 0);

    picker_refresh();
}

void picker_refresh(void)
{
    uint8_t p = g_ui.picker_preview;
    uint8_t prev = (uint8_t)((p + BOT_AGENT_COUNT - 1) % BOT_AGENT_COUNT);
    uint8_t next = (uint8_t)((p + 1) % BOT_AGENT_COUNT);

    fill_card(s_card_c, s_init_c, p, true);
    fill_card(s_card_l, NULL, prev, false);
    fill_card(s_card_r, NULL, next, false);

    const bot_sim_agent_t *a = &g_ui.agents[p];
    lv_label_set_text(s_name, a->label);
    lv_obj_set_style_text_color(s_name, lv_color_hex(a->accent), 0);
    lv_label_set_text(s_state, state_text_short(a->state));
    lv_label_set_text(s_hint, g_ui.picker_selecting ? "SELECTING..." : "TAP TO SELECT");
}
