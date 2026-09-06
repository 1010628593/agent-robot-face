#include "bot_face_motion.h"
#include <math.h>
#include <string.h>

static float clamp(float x, float lo, float hi) { return x<lo?lo:(x>hi?hi:x); }
static float ease(float x) { x=clamp(x,0,1); return x*x*(3-2*x); }
static float mix(float a,float b,float t) { return a+(b-a)*t; }
static float wave(uint32_t t,uint32_t period) {
    return sinf((float)(t%period)*6.28318530718f/(float)period);
}
static uint32_t hash(uint32_t x) {
    x^=x>>16; x*=0x7feb352du; x^=x>>15; x*=0x846ca68bu; return x^(x>>16);
}
static float track(const uint16_t *times,const float *values,unsigned n,uint32_t t) {
    for(unsigned i=1;i<n;i++) {
        if(t<=times[i]) return mix(values[i-1],values[i],
            ease((float)(t-times[i-1])/(float)(times[i]-times[i-1])));
    }
    return values[n-1];
}
static bot_face_pose_t neutral(void) {
    return (bot_face_pose_t){.cx=233,.cy=233,.eye_w=74,.left_h=108,
        .right_h=108,.separation=126,.opacity=.88f};
}
static bot_face_pose_t blend(bot_face_pose_t a,bot_face_pose_t b,float t) {
    bot_face_pose_t r;
#define LERP(f) r.f=mix(a.f,b.f,t)
    LERP(cx); LERP(cy); LERP(eye_w); LERP(left_h); LERP(right_h);
    LERP(separation); LERP(lid); LERP(tilt); LERP(smile); LERP(cross);
    LERP(pupil); LERP(gaze_x); LERP(gaze_y); LERP(opacity);
#undef LERP
    return r;
}
static float blink(uint32_t t,uint32_t close_at) {
    if(t<close_at) return 0;
    t-=close_at;
    if(t<80) return ease((float)t/80);
    if(t<130) return 1;
    if(t<260) return 1-ease((float)(t-130)/130);
    return 0;
}
static bool can_blink(bot_face_t e) {
    return e==BOT_FACE_IDLE || e==BOT_FACE_WORKING || e==BOT_FACE_THINKING ||
           e==BOT_FACE_TOOL || e==BOT_FACE_WAITING || e==BOT_FACE_CURIOUS ||
           e==BOT_FACE_ATTENTION;
}
static bot_face_pose_t target(const bot_face_motion_t *m,uint32_t now) {
    uint32_t t=now-m->entered_ms;
    bot_face_pose_t p=neutral();
    static const uint16_t gaze_t[]={0,160,520,760,1000,1200};
    static const float gaze_v[]={0,1,1,.9f,0,0};
    float glance=track(gaze_t,gaze_v,6,t%1200);
    switch(m->expression) {
    case BOT_FACE_DISCONNECTED:
        p.left_h=76; p.right_h=64; p.lid=.16f; p.opacity=.3f; break;
    case BOT_FACE_IDLE:
        p.cx+=1.6f*wave(t,4800);p.cy+=1.1f*wave(t,6200);break;
    case BOT_FACE_BLINK: {
        float shut=blink(t%2000,880);
        p.left_h=p.right_h=mix(108,8,shut);break;
    }
    case BOT_FACE_LOOK_LEFT: case BOT_FACE_LOOK_RIGHT:
        p.gaze_x=(m->expression==BOT_FACE_LOOK_LEFT?-1:1)*.82f*glance;
        p.cx+=p.gaze_x*10;p.pupil=glance;break;
    case BOT_FACE_LOOK_UP: case BOT_FACE_LOOK_DOWN:
        p.gaze_y=(m->expression==BOT_FACE_LOOK_UP?-1:1)*.8f*glance;
        p.cy+=p.gaze_y*8;p.pupil=glance;break;
    case BOT_FACE_CURIOUS:
        p.left_h=88;p.right_h=119;p.gaze_x=.25f;p.gaze_y=-.55f;
        p.pupil=.8f;p.cy-=2*wave(t,2800);break;
    case BOT_FACE_WORKING:
        p.left_h=p.right_h=86;p.lid=.14f;p.tilt=.12f;
        p.cx+=1.4f*wave(t,1600);p.cy+=.8f*wave(t,3200);break;
    case BOT_FACE_THINKING:
        p.left_h=p.right_h=68;p.lid=.28f;p.pupil=.85f;
        p.gaze_x=.65f*wave(t,2000);p.gaze_y=-.35f;break;
    case BOT_FACE_TOOL: {
        static const uint16_t ts[]={0,180,580,850,1320,1600};
        static const float xs[]={0,-.8f,-.8f,.8f,.8f,0};
        p.left_h=p.right_h=94;p.lid=.06f;p.tilt=.04f;p.pupil=.9f;
        p.gaze_x=track(ts,xs,6,t%1600);p.cx+=p.gaze_x*7;break;
    }
    case BOT_FACE_WAITING:
        p.eye_w=77;p.left_h=116+2*wave(t,2400);p.right_h=112;
        p.cy-=1.5f*wave(t,2400);break;
    case BOT_FACE_HAPPY: case BOT_FACE_DONE: {
        static const uint16_t ts[]={0,120,300,520,1100,1600};
        static const float smiles[]={0,.3f,1,1,1,1};
        static const float bounce[]={0,2,-6,0,-1,0};
        uint32_t phase=m->expression==BOT_FACE_HAPPY?t%2400:t;
        p.smile=track(ts,smiles,6,phase);p.cy+=track(ts,bounce,6,phase);
        if(m->expression==BOT_FACE_HAPPY && phase>1800)
            p.smile=1-ease((float)(phase-1800)/600);
        break; /* DONE holds its final smile; it does not replay or become idle */
    }
    case BOT_FACE_SURPRISED:
        p.eye_w=81;p.left_h=p.right_h=128;p.pupil=1;break;
    case BOT_FACE_ERROR: {
        static const uint16_t ts[]={0,100,240,340,410,480,560};
        static const float xs[]={0,0,0,-5,5,-2,0};
        p.cross=ease((float)t/240);p.left_h=p.right_h=72;
        p.cx+=track(ts,xs,7,t);break; /* one short shake, then stable X eyes */
    }
    case BOT_FACE_SLEEP:
        p.eye_w=69;p.left_h=p.right_h=8;p.opacity=.25f;break;
    case BOT_FACE_ATTENTION:
        p.left_h=110;p.right_h=119;p.pupil=.9f;p.gaze_x=.6f;break;
    default: break;
    }
    if(can_blink(m->expression)) {
        uint32_t alive=now-m->origin_ms;
        uint32_t start=2400+hash(m->seed+alive/5200)%1700;
        float b=blink(alive%5200,start);
        p.left_h=mix(p.left_h,8,b);p.right_h=mix(p.right_h,8,b);
        p.pupil*=1-b;
    }
    return p;
}
static bot_face_pose_t base_sample(const bot_face_motion_t *m,uint32_t now) {
    bot_face_pose_t p=target(m,now);
    uint32_t age=now-m->entered_ms;
    if(m->transitioning && age<BOT_FACE_TRANSITION_MS)
        p=blend(m->from,p,ease((float)age/BOT_FACE_TRANSITION_MS));
    return p;
}
static void touch_sample(const bot_face_motion_t *m,uint32_t now,
                         float *x,float *y,float *gain) {
    float a=ease((float)(now-m->touch_ms)/(m->touch_active?100.0f:500.0f));
    *x=mix(m->touch_from_x,m->touch_x,a);
    *y=mix(m->touch_from_y,m->touch_y,a);
    *gain=mix(m->touch_from_gain,m->touch_gain,a);
}
bot_face_t bot_face_for_state(bot_state_t s) {
    switch(s) {
    case BOT_STATE_IDLE:return BOT_FACE_IDLE;
    case BOT_STATE_WORKING:return BOT_FACE_WORKING;
    case BOT_STATE_TOOL:return BOT_FACE_TOOL;
    case BOT_STATE_WAITING:return BOT_FACE_WAITING;
    case BOT_STATE_DONE:return BOT_FACE_DONE;
    case BOT_STATE_ERROR:return BOT_FACE_ERROR;
    case BOT_STATE_CANCELLED:return BOT_FACE_SLEEP;
    default:return BOT_FACE_DISCONNECTED;
    }
}
void bot_face_motion_init(bot_face_motion_t *m,uint32_t seed,uint32_t now) {
    memset(m,0,sizeof(*m));m->expression=BOT_FACE_IDLE;m->seed=seed;
    m->origin_ms=m->entered_ms=m->touch_ms=now;m->from=neutral();
}
bool bot_face_motion_set(bot_face_motion_t *m,bot_face_t e,uint32_t key,uint32_t now) {
    if(e<0 || e>=BOT_FACE_COUNT)e=BOT_FACE_DISCONNECTED;
    if(m->expression==e && m->event_key==key)return false;
    m->from=base_sample(m,now);m->expression=e;m->event_key=key;
    m->entered_ms=now;m->transitioning=true;return true;
}
void bot_face_motion_touch(bot_face_motion_t *m,bool pressed,int16_t x,
                           int16_t y,float hold,uint32_t now) {
    float tx=pressed?clamp(((float)x-233)/233,-1,1):0;
    float ty=pressed?clamp(((float)y-233)/233,-1,1):0;
    float gain=pressed?1:0;
    if(pressed) m->hold=isfinite(hold)?clamp(hold,0,1):0;
    if(pressed==m->touch_active && tx==m->touch_x && ty==m->touch_y)return;
    touch_sample(m,now,&m->touch_from_x,&m->touch_from_y,&m->touch_from_gain);
    m->touch_x=tx;m->touch_y=ty;m->touch_gain=gain;
    m->touch_ms=now;m->touch_active=pressed;
}
void bot_face_motion_sample(const bot_face_motion_t *m,uint32_t now,bot_face_pose_t *p) {
    *p=base_sample(m,now);
    float x,y,gain;touch_sample(m,now,&x,&y,&gain);
    p->cx+=x*20;p->cy+=y*14;
    p->gaze_x=mix(p->gaze_x,x,gain);p->gaze_y=mix(p->gaze_y,y,gain);
    /* Touch changes gaze, never the task's smile/cross/lid semantics. */
    p->pupil=mix(p->pupil,.8f,gain);
    p->left_h*=1-.13f*m->hold*gain;p->right_h*=1-.13f*m->hold*gain;
    p->left_h=fmaxf(p->left_h,6);p->right_h=fmaxf(p->right_h,6);
    /* Slow whole-face pixel shift, not a second animation timebase. */
    uint32_t life=now-m->origin_ms;
    p->cx+=1.2f*wave(life,240000);p->cy+=1.2f*wave(life,300000);
}
