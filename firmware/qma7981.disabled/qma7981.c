#include "qma7981.h"
#include <math.h>

static const char *TAG = "qma7981";

/* QMA7981设备结构体 */
struct qma7981_dev_t {
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;
    uint8_t i2c_addr;
    uint8_t range;
    SemaphoreHandle_t read_mux;
};

/* 内部函数：I2C读寄存器 */
static esp_err_t qma7981_read_reg(qma7981_handle_t handle, uint8_t reg_addr, uint8_t *data, size_t len)
{
    if (handle == NULL || handle->dev_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = i2c_master_transmit_receive(handle->dev_handle, &reg_addr, 1, data, len, 1000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C read failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

/* 内部函数：I2C写寄存器 */
static esp_err_t qma7981_write_reg(qma7981_handle_t handle, uint8_t reg_addr, uint8_t data)
{
    if (handle == NULL || handle->dev_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t write_buf[2] = {reg_addr, data};
    esp_err_t ret = i2c_master_transmit(handle->dev_handle, write_buf, sizeof(write_buf), 1000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C write failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t qma7981_init(const qma7981_config_t *config, qma7981_handle_t *handle)
{
    if (config == NULL || handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (config->bus_handle == NULL) {
        ESP_LOGE(TAG, "I2C bus handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    /* 分配设备结构体 */
    struct qma7981_dev_t *dev = malloc(sizeof(struct qma7981_dev_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "Memory allocation failed");
        return ESP_ERR_NO_MEM;
    }
    
    dev->bus_handle = config->bus_handle;
    dev->i2c_addr = config->i2c_addr;
    dev->range = config->range;
    dev->read_mux = xSemaphoreCreateMutex();
    if (dev->read_mux == NULL) {
        ESP_LOGE(TAG, "Mutex creation failed");
        free(dev);
        return ESP_ERR_NO_MEM;
    }
    
    /* 配置I2C设备 */
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = config->i2c_addr,
        .scl_speed_hz = 100000,  /* 100kHz */
    };
    
    esp_err_t ret = i2c_master_bus_add_device(config->bus_handle, &dev_cfg, &dev->dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add I2C device: %s", esp_err_to_name(ret));
        vSemaphoreDelete(dev->read_mux);
        free(dev);
        return ret;
    }
    
    /* 检查设备ID */
    uint8_t device_id;
    ret = qma7981_read_device_id(dev, &device_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read device ID");
        i2c_master_bus_rm_device(dev->dev_handle);
        vSemaphoreDelete(dev->read_mux);
        free(dev);
        return ret;
    }
    
    if (device_id != 0xE7) {
        ESP_LOGE(TAG, "Invalid device ID: 0x%02X, expected 0xE7", device_id);
        i2c_master_bus_rm_device(dev->dev_handle);
        vSemaphoreDelete(dev->read_mux);
        free(dev);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "QMA7981 found, device ID: 0x%02X", device_id);
    
    /* 设置为活动模式 */
    ret = qma7981_write_reg(dev, QMA7981_MODE_ADDR, QMA7981_MODE_ACTIVE_100K);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set active mode");
        i2c_master_bus_rm_device(dev->dev_handle);
        vSemaphoreDelete(dev->read_mux);
        free(dev);
        return ret;
    }
    
    /* 设置量程 */
    ret = qma7981_set_range(dev, config->range);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set range");
        i2c_master_bus_rm_device(dev->dev_handle);
        vSemaphoreDelete(dev->read_mux);
        free(dev);
        return ret;
    }
    
    ESP_LOGI(TAG, "QMA7981 initialized, range: %dg", 
             (config->range == QMA7981_RANGE_2G) ? 2 :
             (config->range == QMA7981_RANGE_4G) ? 4 :
             (config->range == QMA7981_RANGE_8G) ? 8 :
             (config->range == QMA7981_RANGE_16G) ? 16 : 32);
    
    *handle = dev;
    return ESP_OK;
}

esp_err_t qma7981_read_accel(qma7981_handle_t handle, float *x, float *y, float *z)
{
    if (handle == NULL || x == NULL || y == NULL || z == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t data[6];
    esp_err_t ret;
    
    /* 获取互斥锁 */
    if (xSemaphoreTake(handle->read_mux, pdMS_TO_TICKS(1000)) != pdPASS) {
        ESP_LOGE(TAG, "Failed to take mutex");
        return ESP_FAIL;
    }
    
    /* 读取6个字节的加速度数据 (X, Y, Z各2字节) */
    ret = qma7981_read_reg(handle, QMA7981_DXM_ADDR, data, 6);
    if (ret != ESP_OK) {
        xSemaphoreGive(handle->read_mux);
        return ret;
    }
    
    /* 释放互斥锁 */
    xSemaphoreGive(handle->read_mux);
    
    /* 解析X轴数据 */
    int16_t raw_x = (int16_t)((data[0] & 0xFC) | (data[1] << 8));
    *x = (float)raw_x / QMA7981_MAX_VALUE;
    
    /* 解析Y轴数据 */
    int16_t raw_y = (int16_t)((data[2] & 0xFC) | (data[3] << 8));
    *y = (float)raw_y / QMA7981_MAX_VALUE;
    
    /* 解析Z轴数据 */
    int16_t raw_z = (int16_t)((data[4] & 0xFC) | (data[5] << 8));
    *z = (float)raw_z / QMA7981_MAX_VALUE;
    
    return ESP_OK;
}

esp_err_t qma7981_read_device_id(qma7981_handle_t handle, uint8_t *device_id)
{
    if (handle == NULL || device_id == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    return qma7981_read_reg(handle, QMA7981_DEVICE_ID_REG, device_id, 1);
}

esp_err_t qma7981_set_range(qma7981_handle_t handle, uint8_t range)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = qma7981_write_reg(handle, QMA7981_RANGE_ADDR, range);
    if (ret == ESP_OK) {
        handle->range = range;
    }
    return ret;
}

void qma7981_calc_tilt(float x, float y, float z, float *pitch, float *roll)
{
    if (pitch == NULL || roll == NULL) {
        return;
    }
    
    /* 计算俯仰角（绕X轴旋转） */
    *pitch = atan2f(y, sqrtf(x * x + z * z)) * 180.0f / M_PI;
    
    /* 计算横滚角（绕Y轴旋转） */
    *roll = atan2f(-x, sqrtf(y * y + z * z)) * 180.0f / M_PI;
}

esp_err_t qma7981_delete(qma7981_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    /* 设置为待机模式 */
    qma7981_write_reg(handle, QMA7981_MODE_ADDR, QMA7981_MODE_STANDBY);
    
    /* 移除I2C设备 */
    if (handle->dev_handle != NULL) {
        i2c_master_bus_rm_device(handle->dev_handle);
    }
    
    /* 删除互斥锁 */
    if (handle->read_mux != NULL) {
        vSemaphoreDelete(handle->read_mux);
    }
    
    /* 释放内存 */
    free(handle);
    
    return ESP_OK;
}