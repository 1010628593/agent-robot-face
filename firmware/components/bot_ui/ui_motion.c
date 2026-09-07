#include "bot_ui_motion.h"
static void press_value(void *o,int32_t v) {
    lv_obj_set_style_bg_opa(o,(lv_opa_t)v,0);
    lv_obj_set_style_border_opa(o,(lv_opa_t)(190+65*v/160),0);
}
void bot_ui_press_feedback(lv_obj_t *o,bool pressed) {
    if(!o)return;
    int from=lv_obj_get_style_bg_opa(o,0);
    lv_anim_delete(o,press_value);
    lv_anim_t a;lv_anim_init(&a);lv_anim_set_var(&a,o);
    lv_anim_set_values(&a,from,pressed?160:0);
    lv_anim_set_duration(&a,pressed?BOT_OS_PRESS_MS:BOT_OS_RELEASE_MS);
    lv_anim_set_exec_cb(&a,press_value);lv_anim_set_path_cb(&a,lv_anim_path_ease_out);lv_anim_start(&a);
}
static void preview_x(void *o,int32_t x){lv_obj_set_style_translate_x(o,x,0);}
void bot_ui_preview_slide(lv_obj_t *o,int direction) {
    if(!o)return;
    bool moving=lv_anim_get(o,preview_x)!=NULL;
    int from=moving?lv_obj_get_style_translate_x(o,0):direction*BOT_OS_PREVIEW_PX;
    lv_anim_delete(o,preview_x);
    lv_anim_t a;lv_anim_init(&a);lv_anim_set_var(&a,o);lv_anim_set_values(&a,from,0);
    lv_anim_set_duration(&a,BOT_OS_PREVIEW_MS);lv_anim_set_exec_cb(&a,preview_x);
    lv_anim_set_path_cb(&a,lv_anim_path_ease_out);lv_anim_start(&a);
}
static void indicator_x(void *o,int32_t x){lv_obj_set_x(o,x);}
void bot_ui_indicator_move(lv_obj_t *o,int x) {
    if(!o)return;
    int from=lv_obj_get_x(o);lv_anim_delete(o,indicator_x);
    lv_anim_t a;lv_anim_init(&a);lv_anim_set_var(&a,o);lv_anim_set_values(&a,from,x);
    lv_anim_set_duration(&a,BOT_OS_TAB_MS);lv_anim_set_exec_cb(&a,indicator_x);
    lv_anim_set_path_cb(&a,lv_anim_path_ease_out);lv_anim_start(&a);
}
