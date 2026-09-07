#ifdef NDEBUG
#undef NDEBUG
#endif
#include "bot_face_reaction.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    bot_face_motion_t m;bot_face_motion_init(&m,3,0);bot_face_pose_t base,p;
    bot_face_motion_sample(&m,500,&base);
    bot_motion_view_t v={.available=true,.sampled_ms=500,.event_ms=0,.event_id=1,.reaction=BOT_REACTION_DIZZY};
    p=base;bot_face_reaction_apply(&v,BOT_STATE_WORKING,500,&p);
    assert(p.pupil>.5f && p.left_h!=p.right_h && p.cross==base.cross && p.smile==base.smile);
    const bot_state_t protected[]={BOT_STATE_WAITING,BOT_STATE_ERROR,BOT_STATE_DONE,BOT_STATE_UNKNOWN,BOT_STATE_CANCELLED};
    for(unsigned i=0;i<sizeof(protected)/sizeof(protected[0]);i++) {
        p=base;bot_face_reaction_apply(&v,protected[i],500,&p);assert(memcmp(&p,&base,sizeof(p))==0);
    }
    p=base;bot_face_reaction_apply(&v,BOT_STATE_IDLE,900,&p);assert(memcmp(&p,&base,sizeof(p))==0);
    v.sampled_ms=3000;p=base;bot_face_reaction_apply(&v,BOT_STATE_IDLE,3000,&p);assert(memcmp(&p,&base,sizeof(p))==0);
    for(int r=1;r<=3;r++)for(uint32_t t=0;t<2800;t+=10) {
        v.sampled_ms=t;v.reaction=(bot_reaction_t)r;p=base;
        bot_face_reaction_apply(&v,BOT_STATE_TOOL,t,&p);
        assert(isfinite(p.left_h) && p.left_h>=6 && p.left_h<=135 && p.right_h>=6 && p.right_h<=135);
    }
    puts("reaction: priority, freshness, expiry, no Agent mutation and bounds passed");return 0;
}
