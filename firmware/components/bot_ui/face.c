/* Eye-only Face. Each Agent owns a clock; one surface draws the selected one.
 * UI-owner context only. No hardware, model, permission or account writes. */
#include "bot_face.h"
#include "bot_face_geometry.h"
#include "bot_face_reaction.h"
#include "bot_ui.h"
#include <math.h>

typedef struct {
    bot_face_motion_t motion;
    bot_state_t state;
    uint32_t revision, generation;
    bool initialized;
} face_track_t;
static face_track_t s_tracks[BOT_AGENT_COUNT];
static lv_obj_t *s_surface;
static bot_face_geometry_t s_geometry, s_next_geometry;
static bool s_have_frame;
static uint32_t s_frame_ms;
static uint8_t s_surface_agent;
static bot_motion_view_t s_environment;
static uint32_t s_suppressed_event;
static float s_rotation, s_contact_rotation;
static bool s_contact;
static bool environment_fresh(uint32_t now) {
    return s_environment.available && now-s_environment.sampled_ms<=BOT_MOTION_STALE_MS;
}
void face_set_motion(const bot_motion_view_t *v) {
    if(!v) {s_environment.available=false;return;}
    s_environment=*v;
    if(!isfinite(v->rotation_deg) || v->reaction>BOT_REACTION_DIZZY) {
        s_environment.available=false;s_environment.reaction=BOT_REACTION_NONE;
    }
}
void face_map_input(bool pressed,int16_t x,int16_t y,int16_t *ox,int16_t *oy) {
    if(pressed && !s_contact)s_contact_rotation=s_rotation;
    /* Use the same frozen matrix for DOWN, MOVE and final UP. */
    bot_motion_unrotate(s_contact?s_contact_rotation:s_rotation,x,y,ox,oy);
    s_contact=pressed;
}
static int32_t px(float v) { return (int32_t)lroundf(v); }

void face_sync(uint32_t now)
{
    if(g_ui.selected>=BOT_AGENT_COUNT || g_ui.screen!=BOT_SCR_FACE ||
       !bot_face_reaction_allowed(g_ui.agents[g_ui.selected].state) || !environment_fresh(now))
        s_suppressed_event=s_environment.event_id;
    for (unsigned i=0;i<BOT_AGENT_COUNT;i++) {
        face_track_t *t=&s_tracks[i];
        const bot_sim_agent_t *a=&g_ui.agents[i];
        bool fresh=!t->initialized;
        if (fresh) {
            bot_face_motion_init(&t->motion,0xB07FACEu+i*0x9E3779B9u,now);
            t->initialized=true;
        }
        if (fresh || t->state!=a->state || t->revision!=a->transition_id) {
            t->state=a->state;t->revision=a->transition_id;
            bot_face_motion_set(&t->motion,bot_face_for_state(a->state),++t->generation,now);
        }
    }
}
static void draw_primitive(lv_layer_t *layer,const bot_face_primitive_t *p,int32_t dx,int32_t dy)
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
        if(lv_event_get_target_obj(e)==s_surface) {
            face_suspend(lv_tick_get());
            s_surface=NULL;
        }
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
    face_sync(now);
    if(!s_surface || g_ui.selected>=BOT_AGENT_COUNT)return;
    if(s_surface_agent!=g_ui.selected) {
        face_suspend(now);
        s_surface_agent=g_ui.selected;
        s_have_frame=false;
    }
    uint32_t elapsed=now-s_frame_ms;
    if(s_have_frame && elapsed<BOT_FACE_FRAME_MS)return;
    s_frame_ms=now-elapsed%BOT_FACE_FRAME_MS;
    bot_face_pose_t pose;
    bot_face_motion_sample(&s_tracks[s_surface_agent].motion,now,&pose);
    if(s_environment.event_id!=s_suppressed_event)
        bot_face_reaction_apply(&s_environment,g_ui.agents[s_surface_agent].state,now,&pose);
    /* LVGL transforms only the bounded face surface; root/text pages are not
     * continuously rotated. Its invalidation includes old/new transformed bounds. */
    if(!s_contact && environment_fresh(now) && s_environment.orientation_valid) {
        float delta=bot_motion_wrap(s_environment.rotation_deg-s_rotation);
        float dt=fminf((float)elapsed/1000,.05f),limit=240*dt;
        s_rotation=bot_motion_wrap(s_rotation+fmaxf(-limit,fminf(limit,delta)));
    }
    int32_t angle=(int32_t)lroundf(s_rotation*10);
    if(angle<0)angle+=3600;
    if(lv_obj_get_style_transform_rotation(s_surface,0)!=angle)
        lv_obj_set_style_transform_rotation(s_surface,angle,0);
    bot_face_geometry_build(&pose,&s_next_geometry);
    if(!s_have_frame || !same_geometry(&s_next_geometry,&s_geometry)) {
        s_geometry=s_next_geometry;s_have_frame=true;
        lv_obj_invalidate(s_surface);
    }
}
void face_build(lv_obj_t *screen)
{
    uint32_t now=lv_tick_get();
    face_sync(now);
    s_surface_agent=g_ui.selected<BOT_AGENT_COUNT?g_ui.selected:BOT_AGENT_CODEX;
    s_surface=lv_obj_create(screen);
    lv_obj_remove_style_all(s_surface);
    lv_obj_set_pos(s_surface,BOT_FACE_AREA_X,BOT_FACE_AREA_Y);
    lv_obj_set_size(s_surface,BOT_FACE_AREA_W,BOT_FACE_AREA_H);
    lv_obj_set_style_transform_pivot_x(s_surface,233-BOT_FACE_AREA_X,0);
    lv_obj_set_style_transform_pivot_y(s_surface,233-BOT_FACE_AREA_Y,0);
    int32_t angle=(int32_t)lroundf(s_rotation*10);if(angle<0)angle+=3600;
    lv_obj_set_style_transform_rotation(s_surface,angle,0);
    lv_obj_set_style_bg_color(s_surface,lv_color_hex(0),0);
    lv_obj_set_style_bg_opa(s_surface,LV_OPA_COVER,0);
    lv_obj_remove_flag(s_surface,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_surface,face_event,LV_EVENT_ALL,NULL);
    s_have_frame=false;s_frame_ms=now-BOT_FACE_FRAME_MS;
    face_tick(now);
}
void face_touch(bool pressed,int16_t x,int16_t y,float hold,uint32_t now)
{
    if(s_surface && s_surface_agent<BOT_AGENT_COUNT && s_tracks[s_surface_agent].initialized)
        bot_face_motion_touch(&s_tracks[s_surface_agent].motion,pressed,x,y,hold,now);
}
void face_suspend(uint32_t now)
{
    face_touch(false,233,233,0,now);
    s_contact=false;
}
