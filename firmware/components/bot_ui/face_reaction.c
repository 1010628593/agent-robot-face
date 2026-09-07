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
        float envelope=ease((float)age/150)*ease((float)(BOT_MOTION_DIZZY_MS-age)/600);
        float phase=(float)age*.011f;
        p->pupil=fmaxf(p->pupil,.9f*envelope);
        p->gaze_x=p->gaze_x*(1-envelope)+.72f*cosf(phase)*envelope;
        p->gaze_y=p->gaze_y*(1-envelope)+.58f*sinf(phase)*envelope;
        p->left_h*=1-.25f*envelope;p->right_h*=1-.06f*envelope;
        p->lid=fmaxf(p->lid,.10f*envelope);p->cy+=1.6f*sinf(phase)*envelope;
        /* One slow recovery blink, not X eyes (reserved for Agent error). */
        if(age>2050) {
            float blink=sinf((float)(age-2050)*3.14159265f/350);
            p->left_h*=1-.92f*blink;p->right_h*=1-.92f*blink;
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
