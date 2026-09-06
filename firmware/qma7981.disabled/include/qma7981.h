#pragma once

#include <stdio.h>
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "sdkconfig.h"
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* QMA7981 I2C地址 (AD0接GND时为0x12，接VDD时为0x13) */
#define QMA7981_ADDR  0x12

/* QMA7981寄存器地址 */
#define QMA7981_DEVICE_ID_REG    0x00  /* 设备ID寄存器，应返回0xE7 */
#define QMA7981_DXM_ADDR        0x01  /* X轴加速度低字节 */
#define QMA7981_DYM_ADDR        0x03  /* Y轴加速度低字节 */
#define QMA7981_DZM_ADDR        0x05  /* Z轴加速度低字节 */
#define QMA7981_MODE_ADDR       0x11  /* 模式控制寄存器 */
#define QMA7981_RANGE_ADDR      0x10  /* 量程控制寄存器 */

/* QMA7981模式命令 */
#define QMA7981_MODE_STANDBY     0x00  /* 待机模式 */
#define QMA7981_MODE_ACTIVE_100K 0x83  /* 活动模式，100kHz */
#define QMA7981_MODE_ACTIVE_500K 0x80  /* 活动模式，500kHz */

/* QMA7981量程命令 */
#define QMA7981_RANGE_2G         0x01  /* ±2g */
#define QMA7981_RANGE_4G         0x02  /* ±4g */
#define QMA7981_RANGE_8G         0x04  /* ±8g */
#define QMA7981_RANGE_16G        0x08  /* ±16g */
#define QMA7981_RANGE_32G        0x0f  /* ±32g */

/* QMA7981最大值（14位ADC） */
#define QMA7981_MAX_VALUE        0x3FFF

/* QMA7981配置结构体 */
typedef struct {
    i2c_master_bus_handle_t bus_handle;  /* I2C总线句柄 */
    uint8_t i2c_addr;                    /* I2C设备地址 */
    uint8_t range;                       /* 量程设置 */
} qma7981_config_t;

/* QMA7981设备句柄 */
typedef struct qma7981_dev_t* qma7981_handle_t;

/**
 * @brief 初始化QMA7981加速度计
 * 
 * @param config 配置结构体
 * @param handle 返回的设备句柄
 * @return 
 *     - ESP_OK 成功
 *     - ESP_ERR_INVALID_ARG 参数错误
 *     - ESP_ERR_NO_MEM 内存分配失败
 *     - ESP_FAIL 初始化失败
 */
esp_err_t qma7981_init(const qma7981_config_t *config, qma7981_handle_t *handle);

/**
 * @brief 读取三轴加速度数据
 * 
 * @param handle 设备句柄
 * @param x 返回的X轴加速度（单位：g）
 * @param y 返回的Y轴加速度（单位：g）
 * @param z 返回的Z轴加速度（单位：g）
 * @return 
 *     - ESP_OK 成功
 *     - ESP_ERR_INVALID_ARG 参数错误
 *     - ESP_FAIL 读取失败
 */
esp_err_t qma7981_read_accel(qma7981_handle_t handle, float *x, float *y, float *z);

/**
 * @brief 读取设备ID
 * 
 * @param handle 设备句柄
 * @param device_id 返回的设备ID
 * @return 
 *     - ESP_OK 成功
 *     - ESP_FAIL 读取失败
 */
esp_err_t qma7981_read_device_id(qma7981_handle_t handle, uint8_t *device_id);

/**
 * @brief 设置QMA7981量程
 * 
 * @param handle 设备句柄
 * @param range 量程设置（QMA7981_RANGE_*宏）
 * @return 
 *     - ESP_OK 成功
 *     - ESP_FAIL 设置失败
 */
esp_err_t qma7981_set_range(qma7981_handle_t handle, uint8_t range);

/**
 * @brief 计算设备倾斜角度（基于加速度数据）
 * 
 * @param x X轴加速度（单位：g）
 * @param y Y轴加速度（单位：g）
 * @param z Z轴加速度（单位：g）
 * @param pitch 返回的俯仰角（度）
 * @param roll 返回的横滚角（度）
 */
void qma7981_calc_tilt(float x, float y, float z, float *pitch, float *roll);

/**
 * @brief 删除QMA7981设备句柄
 * 
 * @param handle 设备句柄
 * @return 
 *     - ESP_OK 成功
 *     - ESP_FAIL 失败
 */
esp_err_t qma7981_delete(qma7981_handle_t handle);

#ifdef __cplusplus
}
#endif