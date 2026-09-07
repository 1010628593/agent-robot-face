#ifndef BOT_FACE_H
#define BOT_FACE_H
#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>
/* UI-owner context only. Record each Agent's event clock even off-screen. */
void face_sync(uint32_t now_ms);
void face_build(lv_obj_t *screen);
void face_tick(uint32_t now_ms);
void face_touch(bool pressed,int16_t x,int16_t y,float hold,uint32_t now_ms);
void face_suspend(uint32_t now_ms);
#endif
