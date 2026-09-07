#ifndef BOT_FACE_H
#define BOT_FACE_H
#include "lvgl.h"
#include "bot_motion.h"
#include "bot_gesture.h"
#include <stdbool.h>
#include <stdint.h>
/* UI-owner context only. Record each Agent's event clock even off-screen. */
void face_sync(uint32_t now_ms);
void face_build(lv_obj_t *screen);
void face_tick(uint32_t now_ms);
void face_touch(bool pressed,int16_t x,int16_t y,float hold,uint32_t now_ms);
typedef struct {
    int expression,count,tapped;
    float x,y,left,right;
    uint32_t revision;
} bot_face_touch_debug_t;
void face_touch_debug(bot_face_touch_debug_t *out,uint32_t now);
void face_interact(const bot_touch_frame_t *frame,float comfort,uint32_t held_ms);
void face_poke(int16_t x,int16_t y,uint32_t now_ms);
void face_pet(bool stroke,uint32_t now_ms);
void face_suspend(uint32_t now_ms);
/* Device-level environment, not an Agent state or wire-schema extension. */
void face_set_motion(const bot_motion_view_t *view);
void face_map_input(bool pressed,int16_t x,int16_t y,int16_t *out_x,int16_t *out_y);
#endif
