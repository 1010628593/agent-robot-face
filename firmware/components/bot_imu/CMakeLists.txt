idf_component_register(SRCS bot_imu.c bot_qmi8658.c INCLUDE_DIRS include
    REQUIRES bot_motion esp_driver_i2c esp_timer freertos waveshare__esp32_s3_touch_amoled_1_75)
