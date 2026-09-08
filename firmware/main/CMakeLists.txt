idf_component_register(
    SRCS main.c
    INCLUDE_DIRS .
    REQUIRES bot_ui bot_link bot_imu bot_audio waveshare__esp32_s3_touch_amoled_1_75)

idf_component_get_property(LVGL_LIB lvgl__lvgl COMPONENT_LIB)
target_compile_options(
    ${LVGL_LIB}
    PRIVATE
        -DLV_LVGL_H_INCLUDE_SIMPLE
        -Wno-error=attributes
)

if(BOT_DEVICE_SOAK)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE BOT_DEVICE_SOAK=1)
endif()

target_compile_options(${LVGL_LIB} PRIVATE "SHELL:-include ${CMAKE_CURRENT_LIST_DIR}/bot_lvgl_assert.h")
