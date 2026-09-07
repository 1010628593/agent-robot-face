#include "bot_face_geometry.h"
#include <math.h>
#include <string.h>

static void add(bot_face_geometry_t *g,bot_face_primitive_t p) {
    if(p.opacity && g->count<BOT_FACE_MAX_PRIMITIVES)g->items[g->count++]=p;
}
static void eye(bot_face_geometry_t *g,const bot_face_pose_t *p,int side) {
    float cx=p->cx+(float)side*p->separation/2*cosf(p->roll);
    float cy=p->cy+(float)side*p->separation/2*sinf(p->roll);
    float h=side<0?p->left_h:p->right_h;
    float topology=fmaxf(p->smile,p->cross);
    float closed=fminf(1,topology*2);
    h=h+(8-h)*closed;
    float x=cx-p->eye_w/2,y=cy-h/2,w=p->eye_w;
    /* Collapse open eyes to an opaque closed line before changing topology. */
    if(topology<=.5f) {
        add(g,(bot_face_primitive_t){.kind=BOT_FACE_RECT,.x1=x,.y1=y,
            .x2=x+w,.y2=y+h,.radius=fminf(w/2,h/2),.opacity=255});
        /* Pupils are opaque; closed-eye occlusion is geometric, never a fade.
         * Keep the circle inside the pill at every open height. */
        if(h>16) {
            float spiral=fmaxf(0,fminf(1,p->spiral));
            float radius=fmaxf(2,fminf(w,h)/2-6);
            float pr=fminf(9,(h-8)*.22f);
            float extent=fmaxf(pr,radius*spiral);
            float gaze=fmaxf(-1,fminf(1,p->gaze_x-side*p->convergence));
            float px=cx+gaze*fmaxf(0,w/2-extent-5);
            float py=cy+fmaxf(-1,fminf(1,p->gaze_y))*fmaxf(0,h/2-extent-5);
            float dot_radius=pr*(1-spiral);
            if(dot_radius>.5f)
                add(g,(bot_face_primitive_t){.kind=BOT_FACE_RECT,.x1=px-dot_radius,.y1=py-dot_radius,
                    .x2=px+dot_radius,.y2=py+dot_radius,.radius=dot_radius,.opacity=255,.dark=true});
            /* Two-turn Archimedean spiral, mirrored rotation in each eye.
             * Inscribed circle + stroke margin keeps every segment in the pill.
             * 40 segments per eye fits the fixed 96-primitive budget. */
            if(spiral>.025f) {
                float phase=p->spiral_phase*side;
                float previous_x=px,previous_y=py;
                for(unsigned i=1;i<=40;i++) {
                    float t=(float)i/40;
                    float angle=phase+side*t*12.5663706f;
                    float r=radius*t*spiral;
                    float next_x=px+r*cosf(angle),next_y=py+r*sinf(angle);
                    add(g,(bot_face_primitive_t){.kind=BOT_FACE_LINE,
                        .x1=previous_x,.y1=previous_y,.x2=next_x,.y2=next_y,
                        .width=2+2*spiral,.opacity=255,.dark=true});
                    previous_x=next_x;previous_y=next_y;
                }
            }
        }
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
    if(p->smile>.5f && p->smile>=p->cross) {
        /* An opaque closed line bends continuously into a contented arch. */
        float gain=p->smile*2-1;
        for(unsigned i=0;i<8;i++) {
            float u=(float)i/8,v=(float)(i+1)/8;
            add(g,(bot_face_primitive_t){.kind=BOT_FACE_LINE,
                .x1=cx+(u-.5f)*(w-8),.y1=cy-18*sinf(u*3.14159265f)*gain,
                .x2=cx+(v-.5f)*(w-8),.y2=cy-18*sinf(v*3.14159265f)*gain,
                .width=8+4*gain,.opacity=255});
        }
    }

    if(p->cross>.5f && p->cross>p->smile) {
        float d=(w-8)/2+(22-(w-8)/2)*(p->cross*2-1);
        float dy=22*(p->cross*2-1);
        add(g,(bot_face_primitive_t){.kind=BOT_FACE_LINE,
            .x1=cx-d,.y1=cy-dy,.x2=cx+d,.y2=cy+dy,
            .width=8+4*(p->cross*2-1),.opacity=255});
        add(g,(bot_face_primitive_t){.kind=BOT_FACE_LINE,
            .x1=cx-d,.y1=cy+dy,.x2=cx+d,.y2=cy-dy,
            .width=8+4*(p->cross*2-1),.opacity=255});
    }
}
void bot_face_geometry_build(const bot_face_pose_t *pose,bot_face_geometry_t *out) {
    memset(out,0,sizeof(*out));eye(out,pose,-1);eye(out,pose,1);
}
