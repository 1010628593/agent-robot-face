#ifndef BOT_IMU_H
#define BOT_IMU_H
#include "esp_err.h"
#include "bot_motion.h"
/* Call after bsp_display_start has initialized its shared I2C bus. */
esp_err_t bot_imu_start(void);
/* Nonblocking UI-owner read. Converts sensor task times into the LVGL clock.
 * Returns false if disabled, missing, stale or calibration has no sample yet. */
bool bot_imu_latest(uint32_t ui_now,bot_motion_view_t *out);
#endif
