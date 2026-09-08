/* One persistent owner. UI exchanges bounded values, never driver handles. */
#include "bot_audio.h"
#include "bot_audio_core.h"
#include "bot_audio_port.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <string.h>

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static bot_audio_view_t s_view;
static bot_audio_config_t s_desired = {BOT_AUDIO_MODE_OFF, BOT_AUDIO_SENSITIVITY_MEDIUM};
static uint32_t s_revision, s_stopped_revision;
static bool s_freeze, s_motion_available;
static TaskHandle_t s_owner;
static uint32_t clock_ms(void) { return (uint32_t)(esp_timer_get_time()/1000); }
static bool supported(void) {
#if CONFIG_BOT_AUDIO_ENABLE
    return true;
#else
    return false;
#endif
}
static bool verified(void) {
#if CONFIG_BOT_AUDIO_G0_VERIFIED
    return true;
#else
    return false;
#endif
}
static void publish(bot_audio_view_t v) {
    v.supported=supported();v.verified=verified();
    portENTER_CRITICAL(&s_lock);s_view=v;portEXIT_CRITICAL(&s_lock);
}
static uint32_t desired(bot_audio_config_t *c, bool *freeze) {
    portENTER_CRITICAL(&s_lock);
    *c=s_desired;*freeze=s_freeze;uint32_t rev=s_revision;
    if(!s_motion_available && c->mode!=BOT_AUDIO_MODE_OFF) {
        c->mode=BOT_AUDIO_MODE_NATURAL;c->sensitivity=BOT_AUDIO_SENSITIVITY_LOW;
    }
    portEXIT_CRITICAL(&s_lock);return rev;
}
#if CONFIG_BOT_AUDIO_ENABLE
static bot_audio_core_t s_core;
static int16_t s_pcm[221];
static uint32_t s_epoch;
static void erase_audio(void) {
    /* Volatile writes retain the privacy wipe under optimization. */
    volatile unsigned char *p=(volatile unsigned char *)s_pcm;
    for(size_t i=0;i<sizeof(s_pcm);i++)p[i]=0;
    p=(volatile unsigned char *)&s_core;
    for(size_t i=0;i<sizeof(s_core);i++)p[i]=0;
}
static void state(bot_audio_service_state_t st,esp_err_t err,uint32_t revision) {
    bot_audio_view_t v={.stream_epoch=s_epoch,.service_state=st,.last_error=err,
                       .supported=true,.verified=verified()};
    portENTER_CRITICAL(&s_lock);
    if(s_revision==revision){
        s_view=v;
        if(st==BOT_AUDIO_DISABLED)s_stopped_revision=revision;
    }
    portEXIT_CRITICAL(&s_lock);
}
static esp_err_t stop_port(uint32_t rev) {
    state(BOT_AUDIO_STOPPING,ESP_OK,rev);
    esp_err_t ret=bot_audio_port_close();
    if(ret==ESP_OK)ret=bot_audio_port_deinit();
    erase_audio();
    if(ret!=ESP_OK)state(BOT_AUDIO_FAULT,ret,rev);
    return ret;
}
static bool changed(uint32_t revision) {
    bot_audio_config_t c;bool freeze;return desired(&c,&freeze)!=revision;
}
static void worker(void *arg) {
    (void)arg;uint32_t handled=0;bool have_handled=false,unsafe=false;
    for(;;) {
        bot_audio_config_t config;bool freeze;
        uint32_t rev=desired(&config,&freeze);
        if(have_handled && rev==handled){ulTaskNotifyTake(pdTRUE,portMAX_DELAY);continue;}
        handled=rev;have_handled=true;
        if(unsafe) {
            /* Never allocate a second capture after an unconfirmed stop. */
            if(stop_port(rev)!=ESP_OK)continue;
            unsafe=false;
        }
        if(config.mode==BOT_AUDIO_MODE_OFF){state(BOT_AUDIO_DISABLED,ESP_OK,rev);continue;}
        if(!verified()){state(BOT_AUDIO_FAULT,ESP_ERR_NOT_SUPPORTED,rev);continue;}
        esp_err_t failure=ESP_FAIL;
        for(unsigned attempt=0;attempt<3 && !changed(rev);attempt++) {
            if(attempt) {
                uint32_t delay_ms=attempt==1?500:1000;
                ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(delay_ms));
                if(changed(rev))break;
            }
            state(BOT_AUDIO_STARTING,ESP_OK,rev);
            bot_audio_port_config_t port={22050,16,1,10};
            failure=bot_audio_port_init(&port);
            if(failure==ESP_OK)failure=bot_audio_port_open();
            if(failure!=ESP_OK) {
                if(stop_port(rev)!=ESP_OK){unsafe=true;break;}
                continue;
            }
            bot_audio_core_init(&s_core,++s_epoch,&config);
            state(BOT_AUDIO_CALIBRATING,ESP_OK,rev);
            unsigned phase=0;bool have_published=false;uint32_t published_ms=0;
#ifdef CONFIG_BOT_AUDIO_DIAGNOSTICS
            uint32_t last_log=clock_ms();
#endif
            while(!changed(rev)) {
                size_t frames=phase?221:220,got=0;
                failure=bot_audio_port_read(s_pcm,frames*sizeof(int16_t),&got,50);
                bot_audio_port_read_meta_t meta={0};
                bot_audio_port_get_last_read(&meta);
                if(failure!=ESP_OK || got!=frames*sizeof(int16_t)) {
                    if(failure==ESP_OK)failure=ESP_ERR_INVALID_SIZE;
                    bot_audio_core_gap(&s_core,BOT_AUDIO_QUALITY_GAP);
                    break;
                }
                if(changed(rev))break;
                if(meta.clock_uncertain || meta.gap) {
                    failure=ESP_ERR_INVALID_RESPONSE;
                    bot_audio_core_gap(&s_core,BOT_AUDIO_QUALITY_GAP);
                    break; /* DMA loss invalidates the anchor until the port is reopened. */
                }
                if(meta.reconfigured) {
                    bot_audio_core_gap(&s_core,BOT_AUDIO_QUALITY_GAP);
                    state(BOT_AUDIO_CALIBRATING,ESP_OK,rev);
                    phase^=1;continue;
                }
                desired(&config,&freeze);
                if(bot_audio_core_feed(&s_core,s_pcm,frames,(uint32_t)(meta.capture_end_us/1000),freeze)) {
                    bot_audio_view_t v=*bot_audio_core_view(&s_core);
                    v.active_mics=1;v.supported=true;v.verified=true;
                    if(!have_published || (uint32_t)(v.sampled_ms-published_ms)>=20){
                    published_ms=v.sampled_ms;have_published=true;
                    portENTER_CRITICAL(&s_lock);
                    if(s_revision==rev){
                        if(!s_motion_available)v.quality_flags|=BOT_AUDIO_QUALITY_MOTION_FILTER_DEGRADED;
                        s_view=v;
                    }
                    portEXIT_CRITICAL(&s_lock);
                    }
#ifdef CONFIG_BOT_AUDIO_DIAGNOSTICS
                    uint32_t now=clock_ms();
                    if(now-last_log>=1000){last_log=now;ESP_LOGI("bot_audio","state=%d level=%u quality=%u",v.service_state,v.level,v.quality_flags);}
#endif
                }
                memset(s_pcm,0,sizeof(s_pcm));phase^=1;
            }
            if(stop_port(rev)!=ESP_OK){unsafe=true;break;}
            if(changed(rev))break;
        }
        if(!unsafe && !changed(rev))state(BOT_AUDIO_FAULT,failure,rev);
    }
}
#endif

/* Called once from app startup, before controls become interactive. */
esp_err_t bot_audio_init(void) {
#if CONFIG_BOT_AUDIO_ENABLE
    if(s_owner)return ESP_OK;
    publish((bot_audio_view_t){.service_state=BOT_AUDIO_DISABLED});
    if(xTaskCreate(worker,"bot_audio",6144,NULL,3,&s_owner)!=pdPASS) {
        publish((bot_audio_view_t){.service_state=BOT_AUDIO_FAULT,.last_error=ESP_ERR_NO_MEM});
        return ESP_ERR_NO_MEM;
    }
#else
    publish((bot_audio_view_t){.service_state=BOT_AUDIO_DISABLED});
#endif
    return ESP_OK;
}
static esp_err_t request_config(const bot_audio_config_t *c,uint32_t *accepted_revision) {
    if(!c || c->mode>BOT_AUDIO_MODE_RHYTHM || c->mode<BOT_AUDIO_MODE_OFF ||
       c->sensitivity>BOT_AUDIO_SENSITIVITY_HIGH || c->sensitivity<BOT_AUDIO_SENSITIVITY_LOW)return ESP_ERR_INVALID_ARG;
    if(c->mode!=BOT_AUDIO_MODE_OFF && (!supported() || !verified() || !s_owner)) {
        publish((bot_audio_view_t){.service_state=BOT_AUDIO_FAULT,.last_error=ESP_ERR_NOT_SUPPORTED});
        return ESP_ERR_NOT_SUPPORTED;
    }
    portENTER_CRITICAL(&s_lock);
    bool identical=s_desired.mode==c->mode && s_desired.sensitivity==c->sensitivity;
    if(identical && s_view.service_state!=BOT_AUDIO_FAULT){
        if(accepted_revision)*accepted_revision=s_revision;
        portEXIT_CRITICAL(&s_lock);return ESP_OK;
    }
    s_desired=*c;++s_revision;
    if(accepted_revision)*accepted_revision=s_revision;
    /* Immediate visual revocation; actual DISABLED is owner-confirmed. */
    s_view.available=false;s_view.event=BOT_AUDIO_EVENT_NONE;s_view.rhythm_locked=false;
    if(c->mode!=BOT_AUDIO_MODE_OFF)s_view.service_state=BOT_AUDIO_STARTING;
    else if(s_owner)s_view.service_state=BOT_AUDIO_STOPPING;
    portEXIT_CRITICAL(&s_lock);
    if(s_owner)xTaskNotifyGive(s_owner);
    else publish((bot_audio_view_t){.service_state=BOT_AUDIO_DISABLED});
    return ESP_OK;
}
esp_err_t bot_audio_request_config(const bot_audio_config_t *config) {
    return request_config(config,NULL);
}
void bot_audio_set_environment(bool freeze,bool motion_available) {
    portENTER_CRITICAL(&s_lock);
    bool reconfigure=s_motion_available!=motion_available && s_desired.mode!=BOT_AUDIO_MODE_OFF;
    s_freeze=freeze;s_motion_available=motion_available;
    if(reconfigure){++s_revision;s_view.available=false;}
    portEXIT_CRITICAL(&s_lock);
    if(reconfigure && s_owner)xTaskNotifyGive(s_owner);
}
bool bot_audio_latest(uint32_t ui_now,bot_audio_view_t *out) {
    if(!out)return false;
    portENTER_CRITICAL(&s_lock);*out=s_view;bool off=s_desired.mode==BOT_AUDIO_MODE_OFF;portEXIT_CRITICAL(&s_lock);
    uint32_t now=clock_ms(),age=now-out->sampled_ms;
    out->sampled_ms=ui_now-age;
    out->event_ms=ui_now-(now-out->event_ms);
    out->last_onset_ms=ui_now-(now-out->last_onset_ms);
    if(off || age>200)out->available=false;
    return out->available;
}
esp_err_t bot_audio_start(void) {
    bot_audio_config_t c={BOT_AUDIO_MODE_NATURAL,BOT_AUDIO_SENSITIVITY_MEDIUM};
    return bot_audio_request_config(&c);
}
esp_err_t bot_audio_stop(uint32_t timeout_ms) {
    bot_audio_config_t c={BOT_AUDIO_MODE_OFF,BOT_AUDIO_SENSITIVITY_MEDIUM};
    uint32_t requested;
    esp_err_t ret=request_config(&c,&requested);if(ret!=ESP_OK)return ret;
    uint32_t began=clock_ms();
    do {
        bot_audio_view_t v;bot_audio_latest(clock_ms(),&v);
        portENTER_CRITICAL(&s_lock);
        bool acknowledged=!s_owner || s_stopped_revision==requested;
        bool superseded=s_revision!=requested;
        portEXIT_CRITICAL(&s_lock);
        if(superseded)return ESP_ERR_INVALID_STATE;
        if(v.service_state==BOT_AUDIO_DISABLED && acknowledged)return ESP_OK;
        if(v.service_state==BOT_AUDIO_FAULT)return v.last_error?v.last_error:ESP_FAIL;
        if(clock_ms()-began>=timeout_ms)break;
        vTaskDelay(pdMS_TO_TICKS(1));
    }while(true);
    return ESP_ERR_TIMEOUT;
}
bool bot_audio_is_enabled(void) {
    bot_audio_view_t v;bot_audio_latest(clock_ms(),&v);
    return v.service_state==BOT_AUDIO_RUNNING || v.service_state==BOT_AUDIO_CALIBRATING;
}
