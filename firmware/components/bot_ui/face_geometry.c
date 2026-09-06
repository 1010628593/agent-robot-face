#include "bot_face_geometry.h"
#include <math.h>
#include <string.h>

static uint8_t alpha(float a) {
    return (uint8_t)lroundf(fmaxf(0,fminf(1,a))*255);
}
static void add(bot_face_geometry_t *g,bot_face_primitive_t p) {
    if(p.opacity && g->count<BOT_FACE_MAX_PRIMITIVES)g->items[g->count++]=p;
}
static void eye(bot_face_geometry_t *g,const bot_face_pose_t *p,int side) {
    float cx=p->cx+(float)side*p->separation/2;
    float h=side<0?p->left_h:p->right_h;
    float x=cx-p->eye_w/2,y=p->cy-h/2,w=p->eye_w;
    float pill=(1-p->smile)*(1-p->cross)*p->opacity;
    float smile=p->smile*(1-p->cross)*p->opacity;
    if(alpha(pill)) {
        add(g,(bot_face_primitive_t){.kind=BOT_FACE_RECT,.x1=x,.y1=y,
            .x2=x+w,.y2=y+h,.radius=fminf(w/2,h/2),.opacity=alpha(pill)});
        /* Both pupils share a gaze vector; never an accidental cross-eyed face.
         * Fade them out when eyelids are too small to contain the circle. */
        float pr=fminf(9,h*.14f);
        float px=cx+p->gaze_x*(w/2-pr-5);
        float py=p->cy+p->gaze_y*(h/2-pr-5);
        float pupil_fade=fmaxf(0,fminf(1,(h-16)/24));
        add(g,(bot_face_primitive_t){.kind=BOT_FACE_RECT,.x1=px-pr,.y1=py-pr,
            .x2=px+pr,.y2=py+pr,.radius=pr,
            .opacity=alpha(p->pupil*pupil_fade),.dark=true});
        /* An opaque black upper lid gives true sloped focused eyes, not
         * merely shorter pills. It only masks this eye's footprint. */
        float l=fmaxf(0,p->lid+side*p->tilt)*h;
        float r=fmaxf(0,p->lid-side*p->tilt)*h;
        if(l>.1f || r>.1f) {
            add(g,(bot_face_primitive_t){.kind=BOT_FACE_TRIANGLE,
                .x1=x-1,.y1=y-1,.x2=x+w+1,.y2=y-1,.x3=x+w+1,.y3=y+r,
                .opacity=255,.dark=true});
            add(g,(bot_face_primitive_t){.kind=BOT_FACE_TRIANGLE,
                .x1=x-1,.y1=y-1,.x2=x+w+1,.y2=y+r,.x3=x-1,.y3=y+l,
                .opacity=255,.dark=true});
        }
    }
    if(alpha(smile)) {
        /* 210..330 degrees is an upward arch (LVGL: zero at 3 o'clock). */
        float radius=w*.49f;
        add(g,(bot_face_primitive_t){.kind=BOT_FACE_ARC,
            .x1=cx-radius,.y1=p->cy+15-radius,
            .x2=cx+radius,.y2=p->cy+15+radius,
            .radius=radius,.width=12,.start_angle=210,.end_angle=330,
            .opacity=alpha(smile)});
    }
    float cross=p->cross*p->opacity;
    if(alpha(cross)) {
        const float d=22;
        add(g,(bot_face_primitive_t){.kind=BOT_FACE_LINE,
            .x1=cx-d,.y1=p->cy-d,.x2=cx+d,.y2=p->cy+d,
            .width=12,.opacity=alpha(cross)});
        add(g,(bot_face_primitive_t){.kind=BOT_FACE_LINE,
            .x1=cx-d,.y1=p->cy+d,.x2=cx+d,.y2=p->cy-d,
            .width=12,.opacity=alpha(cross)});
    }
}
void bot_face_geometry_build(const bot_face_pose_t *pose,bot_face_geometry_t *out) {
    memset(out,0,sizeof(*out));eye(out,pose,-1);eye(out,pose,1);
}
