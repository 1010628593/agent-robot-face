#include "bot_face_reaction.h"
#include <math.h>
#include <stdio.h>
int main(void){
 float amplitudes[]={.12,.25,.45,.8,1.3};
 puts("amplitude,max_angle,rest_angle_500ms,max_spiral,max_fatigue");
 for(int j=0;j<5;j++){
  bot_motion_t m;bot_motion_init(&m,0,1);bot_face_inertia_t f={0};float angle=0,rest=0,spiral=0,fatigue=0;
  for(unsigned t=10;t<=12000;t+=10){
   float a=t>=2000 && t<8000?amplitudes[j]*sinf((t-2000)*.018849556f):0;
   bot_motion_sample_t sample={.ms=t,.accel={a,-1,0}};bot_motion_feed(&m,&sample);
   bot_motion_view_t v=bot_motion_view(&m,t);
   if(t>=2000 && t<8000)angle=fmaxf(angle,fabsf(v.rotation_deg));if(t==8500)rest=v.rotation_deg;
   if(t%20)continue;
   bot_face_pose_t p={.cx=233,.cy=233,.eye_w=74,.left_h=108,.right_h=108,.separation=126};
   bot_face_reaction_apply(&v,BOT_STATE_IDLE,t,&p);bot_face_inertia_apply(&f,&v,v.rotation_deg,true,t,&p);
   spiral=fmaxf(spiral,p.spiral);fatigue=fmaxf(fatigue,f.fatigue);
  }
  printf("%.2f,%.4f,%.4f,%.4f,%.4f\n",amplitudes[j],angle,rest,spiral,fatigue);
 }
}
