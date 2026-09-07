#include "bot_face_reaction.h"
#include <math.h>
static float ease(float x) { x=fmaxf(0,fminf(1,x));return x*x*(3-2*x); }
bool bot_face_reaction_allowed(bot_state_t s) {
    return s==BOT_STATE_IDLE || s==BOT_STATE_WORKING || s==BOT_STATE_TOOL;
}
void bot_face_reaction_apply(const bot_motion_view_t *v,bot_state_t state,uint32_t now,bot_face_pose_t *p) {
    if(!v || !p || !v->available || now-v->sampled_ms>BOT_MOTION_STALE_MS ||
       !bot_face_reaction_allowed(state))return;
    uint32_t age=now-v->event_ms;
    if(v->reaction==BOT_REACTION_DIZZY && age<BOT_MOTION_DIZZY_MS) {
        float envelope=ease((float)age/150)*ease((float)(BOT_MOTION_DIZZY_MS-age)/250);
        envelope*=v->reaction_strength>0?v->reaction_strength:1;
        float phase=(float)age*.011f;
        p->pupil=fmaxf(p->pupil,.9f*envelope);
        p->gaze_x=p->gaze_x*(1-envelope)+.72f*cosf(phase)*envelope;
        p->gaze_y=p->gaze_y*(1-envelope)+.58f*sinf(phase)*envelope;
        p->left_h*=1-.25f*envelope;p->right_h*=1-.06f*envelope;
        p->lid=fmaxf(p->lid,.10f*envelope);p->cy+=1.6f*sinf(phase)*envelope;
        /* One slow recovery blink, not X eyes (reserved for Agent error). */
        if(age>800 && age<1150) {
            float blink=sinf((float)(age-800)*3.14159265f/350);
            p->left_h*=1-.92f*blink*envelope;p->right_h*=1-.92f*blink*envelope;
        }
    } else if(v->reaction==BOT_REACTION_ATTENTION && age<600) {
        float gain=sinf((float)age*3.14159265f/600);
        p->left_h*=1+.07f*gain;p->right_h*=1+.07f*gain;
    } else if(v->reaction==BOT_REACTION_SETTLE && age<450) {
        float gain=sinf((float)age*6.2831853f/450)*(1-(float)age/450);
        p->left_h*=1-.10f*gain;p->right_h*=1-.10f*gain;p->cy+=1.8f*gain;
    }
    p->left_h=fmaxf(6,fminf(135,p->left_h));p->right_h=fmaxf(6,fminf(135,p->right_h));
}


/* Two coupled-looking spring pairs, integrated in bounded 5ms steps. Sensor
 * force is transformed into the same local coordinates used to draw the face.
 * No displacement integration from raw acceleration: springs prevent drift. */
static float bounded(float x,float lo,float hi) { return fmaxf(lo,fminf(hi,x)); }
void bot_face_inertia_apply(bot_face_inertia_t *m,const bot_motion_view_t *v,
                            float rotation,bool enabled,uint32_t now,bot_face_pose_t *p) {
    if(!m || !p)return;
    uint32_t elapsed=now-m->last_ms;
    m->last_ms=now;
    if(!enabled) { *m=(bot_face_inertia_t){.last_ms=now};return; }
    if(!m->initialized || elapsed>250) {
        *m=(bot_face_inertia_t){.last_ms=now,.initialized=true};return;
    }
    bool fresh=v && v->available && now-v->sampled_ms<=BOT_MOTION_STALE_MS;
    float ax=0,ay=0,az=0,energy=0;
    if(fresh && isfinite(v->screen_accel[0]) && isfinite(v->screen_accel[1]) &&
       isfinite(v->screen_accel[2]) && isfinite(v->linear_g)) {
        float a=rotation*.01745329252f,c=cosf(a),s=sinf(a);
        ax=bounded(c*v->screen_accel[0]+s*v->screen_accel[1],-2.5f,2.5f);
        ay=bounded(-s*v->screen_accel[0]+c*v->screen_accel[1],-2.5f,2.5f);
        az=bounded(v->screen_accel[2],-2.5f,2.5f);
        energy=bounded(v->linear_g-.025f,0,3);
    }
    float dt=(float)elapsed*.001f;
    if(dt<=0)return;
    /* Fast peak envelope spans zero crossings; hysteresis prevents grade chatter. */
    m->amplitude=fmaxf(energy,m->amplitude*expf(-dt/.22f));
    static const float thresholds[]={0,.06f,.18f,.40f,.75f};
    unsigned grade=m->pending_grade;
    while(grade<4 && m->amplitude>=thresholds[grade+1])grade++;
    while(grade>0 && m->amplitude<thresholds[grade]*.75f)grade--;
    if(grade!=m->pending_grade){m->pending_grade=grade;m->grade_ms=now;}
    if(now-m->grade_ms>=(grade>m->grade?60u:140u))m->grade=grade;
    if(energy>.10f) { m->quiet_ms=now;m->recovering=m->fatigue>.18f; }
    float jerk=fmaxf(0,energy-m->previous_energy)/dt;
    m->previous_energy=energy;
    m->startle=fmaxf(m->startle,bounded((jerk-3)*.07f,0,1));
    for(float remaining=dt;remaining>.00001f;) {
        float h=fminf(remaining,.005f);remaining-=h;
        m->intensity+=(bounded(m->amplitude/1.05f,0,1)-m->intensity)*fminf(1,h*24);
        /* Each amplitude band has a fatigue ceiling: duration alone cannot
         * promote gentle movement into full spiral dizziness. */
        float ceiling=ease((m->amplitude-.18f)/.75f);
        if(energy<.10f && now-m->quiet_ms>120)
            m->fatigue=fmaxf(0,m->fatigue-h*.85f);
        else m->fatigue+=(ceiling-m->fatigue)*fminf(1,h*3);
        m->startle=fmaxf(0,m->startle-h*2.4f);
        m->phase+=h*(5+5*m->intensity+3*m->fatigue);
        if(m->phase>6.2831853f)m->phase-=6.2831853f;
        for(int i=0;i<4;i++) {
            float force=i%2?ay:ax;
            float k=i<2?180:220,damping=i<2?20:25;
            float gain=i<2?7000:360;
            float limit=i<2?34:1;
            m->velocity[i]+=h*(-force*gain-k*m->position[i]-damping*m->velocity[i]);
            m->velocity[i]=bounded(m->velocity[i],i<2?-360:-14,i<2?360:14);
            m->position[i]+=h*m->velocity[i];
            if(fabsf(m->position[i])>limit) {
                m->position[i]=copysignf(limit,m->position[i]);
                if(m->velocity[i]*m->position[i]>0)m->velocity[i]*=-.22f;
            }
        }
    }
    float effort=m->intensity,fatigue=ease((m->fatigue-.05f)/.65f);
    float spiral_target=m->grade==4?ease((m->amplitude-.65f)/.50f)*ease((m->fatigue-.30f)/.40f):0;
    m->spiral_gain+=(spiral_target-m->spiral_gain)*(1-expf(-dt/(spiral_target>m->spiral_gain?.05f:.12f)));
    p->spiral=m->spiral_gain;
    p->spiral_phase=fmodf((float)(now%100000u)*.0062831853f,6.2831853f);
    float wobble=sinf(m->phase),orbit=m->phase*1.0f;
    p->cx+=m->position[0]+5*fatigue*wobble;
    p->cy+=m->position[1]+4*fatigue*cosf(orbit);
    p->gaze_x=bounded(p->gaze_x+m->position[2]*.65f+fatigue*.5f*cosf(orbit),-1,1);
    p->gaze_y=bounded(p->gaze_y+m->position[3]*.65f+fatigue*.5f*sinf(orbit),-1,1);
    p->convergence=bounded(p->convergence+fatigue*.45f*wobble,-.7f,.7f);
    float stretch=bounded(-ay*.15f+az*.09f,-.22f,.22f)*effort;
    p->eye_w*=1-stretch;
    float brace=ease((effort-.45f)/.35f)*(.5f+.5f*sinf(m->phase*2));
    float open=1+.24f*m->startle+.13f*effort-.70f*brace;
    p->left_h*=open*(1+stretch)*(1-.32f*fatigue+.12f*fatigue*wobble);
    p->right_h*=open*(1+stretch)*(1-.18f*fatigue-.12f*fatigue*wobble);
    /* Strong dizziness keeps the spirals legible until the recovery blink. */
    p->left_h=fmaxf(p->left_h,60*p->spiral);
    p->right_h=fmaxf(p->right_h,60*p->spiral);
    p->gaze_x*=1-.75f*p->spiral;p->gaze_y*=1-.75f*p->spiral;
    if(m->recovering && energy<=.10f) {
        uint32_t rest=now-m->quiet_ms;
        if(rest>=180 && rest<460) {
            float blink=sinf((float)(rest-180)*3.14159265f/280);
            p->left_h*=1-.94f*blink;p->right_h*=1-.94f*blink;
        }
        if(rest>=1000)m->recovering=false;
    }
    p->lid=bounded(p->lid+.18f*fatigue+.12f*brace,0,.65f);
    p->tilt=bounded(p->tilt+m->position[0]*.003f,-.25f,.25f);
    /* Keep all exaggerated geometry inside the existing rotated surface. */
    p->cx=bounded(p->cx,195,271);p->cy=bounded(p->cy,195,271);
    p->eye_w=bounded(p->eye_w,20,86);p->separation=bounded(p->separation,60,160);
    p->left_h=bounded(p->left_h,6,135);p->right_h=bounded(p->right_h,6,135);
}
