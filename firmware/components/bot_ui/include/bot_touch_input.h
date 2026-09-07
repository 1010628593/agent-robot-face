#ifndef BOT_TOUCH_INPUT_H
#define BOT_TOUCH_INPUT_H
#include "bot_gesture.h"
#include "lvgl.h"
/* LVGL owner only. Replaces the adapter read, never adds a hardware reader. */
bool bot_touch_input_start(lv_indev_t *indev);
typedef struct {
    uint32_t reads,dual,errors,overflows;
    uint8_t count,header,wire_count,record0,record1;
} bot_touch_input_debug_t;
void bot_touch_input_debug(bot_touch_input_debug_t *out);
bool bot_touch_input_pop(bot_touch_frame_t *frame);
#endif
