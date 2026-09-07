/* Integration acceptance for motion coordinates through the production UI.
 * Real LVGL; only the physical panel, pointer and IMU input are virtual.
 * Each case is a separate process so no test relies on another case's state. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "bot_ui.h"
#include "bot_face.h"
#include "lvgl.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static uint32_t now_ms;
static lv_display_t *display;
static lv_indev_t *pointer;
static lv_point_t point = {233, 233};
static bool pressed;
static bot_motion_view_t motion;
static bool refresh_sample = true;
static uint32_t draw_buffer[466 * 32];
extern lv_obj_t *bot_ui_screen(void);

static uint32_t clock_ms(void) { return now_ms; }
lv_indev_t *bsp_display_get_input_dev(void) { return pointer; }
static void read_pointer(lv_indev_t *dev, lv_indev_data_t *data)
{
    (void)dev;
    data->point = point;
    data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    data->continue_reading = false;
}
static void flush(lv_display_t *dev, const lv_area_t *area, uint8_t *map)
{
    (void)map;
    assert(area->x1 >= 0 && area->y1 >= 0 && area->x2 < 466 && area->y2 < 466);
    lv_display_flush_ready(dev);
}
static void advance(uint32_t ms)
{
    assert(ms % 10 == 0);
    for (uint32_t i = 0; i < ms; i += 10) {
        now_ms += 10;
        g_ui.sim_cycle_ms = now_ms; /* Freeze SIM changes, not UI/gesture time. */
        if (refresh_sample) motion.sampled_ms = now_ms;
        bot_ui_set_motion(&motion);
        lv_indev_read(pointer);
        bot_ui_poll();
        lv_obj_update_layout(bot_ui_screen());
        lv_refr_now(display);
    }
}
static lv_obj_t *surface(void)
{
    lv_obj_t *root = bot_ui_screen();
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *obj = lv_obj_get_child(root, (int32_t)i);
        if (lv_obj_get_width(obj) == 322 && lv_obj_get_height(obj) == 244) return obj;
    }
    assert(!"Face surface must exist");
    return NULL;
}
static int rotation(void) { return lv_obj_get_style_transform_rotation(surface(), 0); }
static void orient(float degrees)
{
    motion.available = true;
    motion.orientation_valid = true;
    motion.rotation_deg = degrees;
    refresh_sample = true;
    advance(1600);
    int expected = (int)lroundf(degrees * 10);
    if (expected < 0) expected += 3600;
    assert(rotation() == expected);
}
static void swipe(int x1, int y1, int x2, int y2)
{
    point = (lv_point_t){x1, y1}; pressed = true; advance(20);
    point = (lv_point_t){x2, y2}; advance(160);
    pressed = false; advance(20);
}
static void assert_agent_unchanged(void)
{
    assert(g_ui.selected == BOT_AGENT_CODEX);
    assert(g_ui.agents[BOT_AGENT_CODEX].state == BOT_STATE_IDLE);
}
static void rotated_navigation(void)
{
    /* At +90, an on-glass upward swipe is a left swipe in Face coordinates. */
    orient(90);
    swipe(233, 310, 233, 190);
    assert(g_ui.screen == BOT_SCR_STATS);
    assert_agent_unchanged();
    /* Stats remains unrotated: its own ordinary right swipe still returns. */
    swipe(190, 233, 310, 233);
    assert(g_ui.screen == BOT_SCR_FACE);
    /* At +45, a diagonal on glass must become horizontal before arbitration. */
    orient(45);
    swipe(288, 288, 178, 178);
    assert(g_ui.screen == BOT_SCR_STATS);
    assert_agent_unchanged();
}
static void contact_matrix(void)
{
    orient(90);
    int16_t x, y;
    face_map_input(true, 233, 296, &x, &y);
    assert(x == 296 && y == 233); /* physical below -> face right */
    motion.rotation_deg = -90;
    /* No UI pointer is held in this direct map assertion; close it explicitly. */
    face_map_input(false, 233, 296, &x, &y);
    assert(x == 296 && y == 233); /* final UP uses the DOWN matrix */
    motion.rotation_deg = 90;
    point = (lv_point_t){233, 310}; pressed = true; advance(20);
    motion.rotation_deg = -90; advance(100);
    assert(rotation() == 900); /* Cannot rotate underneath a held finger. */
    point = (lv_point_t){233, 190}; advance(60);
    pressed = false; advance(20);
    assert(g_ui.screen == BOT_SCR_STATS); /* final release must still be mapped */
    assert_agent_unchanged();
}
static void stale_and_flat(void)
{
    orient(90);
    refresh_sample = false;
    /* Last physical sample was >250ms ago; do not chase its new angle. */
    motion.sampled_ms = now_ms - BOT_MOTION_STALE_MS - 1;
    motion.rotation_deg = -45;
    advance(400);
    assert(rotation() == 900);
    /* A fresh but near-flat observation also has no usable in-plane heading. */
    refresh_sample = true; motion.orientation_valid = false; motion.flat = true;
    advance(400);
    assert(rotation() == 900);
    /* Sensor absence must preserve gesture navigation, not stall the UI. */
    motion.available = false;
    swipe(233, 310, 233, 190);
    assert(g_ui.screen == BOT_SCR_STATS);
    swipe(190, 233, 310, 233);
    assert(g_ui.screen == BOT_SCR_FACE);
    motion.flat = false;
    orient(-45);
    assert_agent_unchanged();
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    lv_init(); lv_tick_set_cb(clock_ms);
    display = lv_display_create(466, 466); assert(display);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_buffers(display, draw_buffer, NULL, sizeof(draw_buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    pointer = lv_indev_create(); assert(pointer);
    lv_indev_set_type(pointer, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(pointer, read_pointer);
    bot_ui_init();
    g_ui.agents[BOT_AGENT_CODEX].state = BOT_STATE_IDLE;
    g_ui.agents[BOT_AGENT_CODEX].transition_id++;
    advance(500);
    if (!strcmp(argv[1], "navigation")) rotated_navigation();
    else if (!strcmp(argv[1], "contact")) contact_matrix();
    else if (!strcmp(argv[1], "stale")) stale_and_flat();
    else { fprintf(stderr, "Unknown scenario: %s\n", argv[1]); return 2; }
    lv_indev_delete(pointer); pointer = NULL;
    lv_display_delete(display); display = NULL;
    lv_deinit();
    puts("PASS motion input acceptance through real LVGL");
    return 0;
}
