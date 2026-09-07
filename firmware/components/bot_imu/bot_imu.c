/* Optional sensor service. Owns ONLY the QMI device handle, never the BSP bus.
 * The UI never blocks on I2C. There is one bounded latest-only mailbox. */
#include "bot_imu.h"
#include "bot_qmi8658.h"
#include "bsp/esp-bsp.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <string.h>
static QueueHandle_t s_latest;
static const char *TAG="bot_imu";
static uint32_t clock_ms(void) { return (uint32_t)(esp_timer_get_time()/1000); }
#if CONFIG_BOT_IMU_ENABLE
static int read_reg(void *ctx,uint8_t reg,uint8_t *data,size_t size) {
    return (int)i2c_master_transmit_receive((i2c_master_dev_handle_t)ctx,&reg,1,data,size,10);
}
static int write_reg(void *ctx,uint8_t reg,uint8_t value) {
    uint8_t bytes[]={reg,value};
    return (int)i2c_master_transmit((i2c_master_dev_handle_t)ctx,bytes,sizeof(bytes),10);
}
static void sensor_task(void *arg) {
    i2c_master_bus_handle_t bus=(i2c_master_bus_handle_t)arg;
    /* Three complete attempts over the service lifetime, with bounded backoff.
     * Do not restart the display/PMIC or reset a bus shared with touch. */
    for(unsigned attempt=0;attempt<3;attempt++) {
        i2c_master_dev_handle_t dev=NULL;
        i2c_device_config_t cfg={.dev_addr_length=I2C_ADDR_BIT_LEN_7,
            .device_address=BOT_QMI_ADDRESS,.scl_speed_hz=400000};
        esp_err_t ret=i2c_master_bus_add_device(bus,&cfg,&dev);
        bot_qmi_t qmi;bot_motion_t motion;
        bot_qmi_result_t result=ret==ESP_OK?bot_qmi_init(&qmi,(bot_qmi_bus_t){dev,read_reg,write_reg}):BOT_QMI_IO;
        if(result==BOT_QMI_OK) {
            bot_motion_init(&motion,CONFIG_BOT_IMU_MOUNT_DEG,
#ifdef CONFIG_BOT_IMU_REVERSE_ROTATION
                            -1
#else
                            1
#endif
            );
            ESP_LOGI(TAG,"QMI8658 WHO_AM_I=05 addr=6B; +/-4g +/-1024dps; keep still 1s for gyro bias");
            vTaskDelay(pdMS_TO_TICKS(100));
            uint32_t last_ok=clock_ms(),last_log=last_ok;
            unsigned errors=0;
            TickType_t wake=xTaskGetTickCount();
            while(true) {
                bot_motion_sample_t sample;
                result=bot_qmi_read(&qmi,clock_ms(),&sample);
                if(result==BOT_QMI_OK) {
                    sample.ms=clock_ms();
                    if(bot_motion_feed(&motion,&sample)) {
                        bot_motion_view_t view=bot_motion_view(&motion,sample.ms);
                        xQueueOverwrite(s_latest,&view);last_ok=sample.ms;errors=0;
#ifdef CONFIG_BOT_IMU_DIAGNOSTICS
                        if(sample.ms-last_log>=1000) {
                            last_log=sample.ms;
                            ESP_LOGI(TAG,"a[g]=%.3f,%.3f,%.3f w[dps]=%.2f,%.2f,%.2f angle=%.1f flat=%d bias=%d reaction=%d",
                                (double)sample.accel[0],(double)sample.accel[1],(double)sample.accel[2],
                                (double)sample.gyro[0],(double)sample.gyro[1],(double)sample.gyro[2],
                                (double)view.rotation_deg,view.flat,view.gyro_calibrated,view.reaction);
                        }
#endif
                    }
                } else if(result!=BOT_QMI_NOT_READY)errors++;
                if(errors>=5 || clock_ms()-last_ok>1000)break;
                vTaskDelayUntil(&wake,pdMS_TO_TICKS(10)>0?pdMS_TO_TICKS(10):1);
            }
            (void)last_log;
        }
        bot_motion_view_t unavailable={0};xQueueOverwrite(s_latest,&unavailable);
        if(dev)i2c_master_bus_rm_device(dev);
        ESP_LOGW(TAG,"motion unavailable (attempt %u/3, result %d); Face and touch remain active",attempt+1,result);
        if(attempt<2)vTaskDelay(pdMS_TO_TICKS(1000u<<attempt));
    }
    ESP_LOGW(TAG,"IMU stopped after bounded retries; inspect identity/axis logs, reboot to retry");
    vTaskDelete(NULL);
}
#endif
esp_err_t bot_imu_start(void) {
#if CONFIG_BOT_IMU_ENABLE
    if(s_latest)return ESP_ERR_INVALID_STATE;
    i2c_master_bus_handle_t bus=bsp_i2c_get_handle();
    if(!bus)return ESP_ERR_INVALID_STATE;
    s_latest=xQueueCreate(1,sizeof(bot_motion_view_t));
    if(!s_latest)return ESP_ERR_NO_MEM;
    if(xTaskCreate(sensor_task,"bot_imu",4096,bus,3,NULL)!=pdPASS) {
        vQueueDelete(s_latest);s_latest=NULL;return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
#else
    ESP_LOGI(TAG,"motion disabled by configuration");return ESP_ERR_NOT_SUPPORTED;
#endif
}
bool bot_imu_latest(uint32_t ui_now,bot_motion_view_t *out) {
    if(!out)return false;
    memset(out,0,sizeof(*out));
    if(!s_latest || xQueuePeek(s_latest,out,0)!=pdTRUE)return false;
    uint32_t now=clock_ms(),age=now-out->sampled_ms,event_age=now-out->event_ms;
    out->sampled_ms=ui_now-age;out->event_ms=ui_now-event_age;
    if(!out->available || age>BOT_MOTION_STALE_MS) {
        out->available=false;out->orientation_valid=false;out->reaction=BOT_REACTION_NONE;return false;
    }
    return true;
}
