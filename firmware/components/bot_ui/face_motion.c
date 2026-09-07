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
        .right_h=108,.separation=126,.opacity=1,.pupil=1};
}
static bot_face_pose_t blend(bot_face_pose_t a,bot_face_pose_t b,float t) {
    bot_face_pose_t r;
#define LERP(f) r.f=mix(a.f,b.f,t)
    LERP(cx); LERP(cy); LERP(eye_w); LERP(left_h); LERP(right_h);
    LERP(separation); LERP(lid); LERP(tilt); LERP(smile); LERP(cross);
    LERP(pupil); LERP(gaze_x); LERP(gaze_y); LERP(opacity); LERP(convergence); LERP(spiral); LERP(spiral_phase); LERP(roll);
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
        p.left_h=76; p.right_h=64; p.lid=.16f; break;
    case BOT_FACE_IDLE:
        p.cx+=1.6f*wave(t,4800);p.cy+=1.1f*wave(t,6200);
        if(t%11000>7600) { float a=sinf((float)(t%11000-7600)*3.14159265f/3400);p.gaze_x=.4f*a;p.right_h+=4*a; }
        break;
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
    case BOT_FACE_CANCELLED:
        p.eye_w=68;p.left_h=48;p.right_h=58;p.lid=.18f;p.gaze_y=.65f;break;
    case BOT_FACE_SLEEP:
        p.eye_w=69;p.left_h=p.right_h=8;break;
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
    p.pupil=1;p.opacity=1;
    return p;
}
static bot_face_pose_t base_sample(const bot_face_motion_t *m,uint32_t now) {
    bot_face_pose_t p=target(m,now);
    uint32_t age=now-m->entered_ms;
    if(m->transitioning && age<BOT_FACE_TRANSITION_MS)
        p=blend(m->from,p,ease((float)age/BOT_FACE_TRANSITION_MS));
    return p;
}
static bool playful(bot_face_t e) { return e==BOT_FACE_IDLE || e==BOT_FACE_WORKING || e==BOT_FACE_TOOL; }
static void poke_lids(const bot_face_motion_t *m,uint32_t now,float lids[2]) {
    float age=(float)(now-m->tap_ms);
    for(unsigned i=0;i<2;i++) {
        /* An eye tap always closes its eye. Elsewhere the first poke is
         * attention; the second brings a blink, the third adds avoidance. */
        float target=m->tap_streak>=3 || m->tap_region==i+1 || (m->tap_region>2 && m->tap_streak>=2)?1:0;
        bool nearest=(m->tap_x<233)==(i==0);
        /* After teasing, cautiously peek with the eye nearest the hand,
         * then let the other reopen. This is distinct from a normal blink. */
        float hold=m->tap_streak>=3?(nearest?230:390):115;
        float opening=m->tap_streak>=3?300:170;
        lids[i]=m->tapped?mix(m->tap_closed_from[i],target,ease(age/65))*(1-ease((age-hold)/opening)):0;
    }
}
static void body_sample(const bot_face_motion_t *m,uint32_t now,float p[2],float v[2]) {
    float age=(float)(now-m->body_ms);
    if(!m->touch_active && m->body_ms==m->released_ms)age=fmaxf(0,age-m->release_hold_ms);
    float t=fminf(age/1000,2);
    /* One soft overshoot, then settle. Preserve velocity when retargeting. */
    const float decay=11,frequency=12;
    float envelope=expf(-decay*t),c=cosf(frequency*t),s=sinf(frequency*t);
    for(unsigned i=0;i<2;i++) {
        float a=m->body[i]-m->body_target[i];
        float b=(m->body_velocity[i]+decay*a)/frequency;
        p[i]=m->body_target[i]+envelope*(a*c+b*s);
        v[i]=envelope*((b*frequency-decay*a)*c-(a*frequency+decay*b)*s);
    }
}
static void interaction_sample(const bot_face_motion_t *m,uint32_t now,float channels[5]) {
    float age=(float)(now-m->interaction_ms);
    float t=m->touch_count?1-expf(-age/30):ease(age/500);
    for(unsigned i=0;i<5;i++) {
        float progress=t;
        if(!m->touch_count && i==3)progress=ease((age-160)/500);
        channels[i]=mix(m->interaction_from[i],m->interaction[i],progress);
    }
    /* Shape springs back once; eyelids and comfort only ease open. */
    if(!m->touch_count)channels[2]=m->interaction_from[2]*(1-t)*cosf(age*.012f);
}
static void touch_sample(const bot_face_motion_t *m,uint32_t now,
                         float *x,float *y,float *gain) {
    /* Moving targets must not restart a zero-slope ease every input frame.
     * Exponential following has the same response at 10/20/40 ms sampling. */
    float age=(float)(now-m->touch_ms);
    /* Keep looking where the hand was briefly after it lets go. */
    float a=m->touch_active?1-expf(-age/35.0f):ease((age-100)/500.0f);
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
    case BOT_STATE_CANCELLED:return BOT_FACE_CANCELLED;
    default:return BOT_FACE_DISCONNECTED;
    }
}
void bot_face_motion_init(bot_face_motion_t *m,uint32_t seed,uint32_t now) {
    memset(m,0,sizeof(*m));m->expression=BOT_FACE_IDLE;m->seed=seed;
    m->origin_ms=m->entered_ms=m->touch_ms=now;m->from=neutral();
    m->last_tap=m->last_stroke=255;
}
bool bot_face_motion_set(bot_face_motion_t *m,bot_face_t e,uint32_t key,uint32_t now) {
    if(e<0 || e>=BOT_FACE_COUNT)e=BOT_FACE_DISCONNECTED;
    if(m->expression==e && m->event_key==key)return false;
    m->from=base_sample(m,now);m->expression=e;m->event_key=key;
    m->entered_ms=now;m->transitioning=true;
    bot_face_motion_clear_touch(m,now);
    return true;
}
void bot_face_motion_touch(bot_face_motion_t *m,bool pressed,int16_t x,
                           int16_t y,float hold,uint32_t now) {
    float tx=pressed?clamp(((float)x-233)/233,-1,1):0;
    float ty=pressed?clamp(((float)y-233)/233,-1,1):0;
    float gain=pressed?1:0;
    if(pressed) m->hold=isfinite(hold)?clamp(hold,0,1):0;
    if(pressed==m->touch_active && tx==m->touch_x && ty==m->touch_y)return;
    body_sample(m,now,m->body,m->body_velocity);
    /* Soft boundary: approach the rim without clipping or a hard stop. */
    m->body_target[0]=pressed?tanhf(tx*1.6f):0;
    m->body_target[1]=pressed?tanhf(ty*1.6f):0;
    m->body_ms=now;
    if(pressed && !m->touch_active) {
        m->contact_ms=now;m->release_hold_ms=0;
        if(!m->tapped || now-m->last_tap_ms>450)m->attention_ms=now;
    }
    if(!pressed && m->touch_active) {
        m->released_ms=now;
        m->release_hold_ms=now-m->contact_ms>=300 && !m->tapped?150:0;
    }
    touch_sample(m,now,&m->touch_from_x,&m->touch_from_y,&m->touch_from_gain);
    m->touch_x=tx;m->touch_y=ty;m->touch_gain=gain;
    m->touch_ms=now;m->touch_active=pressed;
}
void bot_face_motion_sample(const bot_face_motion_t *m,uint32_t now,bot_face_pose_t *p) {
    *p=base_sample(m,now);
    float x,y,gain;touch_sample(m,now,&x,&y,&gain);
    float strength=m->expression==BOT_FACE_IDLE?1:playful(m->expression)?.5f:.15f;
    float body[2],velocity[2];body_sample(m,now,body,velocity);
    float displacement=playful(m->expression)?strength:0.09f;
    if(!playful(m->expression))gain=0;
    p->cx+=body[0]*34*displacement;p->cy+=body[1]*24*displacement;
    p->gaze_x=mix(p->gaze_x,clamp(x*1.65f,-1,1),gain);
    p->gaze_y=mix(p->gaze_y,clamp(y*1.65f,-1,1),gain);
    /* Touch changes gaze, never the task's smile/cross/lid semantics. */
    p->pupil=mix(p->pupil,.8f,gain);
    p->left_h*=1-.13f*m->hold*gain;p->right_h*=1-.13f*m->hold*gain;
    p->left_h=fmaxf(p->left_h,6);p->right_h=fmaxf(p->right_h,6);
    if(playful(m->expression) && m->pet_duration && now-m->pet_ms<m->pet_duration) {
        float t=(float)(now-m->pet_ms)/m->pet_duration;
        float a=ease(t/.18f)*ease((1-t)/.25f);
        switch(m->pet_action) {
        case 0:p->left_h=mix(p->left_h,8,a);break;
        case 1:p->left_h=mix(p->left_h,128,a);p->right_h=mix(p->right_h,128,a);break;
        case 2:p->left_h=mix(p->left_h,80,a);p->right_h=mix(p->right_h,120,a);p->gaze_y-=.45f*a;break;
        case 3:p->left_h=mix(p->left_h,42,a);p->tilt+=.18f*a;break;
        case 4:p->smile=a;break;
        case 5:p->left_h=mix(p->left_h,28,a);p->right_h=mix(p->right_h,28,a);p->cx+=7*sinf(t*12.566f)*a;break;
        case 6:p->cx+=9*sinf(t*18.85f)*a;p->cy+=2*sinf(t*12.56f)*a;break;
        }
    }
    float channels[5];
    interaction_sample(m,now,channels);
    if(playful(m->expression)) {
        /* A deliberate eye press/blink must close the eye. Task activity
         * reduces displacement and petting, not the closure endpoint. */
        float left=channels[0],right=channels[1];
        float comfort=channels[3]*strength;
        float age=(float)(now-m->attention_ms);
        float notice=(m->touch_count || m->tapped) && m->tap_streak<2?
            ease(age/90)*(1-ease((age-200)/350))*strength:0;
        /* Curiosity, then trust: soften the focused upper eyelids while
         * looking at the hand; the nearer eye opens a little more. */
        p->lid*=1-.70f*gain;p->tilt*=1-.70f*gain;
        p->left_h+=(x<=0?22:8)*notice;p->right_h+=(x>0?22:8)*notice;
        p->eye_w+=3*notice;
        float pinch=channels[2]*strength;
        p->left_h*=1-.12f*pinch;p->right_h*=1-.12f*pinch;
        p->left_h=mix(p->left_h,8,fmaxf(left,comfort*.82f));
        p->right_h=mix(p->right_h,8,fmaxf(right,comfort*.82f));
        /* Gentle smiling eyes at deep relaxation. Independent eye presses
         * own their closure and must not be hidden by a two-eye smile. */
        float warmth=ease(channels[3])*(1-fmaxf(left,right));
        p->smile=fmaxf(p->smile,(.7f+.2f*strength)*warmth);
        /* Relax into the hand, with a slow nuzzle independent of tap playback. */
        p->cx+=body[0]*8*comfort;
        p->cy+=body[1]*5*comfort;
        /* Only the hand's reversals drive this sway; a stationary hold rests. */
        float sway=clamp(m->contact_velocity[0]/350,-1,1)*comfort;
        p->cx+=4*sway;
        p->roll=(body[0]*.10f+clamp(velocity[0],-3,3)*.025f)*strength+.045f*sway;
        p->separation+=channels[2]*22*strength;
        p->eye_w+=channels[2]*9*strength;
        /* tilt remains a business eyelid shape; roll is the actual head lean. */
        float speed=m->touch_count?m->travel_speed:0;
        p->eye_w+=3*speed*strength;p->cy-=2*speed*strength;
        p->cy+=2*comfort;
        if(m->tapped && now-m->tap_ms<1100) {
            float age=(float)(now-m->tap_ms);
            float a=ease(age/65)*(1-ease((age-160)/340))*strength;
            float lids[2];poke_lids(m,now,lids);
            float side=m->tap_x<233?-1:1;
            float peek=m->touch_count?0:ease((age-260)/170)*(1-ease((age-650)/450))*strength;
            p->gaze_x=mix(p->gaze_x,side*.7f,peek*.65f);
            if(m->tap_region==3)p->convergence=.65f*a;
            else if(m->tap_region==4) {
                p->gaze_y-=.65f*a;
                if(m->tap_streak>=2) {
                    p->left_h=mix(p->left_h,55,a*.6f);p->right_h=mix(p->right_h,55,a*.6f);
                }
            } else if(m->tap_region>=5) {
                p->tilt+=side*.07f*a;
                if(m->tap_streak>=2) {
                    if(side<0)p->left_h=mix(p->left_h,45,a);
                    else p->right_h=mix(p->right_h,45,a);
                }
            }
            /* A fresh contact owns gaze immediately; an old tap may finish
             * its blink but cannot pull attention away from the new hand. */
            if(!m->touch_count)
                p->gaze_x=mix(p->gaze_x,clamp((m->tap_x-233)/180.f,-1,1),a*.65f);
            p->left_h=mix(p->left_h,8,lids[0]);p->right_h=mix(p->right_h,8,lids[1]);
        }
    }
    p->left_h=fmaxf(6,p->left_h);p->right_h=fmaxf(6,p->right_h);
    p->pupil=1;p->opacity=1;
    /* Slow whole-face pixel shift, not a second animation timebase. */
    uint32_t life=now-m->origin_ms;
    p->cx+=1.2f*wave(life,240000);p->cy+=1.2f*wave(life,300000);
}

bool bot_face_motion_pet(bot_face_motion_t *m,bool stroke,uint32_t now) {
    if(!playful(m->expression) || (m->pet_started && (int32_t)(now-m->pet_until)<0))return false;
    uint8_t *bag=stroke?m->stroke_bag:m->tap_bag;
    uint8_t *left=stroke?&m->stroke_left:&m->tap_left;
    uint8_t *last=stroke?&m->last_stroke:&m->last_tap;
    unsigned n=stroke?3:4;
    if(!*left) {
        for(unsigned i=0;i<n;i++)bag[i]=i;
        for(unsigned i=n-1;i>0;i--) { m->seed=hash(m->seed+now+i);unsigned j=m->seed%(i+1);uint8_t b=bag[i];bag[i]=bag[j];bag[j]=b; }
        if(bag[n-1]==*last) { uint8_t b=bag[0];bag[0]=bag[n-1];bag[n-1]=b; }
        *left=n;
    }
    *last=bag[--*left];m->pet_action=*last+(stroke?4:0);
    m->seed=hash(m->seed+now+1);m->pet_duration=600+m->seed%901;
    m->pet_ms=now;m->pet_until=now+m->pet_duration+400;m->pet_started=true;return true;
}

/* Stable neutral-layout hit regions; animation never moves its own targets. */
static uint8_t region(int16_t x,int16_t y)
{
    if(y>=169 && y<=297) {
        if(x>=123 && x<=217)return 1;
        if(x>=249 && x<=343)return 2;
        if(x>217 && x<249)return 3;
    }
    if(y<169)return 4;
    return x<233?5:6;
}
void bot_face_motion_clear_touch(bot_face_motion_t *m,uint32_t now)
{
    m->pet_duration=0;m->touch_active=false;m->touch_gain=m->touch_from_gain=0;
    m->touch_x=m->touch_y=m->touch_from_x=m->touch_from_y=0;
    memset(m->interaction,0,sizeof(m->interaction));
    memset(m->interaction_from,0,sizeof(m->interaction_from));
    m->touch_count=0;m->tapped=false;m->tap_streak=0;m->dual_distance=0;
    memset(m->body,0,sizeof(m->body));memset(m->body_velocity,0,sizeof(m->body_velocity));
    memset(m->body_target,0,sizeof(m->body_target));m->body_ms=now;
    m->travel_speed=0;m->contact_sample_ms=now;m->alternating=0;
    m->contact_velocity[0]=m->contact_velocity[1]=0;m->contact_was_dual=false;m->release_hold_ms=0;
    m->tap_closed_from[0]=m->tap_closed_from[1]=0;
    m->interaction_ms=m->touch_ms=now;m->interaction_duration=90;
}
void bot_face_motion_poke(bot_face_motion_t *m,int16_t x,int16_t y,uint32_t now)
{
    if(!playful(m->expression))return;
    uint8_t r=region(x,y);
    poke_lids(m,now,m->tap_closed_from);
    if(m->tapped && now-m->last_tap_ms<=450 && (x<233)!=(m->tap_x<233)) {
        if(m->alternating<6)m->alternating++;
    } else m->alternating=0;
    if(m->tapped && r==m->tap_region && now-m->last_tap_ms<=450) {
        if(m->tap_streak<6)m->tap_streak++;
    } else m->tap_streak=1;
    /* A poke changes velocity, never teleports the current body pose. The
     * existing spring brings it back; rapid taps add energy without replay. */
    body_sample(m,now,m->body,m->body_velocity);m->body_ms=now;
    float side=x<233?-1:1;
    float impulse=r<=2?6.0f:.0f;
    if(m->tap_streak==2)impulse=fmaxf(impulse,2.0f);
    if(m->tap_streak>=3)impulse=14.0f+1.5f*fminf(m->tap_streak-3,2);
    /* Alternating pokes invite a pursuit; repeated pokes cause avoidance. */
    float direction=m->alternating?side:-side;
    if(m->alternating)impulse=6.0f+fminf(m->alternating,4);
    m->body_velocity[0]=clamp(m->body_velocity[0]+direction*impulse,-18,18);
    m->body_velocity[1]=clamp(m->body_velocity[1]-(impulse>0?2.0f:0),-5,5);
    m->tap_region=r;m->tap_x=x;m->tap_y=y;
    m->tap_ms=m->last_tap_ms=now;m->tapped=true;
}
void bot_face_motion_interact(bot_face_motion_t *m,const bot_touch_frame_t *f,
                              float comfort,uint32_t held_ms)
{
    uint8_t count=f->cancelled?0:f->count;
    if(count>2)count=0;
    float channels[5]={0};int x=233,y=233;
    if(m->tapped && f->time_ms-m->last_tap_ms>=1200){m->tap_streak=0;m->alternating=0;m->tapped=false;}
    if(count) {
        if(!m->touch_count)m->contact_was_dual=false;
        if(count==2)m->contact_was_dual=true;
        x=f->points[0].x;y=f->points[0].y;
        if(count==2){x=(x+f->points[1].x)/2;y=(y+f->points[1].y)/2;}
        uint32_t dt=f->time_ms-m->contact_sample_ms;
        if(m->touch_count==count && dt>0 && dt<120) {
            float dx=x-m->last_contact_x,dy=y-m->last_contact_y;
            float speed=clamp(sqrtf(dx*dx+dy*dy)*1000/dt/700,0,1);
            m->travel_speed=mix(m->travel_speed,speed,1-expf(-(float)dt/80));
            float follow=1-expf(-(float)dt/140);
            m->contact_velocity[0]=mix(m->contact_velocity[0],clamp(dx*1000/dt,-700,700),follow);
            m->contact_velocity[1]=mix(m->contact_velocity[1],clamp(dy*1000/dt,-700,700),follow);
        } else {m->travel_speed=0;m->contact_velocity[0]=m->contact_velocity[1]=0;}
        m->last_contact_x=x;m->last_contact_y=y;m->contact_sample_ms=f->time_ms;
        channels[3]=clamp(comfort,0,1);
        /* Cupping persists under the surviving finger until it also lifts. */
        if(count==1 && m->contact_was_dual)channels[3]=fmaxf(channels[3],.65f*ease(held_ms/1500.f));
        channels[4]=clamp((x-233)/180.f,-1,1);
        if(count==1 && region(x,y)==4)
            channels[3]=fmaxf(channels[3],.4f*ease(held_ms/1500.f));
        if(count==2) {
            for(unsigned i=0;i<2;i++) {
                uint8_t r=region(f->points[i].x,f->points[i].y);
                if(r==1)channels[0]=1;
                if(r==2)channels[1]=1;
            }
            float dx=f->points[1].x-f->points[0].x,dy=f->points[1].y-f->points[0].y;
            float d=sqrtf(dx*dx+dy*dy);
            if(m->touch_count!=2)m->dual_distance=d;
            channels[2]=clamp((d-m->dual_distance)/90,-1,1);
            channels[3]=fmaxf(channels[3],.85f*ease(held_ms/1500.f));
        } else if(m->touch_count==2 || m->interaction[0]>0 || m->interaction[1]>0) {
            /* Preserve closure only beneath the surviving finger. */
            uint8_t r=region(x,y);
            if(r==1)channels[0]=1;
            if(r==2)channels[1]=1;
        }
    }
    bot_face_motion_touch(m,count>0,x,y,0,f->time_ms);
    if(!count && m->tapped && !f->cancelled) {
        /* Let the spring peek back after recoiling; retarget from the sampled
         * position/velocity, so a new contact can interrupt without jumping. */
        float age=(float)(f->time_ms-m->tap_ms);
        float peek=ease((age-260)/170)*(1-ease((age-650)/450));
        float tx=clamp((m->tap_x-233)/180.f,-1,1)*.45f*peek;
        if(fabsf(tx-m->body_target[0])>.0001f) {
            body_sample(m,f->time_ms,m->body,m->body_velocity);
            m->body_target[0]=tx;m->body_ms=f->time_ms;
        }
    }
    bool changed=count!=m->touch_count;
    for(unsigned i=0;i<5;i++)if(fabsf(channels[i]-m->interaction[i])>.001f)changed=true;
    if(changed) {
        float current[5];interaction_sample(m,f->time_ms,current);
        for(unsigned i=0;i<5;i++) {
            m->interaction_from[i]=current[i];
            m->interaction[i]=channels[i];
        }
        m->interaction_ms=f->time_ms;m->interaction_duration=count?90:500;
    }
    m->touch_count=count;
    if(!count)m->contact_was_dual=false;
}
