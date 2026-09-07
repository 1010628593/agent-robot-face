/* Offline interaction trace using the real estimator and pose compositor.
 * Synthetic acceleration is an engineering preview, not hardware acceptance.
 * Compile with bot_motion.c and face_reaction.c, their includes, and -lm. */
#include "bot_face_reaction.h"
#include <math.h>
#include <stdio.h>
int main(void) {
    puts("scenario,ms,linear_g,face_x,face_y,gaze_x,gaze_y,left_h,right_h,fatigue");
    const float strength[]={.15f,.55f,1.1f,2.0f};
    for(int scenario=0;scenario<4;scenario++) {
        bot_motion_t sensor;bot_motion_init(&sensor,0,1);
        bot_face_inertia_t inertia={0};
        for(uint32_t now=10;now<=16000;now+=10) {
            float drive=now>=2000 && now<7000?strength[scenario]*sinf((now-2000)*.018849556f):0;
            bot_motion_sample_t sample={.ms=now,.accel={drive,-1,0},.gyro={0,0,0}};
            bot_motion_feed(&sensor,&sample);
            if(now%20)continue;
            bot_motion_view_t view=bot_motion_view(&sensor,now);
            bot_face_pose_t pose={.cx=233,.cy=233,.eye_w=74,.left_h=108,.right_h=108,.separation=126,.pupil=1,.opacity=1};
            bot_face_reaction_apply(&view,BOT_STATE_IDLE,now,&pose);
            bot_face_inertia_apply(&inertia,&view,view.rotation_deg,true,now,&pose);
            printf("%d,%u,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f\n",scenario,now,view.linear_g,pose.cx,pose.cy,pose.gaze_x,pose.gaze_y,pose.left_h,pose.right_h,inertia.fatigue);
        }
    }
}
