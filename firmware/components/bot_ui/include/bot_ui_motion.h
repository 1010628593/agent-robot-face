#ifndef BOT_UI_MOTION_H
#define BOT_UI_MOTION_H
#include "lvgl.h"
#include "bot_os_motion.h"
/* Object-owned animations; LVGL removes them when the target is deleted. */
void bot_ui_press_feedback(lv_obj_t *object,bool pressed);
void bot_ui_preview_slide(lv_obj_t *object,int direction);
void bot_ui_indicator_move(lv_obj_t *object,int x);
#endif
