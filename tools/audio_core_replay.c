#include "bot_audio_core.h"
#include <stdio.h>
#include <math.h>
#include <stdint.h>
static bot_audio_core_t c;
static uint32_t now;
static uint64_t samples;
static unsigned hop;
static void feed(float amplitude, float dc, unsigned ms) {
 for(unsigned j=0;j<ms/10;j++) {
  int16_t pcm[221]; unsigned n=220+(hop++%2);
  for(unsigned i=0;i<n;i++,samples++) pcm[i]=(int16_t)(dc+amplitude*sinf(6.2831853f*800*samples/22050));
  now+=10;bot_audio_core_feed(&c,pcm,n,now,false);
 }
}
int main(void) {
 bot_audio_config_t cfg={BOT_AUDIO_MODE_RHYTHM,BOT_AUDIO_SENSITIVITY_MEDIUM};
 now=UINT32_MAX-1000;bot_audio_core_init(&c,7,&cfg);
 feed(8,1000,3500);printf("calibration state=%d rms=%g floor=%g\n",c.view.service_state,c.view.rms,c.view.noise_floor_dbfs);
 for(int i=0;i<6;i++){feed(12000,1000,30);printf("onset %d event=%d id=%u beat=%u locked=%d period=%.0f\n",i,c.view.event,c.view.event_id,c.view.beat_seq,c.view.rhythm_locked,c.view.period_ms);feed(8,1000,470);}
 feed(8,1000,1000);printf("lost lock=%d retained=%d\n",c.view.rhythm_locked,c.view.event);
 feed(700,1000,1500);printf("sustained=%d event=%d\n",c.view.sustained,c.view.event);
 feed(8,1000,600);printf("quiet sustained=%d event=%d\n",c.view.sustained,c.view.event);
 int16_t pcm[220];for(unsigned i=0;i<220;i++)pcm[i]=-32768;
 now+=10;bot_audio_core_feed(&c,pcm,220,now,false);printf("negative rail peak=%g quality=%u\n",c.view.peak,c.view.quality_flags);
 bot_audio_core_feed(&c,pcm,219,now,false);printf("short available=%d quality=%u state=%d\n",c.view.available,c.view.quality_flags,c.view.service_state);
 const unsigned bpm[] = {60,90,120,150,180};
 for(unsigned b=0;b<5;b++) {
  bot_audio_core_init(&c,8+b,&cfg); feed(8,0,3500);
  unsigned previous_target=0;
  for(unsigned i=0;i<6;i++) {
   unsigned target=(unsigned)lroundf(i*6000.0f/bpm[b])*10;
   if(i) feed(8,0,target-previous_target-30);
   feed(1500,0,30); previous_target=target;
  }
  printf("bpm=%u locked=%d period=%.0f beat=%u\n",bpm[b],c.view.rhythm_locked,c.view.period_ms,c.view.beat_seq);
  if(bpm[b]==120) {
   feed(8,0,470);feed(24000,0,30);
   printf("exceptional locked=%d event=%d\n",c.view.rhythm_locked,c.view.event);
  }
 }
 bot_audio_core_init(&c,20,&cfg);
 for(unsigned i=0;i<100;i++) feed((i%2)?10000:8,0,100);
 printf("unstable state=%d low_confidence=%d\n",c.view.service_state,!!(c.view.quality_flags&BOT_AUDIO_QUALITY_LOW_CONFIDENCE));
 bot_audio_core_init(&c,21,&cfg);printf("restart epoch=%u available=%d event_id=%u beat=%u\n",c.view.stream_epoch,c.view.available,c.view.event_id,c.view.beat_seq);
 return 0;
}
