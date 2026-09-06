/* main.c — Bot Status firmware (SIM build, T06–T08/T14).
 *
 * BSP starts the display, touch and the LVGL task; the UI is built once and
 * polled from an LVGL timer with the display lock held. Brightness follows
 * design/ui_tokens.json (normal 28%).
 */
#include "bsp/display.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "lvgl.h"

#include "bot_ui.h"

static const char *TAG = "bot_status";

static void ui_timer_cb(lv_timer_t *t)
{
    (void)t;
    bot_ui_poll();
}

void app_main(void)
{
    lv_display_t *display = bsp_display_start();
    if (!display) {
        ESP_LOGE(TAG, "bsp_display_start failed");
        return;
    }
    /* design/ui_tokens.json brightness_percent.normal = 28 */
    bsp_display_brightness_set(28);

    esp_err_t lock_ret = bsp_display_lock((uint32_t)-1);
    if (lock_ret == ESP_OK) {
        bot_ui_init();
        lv_timer_create(ui_timer_cb, 10, NULL);
        bsp_display_unlock();
        ESP_LOGI(TAG, "bot_status SIM UI started (466x466)");
    } else {
        ESP_LOGE(TAG, "bsp_display_lock failed: ret=0x%x", lock_ret);
    }
}
