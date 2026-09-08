/* Test-only LVGL API seam: verifies UI lifecycle, not LVGL rendering/ESP-IDF. */
#ifndef FAKE_LVGL_H
#define FAKE_LVGL_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#define LV_RADIUS_CIRCLE 32767
#define LV_OPA_COVER 255
#define LV_OPA_TRANSP 0
#define LV_OBJ_FLAG_SCROLLABLE 1
#define LV_OBJ_FLAG_CLICKABLE 2
#define LV_OBJ_FLAG_HIDDEN 4
#define LV_EVENT_DRAW_MAIN 1
#define LV_EVENT_DELETE 2
#define LV_EVENT_ALL 255
#define LV_TEXT_ALIGN_CENTER 1
#define lv_snprintf snprintf
 typedef uint8_t lv_opa_t;
typedef uint32_t lv_color_t;
typedef struct {int32_t x,y;} lv_point_t;
typedef struct {float x,y;} lv_point_precise_t;
typedef struct {int32_t x1,y1,x2,y2;} lv_area_t;
typedef struct {int unused;} lv_font_t;
typedef struct {int unused;} lv_layer_t;
typedef enum {LV_INDEV_STATE_RELEASED,LV_INDEV_STATE_PRESSED} lv_indev_state_t;
typedef struct {lv_point_t p;lv_indev_state_t state;} lv_indev_t;
typedef struct lv_obj lv_obj_t;
typedef struct {int code;lv_obj_t *target;} lv_event_t;
struct lv_obj {int x,y,w,h,rotation;bool alive;lv_obj_t *parent;void (*cb)(lv_event_t *);char text[64];};
typedef struct {lv_color_t bg_color;lv_opa_t bg_opa;int radius,border_width;} lv_draw_rect_dsc_t;
typedef struct {lv_point_precise_t p1,p2;lv_color_t color;int width;lv_opa_t opa;bool round_start,round_end;} lv_draw_line_dsc_t;
typedef struct {lv_point_precise_t p[3];lv_color_t color;lv_opa_t opa;} lv_draw_triangle_dsc_t;
typedef struct {lv_point_t center;uint16_t radius;int width;float start_angle,end_angle;lv_color_t color;lv_opa_t opa;bool rounded;} lv_draw_arc_dsc_t;
extern lv_font_t lv_font_montserrat_20;
uint32_t lv_tick_get(void);
lv_obj_t *lv_obj_create(lv_obj_t *p);
lv_obj_t *lv_label_create(lv_obj_t *p);
void lv_obj_clean(lv_obj_t *p);
void lv_obj_set_style_transform_pivot_x(lv_obj_t *p,int v,int s);
void lv_obj_set_style_transform_pivot_y(lv_obj_t *p,int v,int s);
void lv_obj_set_style_transform_rotation(lv_obj_t *p,int v,int s);
int lv_obj_get_style_transform_rotation(lv_obj_t *p,int s);
void lv_obj_remove_style_all(lv_obj_t *p);
void lv_obj_remove_flag(lv_obj_t *p,int f);
void lv_obj_add_flag(lv_obj_t *p,int f);
void lv_obj_set_size(lv_obj_t *p,int w,int h);
void lv_obj_set_pos(lv_obj_t *p,int x,int y);
void lv_obj_set_style_bg_color(lv_obj_t *p,lv_color_t c,int s);
void lv_obj_set_style_bg_opa(lv_obj_t *p,int o,int s);
void lv_obj_set_style_text_font(lv_obj_t *p,const lv_font_t *f,int s);
void lv_obj_set_style_text_color(lv_obj_t *p,lv_color_t c,int s);
void lv_obj_set_style_text_align(lv_obj_t *p,int a,int s);
void lv_label_set_text(lv_obj_t *p,const char *t);
const char *lv_label_get_text(lv_obj_t *p);
lv_color_t lv_color_hex(uint32_t c);
lv_color_t lv_color_mix(lv_color_t a,lv_color_t b,uint8_t mix);
void lv_scr_load(lv_obj_t *p);
void lv_indev_get_point(lv_indev_t *i,lv_point_t *p);
lv_indev_state_t lv_indev_get_state(lv_indev_t *i);
void lv_obj_add_event_cb(lv_obj_t *p,void (*cb)(lv_event_t *),int f,void *u);
int lv_event_get_code(lv_event_t *e);
lv_obj_t *lv_event_get_target_obj(lv_event_t *e);
lv_layer_t *lv_event_get_layer(lv_event_t *e);
void lv_obj_get_coords(lv_obj_t *p,lv_area_t *a);
void lv_obj_invalidate(lv_obj_t *p);
void lv_obj_invalidate_area(lv_obj_t *p,const lv_area_t *area);
void lv_draw_rect_dsc_init(lv_draw_rect_dsc_t *d);
void lv_draw_line_dsc_init(lv_draw_line_dsc_t *d);
void lv_draw_triangle_dsc_init(lv_draw_triangle_dsc_t *d);
void lv_draw_arc_dsc_init(lv_draw_arc_dsc_t *d);
void lv_draw_rect(lv_layer_t *l,const lv_draw_rect_dsc_t *d,const lv_area_t *a);
void lv_draw_line(lv_layer_t *l,const lv_draw_line_dsc_t *d);
void lv_draw_triangle(lv_layer_t *l,const lv_draw_triangle_dsc_t *d);
void lv_draw_arc(lv_layer_t *l,const lv_draw_arc_dsc_t *d);
extern uint32_t fake_now;
extern int fake_cleans,fake_creates,fake_draws,fake_invalidations;
extern lv_indev_t fake_indev;
lv_obj_t *fake_find_label(const char *text);
#endif
