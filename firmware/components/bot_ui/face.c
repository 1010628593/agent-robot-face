/* The display IS the face: two eyes on black, no state ring, labels or icons.
 * SIM provenance is rendered by ui.c, not concealed by this presentation.
 * One local drawing surface; no object allocation in tick/state transitions. */
#include "bot_face.h"
#include "bot_face_geometry.h"
#include "bot_ui.h"
#include <math.h>

static lv_obj_t *s_surface;
static bot_face_motion_t s_motion;
static bot_face_geometry_t s_geometry;
static bot_face_geometry_t s_next_geometry; /* Keep the bounded scratch off the LVGL task stack. */
static bool s_ready, s_have_frame, s_have_state;
static uint32_t s_frame_ms, s_revision, s_generation;
static uint8_t s_agent;
static bot_state_t s_state;

static int32_t px(float v) { return (int32_t)lroundf(v); }

static void draw_primitive(lv_layer_t *layer,const bot_face_primitive_t *p,
                           int32_t dx,int32_t dy)
{
    lv_color_t color=lv_color_hex(p->dark?0:BOT_FACE_EYE_COLOR);
    switch(p->kind) {
    case BOT_FACE_RECT: {
        lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);
        d.bg_color=color;d.bg_opa=p->opacity;d.radius=px(p->radius);
        lv_area_t a={px(p->x1)+dx,px(p->y1)+dy,px(p->x2)+dx,px(p->y2)+dy};
        lv_draw_rect(layer,&d,&a);break;
    }
    case BOT_FACE_LINE: {
        lv_draw_line_dsc_t d;lv_draw_line_dsc_init(&d);
        d.color=color;d.opa=p->opacity;d.width=px(p->width);
        d.p1=(lv_point_precise_t){px(p->x1)+dx,px(p->y1)+dy};
        d.p2=(lv_point_precise_t){px(p->x2)+dx,px(p->y2)+dy};
        d.round_start=d.round_end=1;lv_draw_line(layer,&d);break;
    }
    case BOT_FACE_TRIANGLE: {
        lv_draw_triangle_dsc_t d;lv_draw_triangle_dsc_init(&d);
        d.color=color;d.opa=p->opacity;
        d.p[0]=(lv_point_precise_t){px(p->x1)+dx,px(p->y1)+dy};
        d.p[1]=(lv_point_precise_t){px(p->x2)+dx,px(p->y2)+dy};
        d.p[2]=(lv_point_precise_t){px(p->x3)+dx,px(p->y3)+dy};
        lv_draw_triangle(layer,&d);break;
    }
    case BOT_FACE_ARC: {
        lv_draw_arc_dsc_t d;lv_draw_arc_dsc_init(&d);
        d.color=color;d.opa=p->opacity;d.width=px(p->width);
        d.center=(lv_point_t){px((p->x1+p->x2)/2)+dx,px((p->y1+p->y2)/2)+dy};
        d.radius=(uint16_t)px(p->radius);
        d.start_angle=p->start_angle;d.end_angle=p->end_angle;d.rounded=1;
        lv_draw_arc(layer,&d);break;
    }
    }
}
static void face_event(lv_event_t *e)
{
    if(lv_event_get_code(e)==LV_EVENT_DELETE) {
        if(lv_event_get_target_obj(e)==s_surface)s_surface=NULL;
        return;
    }
    if(lv_event_get_code(e)!=LV_EVENT_DRAW_MAIN)return;
    lv_area_t a;lv_obj_get_coords(lv_event_get_target_obj(e),&a);
    lv_layer_t *layer=lv_event_get_layer(e);
    for(unsigned i=0;i<s_geometry.count;i++)
        draw_primitive(layer,&s_geometry.items[i],a.x1-BOT_FACE_AREA_X,a.y1-BOT_FACE_AREA_Y);
}
static bool same_geometry(const bot_face_geometry_t *a,const bot_face_geometry_t *b)
{
    if(a->count!=b->count)return false;
    for(unsigned i=0;i<a->count;i++) {
        const bot_face_primitive_t *x=&a->items[i],*y=&b->items[i];
        if(x->kind!=y->kind || x->opacity!=y->opacity || x->dark!=y->dark)return false;
#define DIFFER(f) if(px(x->f)!=px(y->f))return false
        DIFFER(x1);DIFFER(y1);DIFFER(x2);DIFFER(y2);DIFFER(x3);DIFFER(y3);
        DIFFER(radius);DIFFER(width);DIFFER(start_angle);DIFFER(end_angle);
#undef DIFFER
    }
    return true;
}
void face_tick(uint32_t now)
{
    if(!s_ready || !s_surface)return;
    const bot_sim_agent_t *a=&g_ui.agents[g_ui.selected];
    if(!s_have_state || s_agent!=g_ui.selected || s_state!=a->state ||
       s_revision!=a->transition_id) {
        s_agent=g_ui.selected;s_state=a->state;s_revision=a->transition_id;
        s_have_state=true;
        bot_face_motion_set(&s_motion,bot_face_for_state(a->state),++s_generation,now);
    }
    uint32_t elapsed=now-s_frame_ms;
    if(s_have_frame && elapsed<BOT_FACE_FRAME_MS)return;
    s_frame_ms=now-elapsed%BOT_FACE_FRAME_MS; /* skip missed frames; never catch up in a loop */
    bot_face_pose_t pose;bot_face_motion_sample(&s_motion,now,&pose);
    bot_face_geometry_build(&pose,&s_next_geometry);
    if(!s_have_frame || !same_geometry(&s_next_geometry,&s_geometry)) {
        s_geometry=s_next_geometry;s_have_frame=true;
        lv_obj_invalidate(s_surface); /* only the local eye region, not all 466x466 */
    }
}
void face_build(lv_obj_t *screen)
{
    uint32_t now=lv_tick_get();
    if(!s_ready) {bot_face_motion_init(&s_motion,0xB07FACEu,now);s_ready=true;}
    s_surface=lv_obj_create(screen);
    lv_obj_remove_style_all(s_surface);
    lv_obj_set_pos(s_surface,BOT_FACE_AREA_X,BOT_FACE_AREA_Y);
    lv_obj_set_size(s_surface,BOT_FACE_AREA_W,BOT_FACE_AREA_H);
    lv_obj_set_style_bg_color(s_surface,lv_color_hex(0),0);
    lv_obj_set_style_bg_opa(s_surface,LV_OPA_COVER,0);
    lv_obj_remove_flag(s_surface,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_surface,face_event,LV_EVENT_ALL,NULL);
    s_have_frame=false;s_frame_ms=now-BOT_FACE_FRAME_MS;
    face_tick(now);
}
void face_touch(bool pressed,int16_t x,int16_t y,float hold,uint32_t now)
{
    if(s_ready)bot_face_motion_touch(&s_motion,pressed,x,y,hold,now);
}
void face_suspend(uint32_t now)
{
    face_touch(false,233,233,0,now);
}
