/* Scripted lifecycle replay against the actual service translation unit.
 * pthread synchronization replaces FreeRTOS; fake port never accesses hardware. */
#define _POSIX_C_SOURCE 200809L
#include "bot_audio.h"
#include "bot_audio_port.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <stdatomic.h>

struct replay_task {
 pthread_t thread; pthread_mutex_t mutex; pthread_cond_t cond;
 unsigned notifications; void (*fn)(void *); void *arg;
};
static struct replay_task task;
static atomic_uint workers, opens, closes, violations, reads;
static atomic_bool read_timeout, cleanup_failure, hold_open, entered_open, release_open;
static atomic_bool port_open, port_initialized;
static uint64_t stream_anchor, stream_samples;
static bot_audio_port_read_meta_t meta;
static unsigned generation;
static void sleep_ms(unsigned ms) {
 struct timespec delay={ms/1000,(long)(ms%1000)*1000000};
 while(nanosleep(&delay,&delay)!=0 && errno==EINTR) {}
}
int64_t esp_timer_get_time(void) {
 struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);
 return (int64_t)ts.tv_sec*1000000+ts.tv_nsec/1000;
}
static void *entry(void *p) { struct replay_task *t=p;t->fn(t->arg);return NULL; }
int xTaskCreate(void (*fn)(void *),const char *name,unsigned stack,void *arg,unsigned priority,TaskHandle_t *out) {
 (void)name;(void)stack;(void)priority;
 if(atomic_fetch_add(&workers,1)!=0){atomic_fetch_add(&violations,1);return 0;}
 task=(struct replay_task){.mutex=PTHREAD_MUTEX_INITIALIZER,.cond=PTHREAD_COND_INITIALIZER,.fn=fn,.arg=arg};
 *out=&task;return pthread_create(&task.thread,NULL,entry,&task)==0?pdPASS:0;
}
uint32_t ulTaskNotifyTake(int clear,uint32_t ticks) {
 pthread_mutex_lock(&task.mutex);
 if(ticks==portMAX_DELAY) {
  while(!task.notifications)pthread_cond_wait(&task.cond,&task.mutex);
 } else if(!task.notifications) {
  struct timespec end;clock_gettime(CLOCK_REALTIME,&end);
  end.tv_sec+=ticks/1000;end.tv_nsec+=(long)(ticks%1000)*1000000;
  if(end.tv_nsec>=1000000000){++end.tv_sec;end.tv_nsec-=1000000000;}
  while(!task.notifications && pthread_cond_timedwait(&task.cond,&task.mutex,&end)!=ETIMEDOUT) {}
 }
 unsigned n=task.notifications;if(clear)task.notifications=0;else if(n)--task.notifications;
 pthread_mutex_unlock(&task.mutex);return n;
}
void xTaskNotifyGive(TaskHandle_t t) {
 pthread_mutex_lock(&t->mutex);++t->notifications;pthread_cond_signal(&t->cond);pthread_mutex_unlock(&t->mutex);
}
void vTaskDelay(uint32_t ticks) { sleep_ms(ticks); }
esp_err_t bot_audio_port_init(const bot_audio_port_config_t *c) {
 (void)c;if(atomic_exchange(&port_initialized,true)){atomic_fetch_add(&violations,1);return ESP_ERR_INVALID_STATE;}return ESP_OK;
}
esp_err_t bot_audio_port_open(void) {
 atomic_store(&entered_open,true);
 if(atomic_load(&hold_open))while(!atomic_load(&release_open))sleep_ms(1);
 if(atomic_exchange(&port_open,true)){atomic_fetch_add(&violations,1);return ESP_ERR_INVALID_STATE;}
 atomic_fetch_add(&opens,1);stream_anchor=esp_timer_get_time();stream_samples=0;++generation;return ESP_OK;
}
esp_err_t bot_audio_port_read(void *data,size_t size,size_t *got,uint32_t timeout) {
 (void)timeout;sleep_ms(10);atomic_fetch_add(&reads,1);*got=0;
 if(atomic_load(&read_timeout))return ESP_ERR_TIMEOUT;
 memset(data,0,size);*got=size;bool first=stream_samples==0;stream_samples+=size/2;
 meta=(bot_audio_port_read_meta_t){.capture_end_us=stream_anchor+stream_samples*1000000/22050,.generation=generation,.reconfigured=first};
 return ESP_OK;
}
esp_err_t bot_audio_port_get_last_read(bot_audio_port_read_meta_t *out) { *out=meta;return ESP_OK; }
esp_err_t bot_audio_port_close(void) {
 if(atomic_load(&cleanup_failure))return ESP_FAIL;
 if(atomic_exchange(&port_open,false))atomic_fetch_add(&closes,1);return ESP_OK;
}
esp_err_t bot_audio_port_deinit(void) { atomic_store(&port_initialized,false);return ESP_OK; }
esp_err_t bot_audio_port_get_stats(bot_audio_port_stats_t *out) { memset(out,0,sizeof(*out));return ESP_OK; }
bool bot_audio_port_is_open(void) { return atomic_load(&port_open); }
static unsigned failures;
static bot_audio_view_t latest(void) { bot_audio_view_t v;bot_audio_latest((uint32_t)(esp_timer_get_time()/1000),&v);return v; }
static bool wait_state(bot_audio_service_state_t target,unsigned timeout) {
 for(unsigned i=0;i<timeout;i++){if(latest().service_state==target)return true;sleep_ms(1);}return false;
}
static bool wait_atomic(atomic_bool *value,unsigned timeout) {
 for(unsigned i=0;i<timeout;i++){if(atomic_load(value))return true;sleep_ms(1);}return false;
}
static void check(bool okay) { if(!okay)++failures; }
int main(void) {
 check(bot_audio_init()==ESP_OK);bot_audio_set_environment(false,true);
 unsigned cycles=0;
 for(unsigned i=0;i<100;i++) {
  check(bot_audio_start()==ESP_OK);check(wait_state(BOT_AUDIO_CALIBRATING,500));
  if(bot_audio_stop(500)==ESP_OK && !atomic_load(&port_open))++cycles;else ++failures;
 }
 printf("start_stop cycles=%u workers=%u opens=%u closes=%u\n",cycles,atomic_load(&workers),atomic_load(&opens),atomic_load(&closes));
 check(bot_audio_start()==ESP_OK);check(wait_state(BOT_AUDIO_CALIBRATING,500));
 uint32_t epoch=latest().stream_epoch;unsigned before=atomic_load(&opens);
 for(unsigned i=0;i<20;i++)check(bot_audio_start()==ESP_OK);
 sleep_ms(60);bool duplicate=epoch==latest().stream_epoch && before==atomic_load(&opens);check(duplicate);
 printf("duplicate_start stable_epoch=%d opens_delta=%u\n",duplicate,atomic_load(&opens)-before);
 bot_audio_config_t rhythm={BOT_AUDIO_MODE_RHYTHM,BOT_AUDIO_SENSITIVITY_HIGH};
 check(bot_audio_request_config(&rhythm)==ESP_OK);sleep_ms(60);
 bool advanced=latest().stream_epoch>epoch;check(advanced);printf("config_change epoch_advanced=%d\n",advanced);
 check(bot_audio_stop(500)==ESP_OK);
 atomic_store(&hold_open,true);atomic_store(&entered_open,false);atomic_store(&release_open,false);
 check(bot_audio_start()==ESP_OK);check(wait_atomic(&entered_open,500));
 esp_err_t stop=bot_audio_stop(5);check(stop==ESP_ERR_TIMEOUT);check(latest().service_state==BOT_AUDIO_STOPPING);
 atomic_store(&release_open,true);check(wait_state(BOT_AUDIO_DISABLED,500));check(!atomic_load(&port_open));
 printf("off_during_open stop_timed_out=%d eventually_disabled=%d\n",stop==ESP_ERR_TIMEOUT,latest().service_state==BOT_AUDIO_DISABLED);
 atomic_store(&hold_open,false);atomic_store(&read_timeout,true);before=atomic_load(&opens);
 check(bot_audio_start()==ESP_OK);check(wait_state(BOT_AUDIO_FAULT,3000));
 unsigned attempts=atomic_load(&opens)-before;check(attempts==3);check(!atomic_load(&port_open));
 printf("read_timeout attempts=%u fault=%d\n",attempts,latest().service_state==BOT_AUDIO_FAULT);
 atomic_store(&read_timeout,false);check(bot_audio_stop(500)==ESP_OK);
 check(bot_audio_start()==ESP_OK);check(wait_state(BOT_AUDIO_CALIBRATING,500));
 atomic_store(&cleanup_failure,true);stop=bot_audio_stop(500);check(stop!=ESP_OK);check(latest().service_state==BOT_AUDIO_FAULT);
 before=atomic_load(&opens);check(bot_audio_start()==ESP_OK);check(wait_state(BOT_AUDIO_FAULT,500));
 bool blocked=before==atomic_load(&opens);check(blocked);printf("cleanup_failure rejected_off=%d no_second_open=%d workers=%u\n",stop!=ESP_OK,blocked,atomic_load(&workers));
 atomic_store(&cleanup_failure,false);check(bot_audio_stop(500)==ESP_OK);
 printf("final closed=%d balanced=%d lifecycle_violations=%u script_failures=%u\n",!atomic_load(&port_open),atomic_load(&opens)==atomic_load(&closes),atomic_load(&violations),failures);
 return failures || atomic_load(&violations) || atomic_load(&opens)!=atomic_load(&closes) ? 1 : 0;
}
