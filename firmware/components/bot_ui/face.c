/* Eye-only Face. Each Agent owns a clock; one surface draws the selected one.
 * UI-owner context only. No hardware, model, permission or account writes. */
#include "bot_face.h"
#include "bot_face_geometry.h"
#include "bot_face_reaction.h"
#include "bot_ui.h"
#include <math.h>
#include <string.h>
#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#include "driver/usb_serial_jtag.h"
#include <stdio.h>
#endif

typedef struct {
    bot_face_motion_t motion;
    bot_state_t state;
    uint32_t revision, generation;
    bool initialized;
} face_track_t;
void face_audio_sync(uint32_t now,bool imu_active);
void face_audio_cancel(void);
bool face_audio_apply(uint32_t now,bot_face_pose_t *pose);
static bool imu_active(uint32_t now);
static face_track_t s_tracks[BOT_AGENT_COUNT];
static lv_obj_t *s_surface,*s_audio_feedback;
const char *face_audio_feedback(uint32_t now);
static bot_face_geometry_t s_geometry, s_next_geometry;
static bool s_have_frame;
static uint32_t s_frame_ms;
static uint8_t s_surface_agent;
static bot_motion_view_t s_environment;
static uint32_t s_suppressed_event;
static float s_rotation, s_contact_rotation;
static bool s_contact;
static bot_face_inertia_t s_inertia;
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

static bool imu_active(uint32_t now) {
    uint32_t age=now-s_environment.event_ms;
    return environment_fresh(now)&&s_environment.event_id!=s_suppressed_event&&
        ((s_environment.reaction==BOT_REACTION_DIZZY&&age<BOT_MOTION_DIZZY_MS)||
         (s_environment.reaction==BOT_REACTION_ATTENTION&&age<600)||
         (s_environment.reaction==BOT_REACTION_SETTLE&&age<450));
}
void face_sync(uint32_t now)
{
    face_audio_sync(now,imu_active(now));
    if(g_ui.selected>=BOT_AGENT_COUNT || g_ui.screen!=BOT_SCR_FACE ||
       !bot_face_reaction_allowed(g_ui.agents[g_ui.selected].state) || !environment_fresh(now))
        s_suppressed_event=s_environment.event_id;
    for (unsigned i=0;i<BOT_AGENT_COUNT;i++) {
        face_track_t *t=&s_tracks[i];
        const bot_ui_agent_view_t *a=&g_ui.agents[i];
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
/* Rotate geometry once, before rasterization. No intermediate RGB bitmap or
 * transformed object layer: radii and stroke widths remain rotation invariant. */
static lv_point_precise_t rotated(float x,float y,float c,float sn,int32_t dx,int32_t dy)
{
    x-=233;y-=233;
    return (lv_point_precise_t){px(233+c*x-sn*y)+dx,px(233+sn*x+c*y)+dy};
}
static void draw_primitive(lv_layer_t *layer,const bot_face_primitive_t *p,
                           int32_t dx,int32_t dy,float c,float sn)
{
    lv_color_t color=lv_color_hex(p->dark?0:BOT_FACE_EYE_COLOR);
    switch(p->kind) {
    case BOT_FACE_RECT: {
        /* Geometry owns occlusion; every eye and pupil paint is opaque. */
        uint8_t opacity=LV_OPA_COVER;
        /* Geometry emits capsules (radius=min(w,h)/2), including circular pupils. */
        float w=p->x2-p->x1,h=p->y2-p->y1;
        float cx=(p->x1+p->x2)/2,cy=(p->y1+p->y2)/2;
        float r=fminf(w,h)/2;
        float vx=w>h?(w-h)/2:0,vy=h>w?(h-w)/2:0;
        lv_point_precise_t a=rotated(cx-vx,cy-vy,c,sn,dx,dy);
        lv_point_precise_t b=rotated(cx+vx,cy+vy,c,sn,dx,dy);
        if(a.x==b.x && a.y==b.y) {
            lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);
            d.bg_color=color;d.bg_opa=opacity;d.radius=LV_RADIUS_CIRCLE;
            int32_t diameter=px(2*r)+1,half=diameter/2;
            lv_area_t area={a.x-half,a.y-half,a.x-half+diameter-1,a.y-half+diameter-1};
            lv_draw_rect(layer,&d,&area);
        } else {
            lv_draw_line_dsc_t d;lv_draw_line_dsc_init(&d);
            d.color=color;d.opa=opacity;d.width=px(2*r)+1;
            d.p1=a;d.p2=b;d.round_start=d.round_end=1;
            lv_draw_line(layer,&d);
        }
        break;
    }
    case BOT_FACE_LINE: {
        lv_draw_line_dsc_t d;lv_draw_line_dsc_init(&d);
        d.color=color;d.opa=p->opacity;d.width=px(p->width);
        d.p1=rotated(p->x1,p->y1,c,sn,dx,dy);
        d.p2=rotated(p->x2,p->y2,c,sn,dx,dy);
        d.round_start=d.round_end=1;lv_draw_line(layer,&d);break;
    }
    case BOT_FACE_TRIANGLE: {
        lv_draw_triangle_dsc_t d;lv_draw_triangle_dsc_init(&d);
        d.color=color;d.opa=p->opacity;
        d.p[0]=rotated(p->x1,p->y1,c,sn,dx,dy);
        d.p[1]=rotated(p->x2,p->y2,c,sn,dx,dy);
        d.p[2]=rotated(p->x3,p->y3,c,sn,dx,dy);
        lv_draw_triangle(layer,&d);break;
    }
    case BOT_FACE_ARC: {
        lv_draw_arc_dsc_t d;lv_draw_arc_dsc_init(&d);
        d.color=color;d.opa=p->opacity;d.width=px(p->width);
        lv_point_precise_t center=rotated((p->x1+p->x2)/2,(p->y1+p->y2)/2,c,sn,dx,dy);
        d.center=(lv_point_t){center.x,center.y};d.radius=(uint16_t)px(p->radius);
        d.start_angle=(px(p->start_angle+s_rotation)%360+360)%360;
        d.end_angle=(px(p->end_angle+s_rotation)%360+360)%360;
        d.rounded=1;lv_draw_arc(layer,&d);break;
    }
    }
}
static void face_event(lv_event_t *e)
{
    if(lv_event_get_code(e)==LV_EVENT_DELETE) {
        if(lv_event_get_target_obj(e)==s_surface) {
            face_suspend(lv_tick_get());
            s_surface=NULL;s_audio_feedback=NULL;
        }
        return;
    }
    if(lv_event_get_code(e)!=LV_EVENT_DRAW_MAIN)return;
    lv_area_t a;lv_obj_get_coords(lv_event_get_target_obj(e),&a);
    lv_layer_t *layer=lv_event_get_layer(e);
    float radians=s_rotation*0.017453292519943295f,c=cosf(radians),sn=sinf(radians);
    for(unsigned i=0;i<s_geometry.count;i++)
        draw_primitive(layer,&s_geometry.items[i],a.x1-30,a.y1-30,c,sn);
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
/* Conservative screen-space bounds, including antialias and stroke caps.
 * Invalidate old and new pixels instead of the entire circumscribed square. */
static bool geometry_bounds(const bot_face_geometry_t *g,float angle,lv_area_t *out)
{
    if(!g->count)return false;
    float rad=angle*.017453292519943295f,c=cosf(rad),sn=sinf(rad);
    *out=(lv_area_t){466,466,0,0};
    for(unsigned i=0;i<g->count;i++) {
        const bot_face_primitive_t *p=&g->items[i];
        float x1=fminf(p->x1,p->x2),y1=fminf(p->y1,p->y2);
        float x2=fmaxf(p->x1,p->x2),y2=fmaxf(p->y1,p->y2);
        if(p->kind==BOT_FACE_TRIANGLE) {
            x1=fminf(x1,p->x3);y1=fminf(y1,p->y3);
            x2=fmaxf(x2,p->x3);y2=fmaxf(y2,p->y3);
        }
        float pad=(p->kind==BOT_FACE_LINE || p->kind==BOT_FACE_ARC)?p->width/2+3:3;
        x1-=pad;y1-=pad;x2+=pad;y2+=pad;
        for(unsigned corner=0;corner<4;corner++) {
            lv_point_precise_t q=rotated(corner&1?x2:x1,corner&2?y2:y1,c,sn,0,0);
            if(q.x<out->x1)out->x1=q.x;
            if(q.x>out->x2)out->x2=q.x;
            if(q.y<out->y1)out->y1=q.y;
            if(q.y>out->y2)out->y2=q.y;
        }
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
    const char *feedback=face_audio_feedback(now);
    if(s_audio_feedback){
        if(feedback){if(strcmp(lv_label_get_text(s_audio_feedback),feedback))lv_label_set_text(s_audio_feedback,feedback);lv_obj_remove_flag(s_audio_feedback,LV_OBJ_FLAG_HIDDEN);}
        else lv_obj_add_flag(s_audio_feedback,LV_OBJ_FLAG_HIDDEN);
    }
    uint32_t elapsed=now-s_frame_ms;
    if(s_have_frame && elapsed<BOT_FACE_FRAME_MS)return;
    s_frame_ms=now-elapsed%BOT_FACE_FRAME_MS;
    bot_face_pose_t pose;
    bot_face_motion_sample(&s_tracks[s_surface_agent].motion,now,&pose);
    if(!s_contact && s_environment.event_id!=s_suppressed_event)
        bot_face_reaction_apply(&s_environment,g_ui.agents[s_surface_agent].state,now,&pose);
    float previous_rotation=s_rotation;
    if(!s_contact && environment_fresh(now) && s_environment.orientation_valid) {
        float delta=bot_motion_wrap(s_environment.rotation_deg-s_rotation);
        float dt=fminf((float)elapsed/1000,.25f),limit=720*dt;
        s_rotation=bot_motion_wrap(s_rotation+fmaxf(-limit,fminf(limit,delta)));
    }
    bool audio_active=face_audio_apply(now,&pose);
    bot_face_inertia_apply(&s_inertia,&s_environment,s_rotation,
        !s_contact && !audio_active && !imu_active(now) && bot_face_reaction_allowed(g_ui.agents[s_surface_agent].state),now,&pose);
#if defined(ESP_PLATFORM) && defined(CONFIG_BOT_IMU_DIAGNOSTICS)
    static uint32_t motion_log_ms;
    if(now-motion_log_ms>=250) {
        motion_log_ms=now;char line[256];
        int n=snprintf(line,sizeof(line),"@motion {\"ms\":%lu,\"angle\":%.2f,\"linear_g\":%.3f,\"amplitude\":%.3f,\"grade\":%u,\"fatigue\":%.3f,\"spiral\":%.3f,\"fresh\":%d,\"bias\":%d}\n",
            (unsigned long)now,(double)s_environment.rotation_deg,(double)s_environment.linear_g,
            (double)s_inertia.amplitude,s_inertia.grade,(double)s_inertia.fatigue,
            (double)pose.spiral,environment_fresh(now),s_environment.gyro_calibrated);
        if(n>0 && n<(int)sizeof(line))usb_serial_jtag_write_bytes(line,n,0);
    }
#endif
    bot_face_geometry_build(&pose,&s_next_geometry);
    if(!s_have_frame || previous_rotation!=s_rotation || !same_geometry(&s_next_geometry,&s_geometry)) {
        lv_area_t old_area,new_area;
        bool had_area=s_have_frame && geometry_bounds(&s_geometry,previous_rotation,&old_area);
        bool has_area=geometry_bounds(&s_next_geometry,s_rotation,&new_area);
        if(!s_have_frame)lv_obj_invalidate(s_surface);
        else {
            if(had_area)lv_obj_invalidate_area(s_surface,&old_area);
            if(has_area)lv_obj_invalidate_area(s_surface,&new_area);
        }
        s_geometry=s_next_geometry;s_have_frame=true;
    }
}
void face_build(lv_obj_t *screen)
{
    uint32_t now=lv_tick_get();
    face_sync(now);
    s_surface_agent=g_ui.selected<BOT_AGENT_COUNT?g_ui.selected:BOT_AGENT_CODEX;
    s_surface=lv_obj_create(screen);
    lv_obj_remove_style_all(s_surface);
    /* Circumscribed square of the original 322x244 drawing bounds, plus AA
     * margin. Clear old and new geometry even during a large angle change. */
    lv_obj_set_pos(s_surface,30,30);
    lv_obj_set_size(s_surface,406,406);
    lv_obj_set_style_bg_color(s_surface,lv_color_hex(0),0);
    lv_obj_set_style_bg_opa(s_surface,LV_OPA_COVER,0);
    lv_obj_remove_flag(s_surface,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_surface,face_event,LV_EVENT_ALL,NULL);
    s_audio_feedback=lv_label_create(s_surface);
    lv_obj_set_pos(s_audio_feedback,53,320);lv_obj_set_size(s_audio_feedback,300,32);
    lv_obj_set_style_text_font(s_audio_feedback,&bot_font_22,0);
    lv_obj_set_style_text_color(s_audio_feedback,lv_color_hex(0x8d9199),0);
    lv_obj_set_style_text_align(s_audio_feedback,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_remove_flag(s_audio_feedback,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_audio_feedback,LV_OBJ_FLAG_HIDDEN);
    s_have_frame=false;s_frame_ms=now-BOT_FACE_FRAME_MS;
    face_tick(now);
}
void face_touch(bool pressed,int16_t x,int16_t y,float hold,uint32_t now)
{
    if(s_surface && s_surface_agent<BOT_AGENT_COUNT && s_tracks[s_surface_agent].initialized)
        bot_face_motion_touch(&s_tracks[s_surface_agent].motion,pressed,x,y,hold,now);
}
void face_interact(const bot_touch_frame_t *frame,float comfort,uint32_t held_ms)
{
    if(s_surface && s_surface_agent<BOT_AGENT_COUNT)
        bot_face_motion_interact(&s_tracks[s_surface_agent].motion,frame,comfort,held_ms);
}
void face_poke(int16_t x,int16_t y,uint32_t now)
{
    if(s_surface && s_surface_agent<BOT_AGENT_COUNT)
        bot_face_motion_poke(&s_tracks[s_surface_agent].motion,x,y,now);
}
void face_pet(bool stroke,uint32_t now)
{
    if(s_surface && s_surface_agent<BOT_AGENT_COUNT && bot_face_reaction_allowed(g_ui.agents[s_surface_agent].state))
        bot_face_motion_pet(&s_tracks[s_surface_agent].motion,stroke,now);
}
void face_suspend(uint32_t now)
{
    face_audio_cancel();
    s_inertia=(bot_face_inertia_t){0};
    face_touch(false,233,233,0,now);
    if(s_surface_agent<BOT_AGENT_COUNT)bot_face_motion_clear_touch(&s_tracks[s_surface_agent].motion,now);
    s_contact=false;
}

void face_touch_debug(bot_face_touch_debug_t *out,uint32_t now)
{
    *out=(bot_face_touch_debug_t){.expression=-1};
    if(!s_surface || s_surface_agent>=BOT_AGENT_COUNT)return;
    bot_face_motion_t *m=&s_tracks[s_surface_agent].motion;
    bot_face_pose_t p;bot_face_motion_sample(m,now,&p);
    *out=(bot_face_touch_debug_t){.expression=m->expression,.count=m->touch_count,
        .tapped=m->tapped,.x=m->touch_x,.y=m->touch_y,.left=p.left_h,.right=p.right_h,
        .revision=s_tracks[s_surface_agent].generation};
}
