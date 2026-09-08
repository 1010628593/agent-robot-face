/* UI-owner audio director. Hardware only publishes value snapshots. */
#include "bot_ui.h"
#include "bot_face_reaction.h"
#include <math.h>
static bot_audio_view_t audio;
static bot_audio_config_t config={BOT_AUDIO_MODE_OFF,BOT_AUDIO_SENSITIVITY_MEDIUM};
static bool config_pending,contact,had_touch,had_motion;
static uint32_t touch_ms,motion_ms,epoch,event_id,beat_seq,started,duration;
static float amplitude;
static bot_audio_event_t reaction;
static uint32_t loud_ms;
static bool had_loud,feedback_pending,feedback_active;
static uint32_t feedback_ms;
static uint8_t agent=255;
static const char *reason="off";
void stats_refresh(void);
bool stats_options_open(void);
static uint32_t diagnostics_ms;
void bot_ui_set_audio(const bot_audio_view_t *v){
 if(!v)return;
 bool changed=audio.service_state!=v->service_state||audio.last_error!=v->last_error;
 audio=*v;
 if(g_ui.screen==BOT_SCR_STATS&&(changed||(stats_options_open()&&lv_tick_get()-diagnostics_ms>=250))){diagnostics_ms=lv_tick_get();stats_refresh();}
}
const bot_audio_view_t *bot_ui_audio_view(void){return &audio;}
const bot_audio_config_t *bot_ui_audio_config(void){return &config;}
const char *bot_ui_audio_suppression(void){return reason;}
void bot_ui_audio_request(bot_audio_config_t c){config=c;config_pending=true;if(g_ui.screen==BOT_SCR_STATS)feedback_pending=true;if(c.mode==BOT_AUDIO_MODE_OFF)duration=0;}
bool bot_ui_take_audio_config(bot_audio_config_t *out){if(!out||!config_pending)return false;*out=config;config_pending=false;return true;}
void face_audio_contact(const bot_touch_frame_t *f){if(f->count||contact){touch_ms=f->time_ms;had_touch=true;}contact=f->count>0;}
void face_audio_motion(const bot_motion_view_t *v){if(v&&v->available&&(v->linear_g>.18f||v->gyro_dps>25)){motion_ms=v->sampled_ms;had_motion=true;}}
bool bot_ui_audio_background_suppressed(void){uint32_t now=lv_tick_get();return contact||(had_touch&&now-touch_ms<250)||(had_motion&&now-motion_ms<300);}
void face_audio_cancel(void){duration=0;event_id=audio.event_id;beat_seq=audio.beat_seq;epoch=audio.stream_epoch;}
void face_audio_sync(uint32_t now,bool imu_active){
 const bot_navigation_t *nav=bot_ui_navigation();
 bool fresh=audio.available&&now-audio.sampled_ms<=150;
 reason=config.mode==BOT_AUDIO_MODE_OFF?"off":audio.service_state!=BOT_AUDIO_RUNNING?"service":!fresh?"stale":
  g_ui.screen!=BOT_SCR_FACE||nav->phase!=BOT_NAV_FACE?"hidden":g_ui.selected>=BOT_AGENT_COUNT?"agent":
  !bot_face_reaction_allowed(g_ui.agents[g_ui.selected].state)?"task":contact||(had_touch&&now-touch_ms<250)?"touch":
  (had_motion&&now-motion_ms<300)?"motion":imu_active?"imu":(audio.quality_flags & ~BOT_AUDIO_QUALITY_MOTION_FILTER_DEGRADED)?"quality":"none";
 bool eligible=reason[0]=='n';
 if(agent!=g_ui.selected||epoch!=audio.stream_epoch){face_audio_cancel();agent=g_ui.selected;}
 if(!eligible){face_audio_cancel();return;}
 if(duration&&now-started>=duration)duration=0;
 /* Leave the event pending for 60ms so delayed motion/touch samples can veto it. */
 if(audio.event_id!=event_id){uint32_t age=now-audio.event_ms;
  if(age>=60){event_id=audio.event_id;if(age<=(audio.event_ttl_ms<150?audio.event_ttl_ms:150)){
   if(audio.event==BOT_AUDIO_EVENT_LOUD&&(!had_loud||now-loud_ms>=1200)&&!(config.mode==BOT_AUDIO_MODE_RHYTHM&&audio.rhythm_locked)){started=now;duration=480;amplitude=.10f*fmaxf(.2f,fminf(1,audio.event_strength));reaction=BOT_AUDIO_EVENT_LOUD;loud_ms=now;had_loud=true;}
   else if(audio.event==BOT_AUDIO_EVENT_SUSTAINED&&!duration){started=now;duration=3000;amplitude=.02f;reaction=BOT_AUDIO_EVENT_SUSTAINED;}
   else if(audio.event==BOT_AUDIO_EVENT_QUIET&&(!duration||reaction==BOT_AUDIO_EVENT_SUSTAINED)){started=now;duration=600;amplitude=-.02f;reaction=BOT_AUDIO_EVENT_QUIET;}
  }}
 }
 if(audio.beat_seq!=beat_seq){uint32_t age=now-audio.last_onset_ms;
  if(age>=60){beat_seq=audio.beat_seq;if(age<=150&&config.mode==BOT_AUDIO_MODE_RHYTHM&&audio.rhythm_locked&&(!duration||reaction!=BOT_AUDIO_EVENT_LOUD)){started=now;duration=180;amplitude=.04f;reaction=BOT_AUDIO_EVENT_RHYTHM;}}
 }
}
bool face_audio_apply(uint32_t now,bot_face_pose_t *p){
 if(!duration)return false;
 uint32_t age=now-started;if(age>=duration){duration=0;return false;}
 float envelope=sinf(3.14159265f*(float)age/duration);
 if(reaction==BOT_AUDIO_EVENT_LOUD){
  float phase=age<90?(float)age/90:age<180?1:(float)(480-age)/300;
  envelope=phase*phase*(3-2*phase);
 }
 if(reaction==BOT_AUDIO_EVENT_SUSTAINED)envelope*=fmaxf(0,fminf(1,audio.level_norm));
 float scale=g_ui.agents[g_ui.selected].state==BOT_STATE_IDLE?1:.5f;
 /* Blink lids and reserved expressions remain exactly as sampled. */
 if(p->left_h<=18||p->right_h<=18||p->cross>0||p->smile>0){duration=0;return false;}
 p->left_h*=1+amplitude*envelope*scale;p->right_h*=1+amplitude*envelope*scale;
 if(reaction==BOT_AUDIO_EVENT_LOUD){p->cy-=2*envelope*scale;p->gaze_y-=.08f*envelope*scale;}
 if(reaction==BOT_AUDIO_EVENT_RHYTHM)p->cy+=1.5f*envelope*scale;
 return true;
}

/* One transient acknowledgement after an explicit settings change; text always
 * reflects confirmed service state rather than the requested switch value. */
const char *face_audio_feedback(uint32_t now){
 if(g_ui.screen!=BOT_SCR_FACE)return NULL;
 if(feedback_pending){feedback_pending=false;feedback_active=true;feedback_ms=now;}
 if(!feedback_active||now-feedback_ms>=1200){feedback_active=false;return NULL;}
 static const char *messages[]={"声响应 · 已关闭","声响应 · 启动中","声响应 · 校准中","声响应 · 运行","声响应 · 停止中","声响应 · 故障"};
 return (unsigned)audio.service_state<6?messages[audio.service_state]:messages[5];
}
