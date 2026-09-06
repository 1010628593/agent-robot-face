#ifndef BOT_FACE_H
#define BOT_FACE_H
#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>
/* UI-owner context only. Motion survives leaving/re-entering FACE. */
void face_build(lv_obj_t *screen);
void face_tick(uint32_t now_ms);
void face_touch(bool pressed,int16_t x,int16_t y,float hold,uint32_t now_ms);
void face_suspend(uint32_t now_ms);
#endif
