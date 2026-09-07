idf_component_register(
    SRCS ui.c face.c face_motion.c face_geometry.c face_reaction.c picker.c stats.c
    INCLUDE_DIRS include
    REQUIRES bot_core bot_motion lvgl esp_timer waveshare__esp32_s3_touch_amoled_1_75)
