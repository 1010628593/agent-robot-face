#include "lvgl.h"
#include <assert.h>
#include <string.h>
lv_font_t lv_font_montserrat_20;
uint32_t fake_now;
int fake_cleans,fake_creates,fake_draws,fake_invalidations;
lv_indev_t fake_indev;
static lv_obj_t objects[2048];
static unsigned next;
static lv_layer_t layer;
uint32_t lv_tick_get(void){return fake_now;}
int64_t esp_timer_get_time(void){return (int64_t)fake_now*1000;}
lv_indev_t *bsp_display_get_input_dev(void){return &fake_indev;}
lv_obj_t *lv_obj_create(lv_obj_t *p){assert(next<2048);lv_obj_t *o=&objects[next++];memset(o,0,sizeof(*o));o->alive=true;o->parent=p;fake_creates++;return o;}
lv_obj_t *lv_label_create(lv_obj_t *p){return lv_obj_create(p);}
void lv_obj_clean(lv_obj_t *p){fake_cleans++;for(unsigned i=0;i<next;i++)if(objects[i].alive && objects[i].parent==p){lv_obj_t *o=&objects[i];if(o->cb){lv_event_t e={LV_EVENT_DELETE,o};o->cb(&e);}o->alive=false;}}
void lv_obj_remove_style_all(lv_obj_t *p){(void)p;}
void lv_obj_remove_flag(lv_obj_t *p,int f){(void)p;(void)f;}
void lv_obj_add_flag(lv_obj_t *p,int f){(void)p;(void)f;}
const char *lv_label_get_text(lv_obj_t *p){return p->text;}
void lv_obj_set_size(lv_obj_t *p,int w,int h){p->w=w;p->h=h;}
void lv_obj_set_pos(lv_obj_t *p,int x,int y){p->x=x;p->y=y;}
void lv_obj_set_style_bg_color(lv_obj_t *p,lv_color_t c,int s){(void)p;(void)c;(void)s;}
void lv_obj_set_style_bg_opa(lv_obj_t *p,int o,int s){(void)p;(void)o;(void)s;}
void lv_obj_set_style_text_font(lv_obj_t *p,const lv_font_t *f,int s){(void)p;(void)f;(void)s;}
void lv_obj_set_style_text_color(lv_obj_t *p,lv_color_t c,int s){(void)p;(void)c;(void)s;}
void lv_obj_set_style_text_align(lv_obj_t *p,int a,int s){(void)p;(void)a;(void)s;}
void lv_label_set_text(lv_obj_t *p,const char *t){snprintf(p->text,sizeof(p->text),"%s",t);}
lv_color_t lv_color_hex(uint32_t c){return c;}
void lv_scr_load(lv_obj_t *p){(void)p;}
void lv_indev_get_point(lv_indev_t *i,lv_point_t *p){*p=i->p;}
lv_indev_state_t lv_indev_get_state(lv_indev_t *i){return i->state;}
void lv_obj_add_event_cb(lv_obj_t *p,void (*cb)(lv_event_t *),int f,void *u){(void)f;(void)u;p->cb=cb;}
int lv_event_get_code(lv_event_t *e){return e->code;}
lv_obj_t *lv_event_get_target_obj(lv_event_t *e){return e->target;}
lv_layer_t *lv_event_get_layer(lv_event_t *e){(void)e;return &layer;}
void lv_obj_get_coords(lv_obj_t *p,lv_area_t *a){a->x1=p->x;a->y1=p->y;for(lv_obj_t *q=p->parent;q;q=q->parent){a->x1+=q->x;a->y1+=q->y;}a->x2=a->x1+p->w-1;a->y2=a->y1+p->h-1;}
void lv_obj_invalidate(lv_obj_t *p){assert(p->alive);fake_invalidations++;if(p->cb){lv_event_t e={LV_EVENT_DRAW_MAIN,p};p->cb(&e);}}
#define INIT(name,type) void name(type *d){memset(d,0,sizeof(*d));}
INIT(lv_draw_rect_dsc_init,lv_draw_rect_dsc_t)
INIT(lv_draw_line_dsc_init,lv_draw_line_dsc_t)
INIT(lv_draw_triangle_dsc_init,lv_draw_triangle_dsc_t)
INIT(lv_draw_arc_dsc_init,lv_draw_arc_dsc_t)
void lv_draw_rect(lv_layer_t *l,const lv_draw_rect_dsc_t *d,const lv_area_t *a){(void)l;assert(a->x2>=a->x1 && a->y2>=a->y1);assert(d->bg_opa);fake_draws++;}
void lv_draw_line(lv_layer_t *l,const lv_draw_line_dsc_t *d){(void)l;assert(d->width>0);fake_draws++;}
void lv_draw_triangle(lv_layer_t *l,const lv_draw_triangle_dsc_t *d){(void)l;assert(d->opa);fake_draws++;}
void lv_draw_arc(lv_layer_t *l,const lv_draw_arc_dsc_t *d){(void)l;assert(d->radius>0 && d->width>0);fake_draws++;}

lv_obj_t *fake_find_label(const char *text){
    for(unsigned i=0;i<next;i++)if(objects[i].alive && !strcmp(objects[i].text,text))return &objects[i];
    return NULL;
}

void lv_obj_set_style_transform_pivot_x(lv_obj_t *p,int v,int s){(void)p;(void)v;(void)s;}
void lv_obj_set_style_transform_pivot_y(lv_obj_t *p,int v,int s){(void)p;(void)v;(void)s;}
void lv_obj_set_style_transform_rotation(lv_obj_t *p,int v,int s){(void)s;p->rotation=v;}
int lv_obj_get_style_transform_rotation(lv_obj_t *p,int s){(void)s;return p->rotation;}

/* Compatibility for production dirty-region and capsule drawing APIs. */
void lv_obj_invalidate_area(lv_obj_t *p,const lv_area_t *area){(void)area;lv_obj_invalidate(p);}
lv_color_t lv_color_mix(lv_color_t a,lv_color_t b,uint8_t mix){
    uint32_t out=0;
    for(int shift=0;shift<=16;shift+=8)
        out|=((((a>>shift)&255)*mix+((b>>shift)&255)*(255-mix))/255)<<shift;
    return out;
}
