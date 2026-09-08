idf_component_register(
    SRCS ui_motion.c touch_input.c ui.c face.c face_audio.c face_motion.c face_geometry.c face_reaction.c picker.c stats.c
         assets/icons.c assets/bot_font_22.c assets/bot_font_24.c
    INCLUDE_DIRS include
    REQUIRES esp_lcd espressif__esp_lcd_touch espressif__esp_lvgl_adapter bot_audio_core bot_core bot_link bot_motion lvgl esp_timer waveshare__esp32_s3_touch_amoled_1_75)
if(CONFIG_BOT_DEV_SIM)
    target_sources(${COMPONENT_LIB} PRIVATE picker_dev.c stats_dev.c)
endif()
