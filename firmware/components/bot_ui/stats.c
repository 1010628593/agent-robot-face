/* stats.c — STATS screen with usage / quota subpages (02_UI_UX §4).
 *
 * Usage: hero number (TURNS) y=133 h=74; rows TOKENS / ACTIVE y=251/293;
 * 24-bucket sparkline (105,332,256,30), null buckets left empty;
 * freshness (130,375). Quota: up to two cards with name, main value, bar,
 * reset; N/A shown as N/A (never fabricated), UNLIMITED spelled out.
 * Refresh hit area shows cached countdown only (SIM build: no real refresh).
 */
#include "bot_ui.h"

#include <math.h>

#include "lvgl.h"

void stats_refresh(void);

#define COL_SECONDARY 0x83949F
#define COL_TEXT 0xDDEAF2
#define COL_ERROR 0xFF707C
#define COL_DONE 0x6DE1A3

static lv_obj_t *s_body; /* rebuilt on tab switch */

static lv_obj_t *mk_label(lv_obj_t *parent, const char *txt, int x, int y, int w,
                          int h, const lv_font_t *font, uint32_t color,
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

static void build_usage(lv_obj_t *parent, const bot_sim_agent_t *a)
{
    char buf[48];

    lv_snprintf(buf, sizeof(buf), "%lu", (unsigned long)a->turns);
    mk_label(parent, buf, 83, 133, 300, 74, &lv_font_montserrat_48, COL_TEXT,
             LV_TEXT_ALIGN_CENTER);
    mk_label(parent, "TURNS TODAY", 133, 210, 200, 24, &lv_font_montserrat_20,
             COL_SECONDARY, LV_TEXT_ALIGN_CENTER);

    lv_snprintf(buf, sizeof(buf), "TOKENS  %lu", (unsigned long)a->total_tokens);
    mk_label(parent, buf, 91, 251, 284, 24, &lv_font_montserrat_24, COL_TEXT,
             LV_TEXT_ALIGN_CENTER);

    char aux[32];
    unsigned mins = (unsigned)(a->active_time_ms / 60000);
    lv_snprintf(buf, sizeof(buf), "ACTIVE  %u:%02u  %u RUN",
                mins / 60, mins % 60, (unsigned)a->active_sessions);
    (void)aux;
    mk_label(parent, buf, 91, 293, 284, 24, &lv_font_montserrat_24, COL_TEXT,
             LV_TEXT_ALIGN_CENTER);

    /* sparkline (105,332,256,30): 24 buckets, null = uncovered -> empty */
    int16_t max = 1;
    for (int i = 0; i < 24; i++) {
        if (a->spark[i] > max) max = a->spark[i];
    }
    for (int i = 0; i < 24; i++) {
        if (a->spark[i] < 0) continue; /* leave gaps, never interpolate */
        int h = 4 + (int)(26 * a->spark[i] / max);
        lv_obj_t *bar = lv_obj_create(parent);
        lv_obj_set_size(bar, 7, h);
        lv_obj_set_pos(bar, 105 + i * 11, 332 + (30 - h));
        lv_obj_set_style_radius(bar, 2, 0);
        lv_obj_set_style_bg_color(bar, lv_color_hex(a->accent), 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_70, 0);
        lv_obj_set_style_border_width(bar, 0, 0);
        lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    }

    mk_label(parent, "SIM / 2s   ~ SIMULATED", 130, 375, 206, 22,
             &lv_font_montserrat_20, COL_SECONDARY, LV_TEXT_ALIGN_CENTER);
}

static void quota_card(lv_obj_t *parent, int y, const char *name,
                       const char *value, int pct_left, const char *reset,
                       uint32_t accent, bool over)
{
    mk_label(parent, name, 83, y, 180, 24, &lv_font_montserrat_20,
             COL_SECONDARY, LV_TEXT_ALIGN_LEFT);
    mk_label(parent, value, 243, y - 8, 140, 32, &lv_font_montserrat_24,
             over ? COL_ERROR : COL_TEXT, LV_TEXT_ALIGN_RIGHT);

    lv_obj_t *bar = lv_bar_create(parent);
    lv_obj_set_size(bar, 200, 8);
    lv_obj_set_pos(bar, 83, y + 30);
    lv_bar_set_range(bar, 0, 100);
    if (pct_left >= 0) {
        lv_bar_set_value(bar, pct_left > 100 ? 100 : pct_left, LV_ANIM_OFF);
    } else {
        lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    }
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x1A2229), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_hex(over ? COL_ERROR : accent),
                              LV_PART_INDICATOR);

    mk_label(parent, reset, 300, y + 22, 100, 22, &lv_font_montserrat_20,
             over ? COL_ERROR : COL_SECONDARY, LV_TEXT_ALIGN_RIGHT);
}

static void build_quota(lv_obj_t *parent, const bot_sim_agent_t *a)
{
    int shown = 0;
    for (int i = 0; i < 2 && shown < 2; i++) {
        const typeof(a->quota[0]) *q = &a->quota[i];
        if (!q->label || q->label[0] == '\0') continue;
        char value[32];
        int pct_left = -1;
        const char *reset = q->reset_label;
        bool over = false;
        if (q->na) {
            lv_snprintf(value, sizeof(value), "N/A");
        } else if (q->unlimited) {
            lv_snprintf(value, sizeof(value), "UNLIMITED");
        } else {
            pct_left = 100 - q->used_pct;
            over = q->used_pct >= 100;
            if (over) {
                lv_snprintf(value, sizeof(value), "OVER");
            } else {
                lv_snprintf(value, sizeof(value), "%d%% LEFT", pct_left);
            }
        }
        quota_card(parent, shown == 0 ? 150 : 250, q->label, value, pct_left,
                   reset, a->accent, over);
        shown++;
    }
    if (shown == 0) {
        mk_label(parent, "NO QUOTA SOURCE", 113, 210, 240, 32,
                 &lv_font_montserrat_24, COL_SECONDARY, LV_TEXT_ALIGN_CENTER);
    }
    mk_label(parent, "SIM / 1m     REFRESH", 130, 375, 206, 22,
             &lv_font_montserrat_20, COL_SECONDARY, LV_TEXT_ALIGN_CENTER);
}

void stats_build(lv_obj_t *scr)
{
    const bot_sim_agent_t *a = &g_ui.agents[g_ui.selected];

    mk_label(scr, a->label, 113, 57, 240, 32, &lv_font_montserrat_24,
             a->accent, LV_TEXT_ALIGN_LEFT);
    mk_label(scr, g_ui.stats_tab == 0 ? "USAGE" : "QUOTA", 300, 57, 90, 24,
             &lv_font_montserrat_20, COL_SECONDARY, LV_TEXT_ALIGN_RIGHT);

    s_body = lv_obj_create(scr);
    lv_obj_set_size(s_body, 466, 466);
    lv_obj_set_pos(s_body, 0, 0);
    lv_obj_set_style_bg_opa(s_body, LV_OPA_0, 0);
    lv_obj_set_style_border_width(s_body, 0, 0);
    lv_obj_remove_flag(s_body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_background(s_body);

    stats_refresh();
}

void stats_refresh(void)
{
    if (!s_body) return;
    lv_obj_clean(s_body);
    const bot_sim_agent_t *a = &g_ui.agents[g_ui.selected];
    if (g_ui.stats_tab == 0) {
        build_usage(s_body, a);
    } else {
        build_quota(s_body, a);
    }
}
