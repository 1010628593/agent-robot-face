/* main.c — Bot Status production firmware with explicit developer SIM opt-in.
 *
 * BSP starts the display, touch and the LVGL task; the UI is built once and
 * polled from an LVGL timer with the display lock held. Brightness follows
 * design/ui_tokens.json (normal 28%).
 */
#include "bsp/display.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "sdkconfig.h"
#include "lvgl.h"

#include "bot_ui.h"
#include "bot_link.h"
#include "bot_imu.h"
#include "bot_audio.h"

#ifdef CONFIG_BOT_AUDIO_G0_PROBE
#include "bot_audio_g0_probe.h"
#endif

static const char *TAG = "bot_status";
static void allocation_failed(size_t bytes,uint32_t caps,const char *function)
{
    ESP_EARLY_LOGE("bot_mem","heap_caps allocation_failed bytes=%u caps=0x%lx function=%s",
        (unsigned)bytes,(unsigned long)caps,function);
}

#ifdef CONFIG_BOT_IMU_DIAGNOSTICS
/* Count only refreshes with invalidated pixels. These are host submission
 * timings, not panel scanout or camera-measured FPS. LVGL owner context only. */
static uint32_t s_render_count;
static struct {bool ready;uint32_t updates,window_ms,avg_us,max_us;} s_diag;
static int64_t s_render_start,s_render_total,s_render_max,s_render_window;
static void render_diagnostics(lv_event_t *e)
{
    int64_t now=esp_timer_get_time();
    if(lv_event_get_code(e)==LV_EVENT_RENDER_START) {
        s_render_start=now;
        if(!s_render_window)s_render_window=now;
    } else if(lv_event_get_code(e)==LV_EVENT_RENDER_READY && s_render_start) {
        int64_t elapsed=now-s_render_start;s_render_start=0;
        s_render_count++;s_render_total+=elapsed;
        if(elapsed>s_render_max)s_render_max=elapsed;
        if(now-s_render_window>=5000000) {
            ESP_LOGI(TAG,"render: %.1f updates/s avg=%.1f max=%.1f ms (submission, not scanout)",
                (double)s_render_count*1000000/(now-s_render_window),
                (double)s_render_total/s_render_count/1000,(double)s_render_max/1000);
            s_diag.updates=s_render_count;s_diag.window_ms=(uint32_t)((now-s_render_window)/1000);
            s_diag.avg_us=(uint32_t)(s_render_total/s_render_count);s_diag.max_us=(uint32_t)s_render_max;s_diag.ready=true;
            s_render_count=0;s_render_total=0;s_render_max=0;s_render_window=now;
        }
    }
}
#endif

extern uint8_t stats_depth(void);
#include "device_soak.inc"
static void ui_timer_cb(lv_timer_t *t)
{
    (void)t;
#ifdef CONFIG_BOT_IMU_DIAGNOSTICS
    static uint32_t heap_at;
    uint32_t heap_now=lv_tick_get();
    if(heap_now-heap_at>=5000){
        heap_at=heap_now;
        char heap_line[256];
        snprintf(heap_line,sizeof(heap_line),"@heap {\"uptime_ms\":%lu,\"internal_free\":%u,\"largest_free\":%u,\"minimum_free\":%u,\"psram_free\":%u,\"screen\":%u}\n",
            (unsigned long)heap_now,(unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
            (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
            (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
            (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM), (unsigned)g_ui.screen);
        bot_link_trace(heap_line);
        bot_link_health_trace();
    }
#endif
    bot_motion_view_t motion;
    bot_imu_latest(lv_tick_get(),&motion);
    bot_ui_set_motion(&motion);
    bot_audio_view_t audio;
    bot_audio_latest(lv_tick_get(),&audio);
    bot_ui_set_audio(&audio);
    bot_ui_poll();
    bot_audio_set_environment(bot_ui_audio_background_suppressed(),motion.available);
    bot_audio_config_t audio_config;
    if(bot_ui_take_audio_config(&audio_config))bot_audio_request_config(&audio_config);
#ifdef BOT_DEVICE_SOAK
    device_soak_tick(lv_tick_get());
#endif
#ifdef CONFIG_BOT_IMU_DIAGNOSTICS
    if(s_diag.ready){s_diag.ready=false;bot_link_diagnostics(s_diag.updates,s_diag.window_ms,s_diag.avg_us,s_diag.max_us,&g_ui.model,
        g_ui.selected<BOT_AGENT_COUNT?g_ui.agents[g_ui.selected].state:BOT_STATE_UNKNOWN,g_ui.screen,stats_depth());}
#endif
}

void app_main(void)
{
    heap_caps_register_failed_alloc_callback(allocation_failed);
#ifdef CONFIG_BOT_AUDIO_G0_PROBE
    /* Contract verification build: probe the microphone, print facts, stop.
     * LVGL and the IMU are intentionally not started so the numbers are clean. */
    bot_audio_g0_probe();
    ESP_LOGI(TAG, "G0 probe done; halting (this build never starts the UI)");
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
#endif
    lv_display_t *display = bsp_display_start();
    if (!display) {
        ESP_LOGE(TAG, "bsp_display_start failed");
        return;
    }
    /* design/ui_tokens.json brightness_percent.normal = 28 */
    bsp_display_brightness_set(28);

    esp_err_t audio_ret=bot_audio_init();
    if(audio_ret!=ESP_OK)ESP_LOGW(TAG,"audio service unavailable: %s",esp_err_to_name(audio_ret));
    esp_err_t imu_ret=bot_imu_start();
    if(imu_ret!=ESP_OK)ESP_LOGW(TAG,"motion service unavailable: %s",esp_err_to_name(imu_ret));
    esp_err_t lock_ret = bsp_display_lock((uint32_t)-1);
    if (lock_ret == ESP_OK) {
#ifdef CONFIG_BOT_IMU_DIAGNOSTICS
        lv_display_add_event_cb(display,render_diagnostics,LV_EVENT_RENDER_START,NULL);
        lv_display_add_event_cb(display,render_diagnostics,LV_EVENT_RENDER_READY,NULL);
#endif
        bot_ui_init();
        lv_timer_create(ui_timer_cb, 10, NULL);
        bsp_display_unlock();
        ESP_LOGI(TAG, "bot_status UI started (466x466)");
    } else {
        ESP_LOGE(TAG, "bsp_display_lock failed: ret=0x%x", lock_ret);
    }
}
