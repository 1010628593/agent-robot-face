cmake_minimum_required(VERSION 3.20)
project(bot_lvgl_tests C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
include(FetchContent)
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(lvgl
    GIT_REPOSITORY https://github.com/lvgl/lvgl.git
    GIT_TAG c016f72d4c125098287be5e83c0f1abed4706ee5)
FetchContent_MakeAvailable(lvgl)

# OFF by default. The pinned upstream mask dispatcher uses a cast callback.
# This exception applies ONLY to function-type UBSan at that LVGL callsite;
# not to application code, memory checking or any other UB check. See README.
option(BOT_LVGL_MASK_CALLBACK_COMPAT "Allow documented LVGL 9.4 callback sanitizer exception" OFF)
if(BOT_LVGL_MASK_CALLBACK_COMPAT)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "LVGL callback compatibility is a Clang-only test option")
    endif()
    message(WARNING "LVGL 9.4: known mask callback function-type check excluded; see tests/lvgl/README.md")
    target_compile_options(lvgl PRIVATE
        "-fsanitize-ignorelist=${CMAKE_CURRENT_SOURCE_DIR}/lvgl-ubsan.ignore")
endif()

set(UI "${CMAKE_CURRENT_SOURCE_DIR}/../../firmware/components/bot_ui")
set(CORE "${CMAKE_CURRENT_SOURCE_DIR}/../../firmware/components/bot_core")
set(LINK "${CMAKE_CURRENT_SOURCE_DIR}/../../firmware/components/bot_link")
set(MOTION "${CMAKE_CURRENT_SOURCE_DIR}/../../firmware/components/bot_motion")
add_executable(test_lvgl_ui test_lvgl_ui.c
    ${UI}/ui_motion.c ${UI}/ui.c ${UI}/face.c ${UI}/face_motion.c ${UI}/face_geometry.c ${UI}/face_reaction.c ${MOTION}/bot_motion.c
    ${UI}/picker.c ${UI}/stats.c ${UI}/picker_dev.c ${UI}/stats_dev.c
    ${UI}/assets/icons.c ${UI}/assets/bot_font_22.c ${UI}/assets/bot_font_24.c
    ${LINK}/bot_link.c ${CORE}/bot_frame.c ${CORE}/bot_json.c ${CORE}/bot_model.c
    ${CORE}/navigation.c ${CORE}/gesture.c ${CORE}/bot_router.c)
target_include_directories(test_lvgl_ui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR} ${UI}/include ${CORE}/include ${MOTION}/include ${LINK}/include)
option(BOT_LVGL_DEV_SIM "Build legacy simulator scenarios" ON)
if(BOT_LVGL_DEV_SIM)
    target_compile_definitions(test_lvgl_ui PRIVATE CONFIG_BOT_DEV_SIM=1)
endif()
target_compile_options(test_lvgl_ui PRIVATE -Wall -Wextra -Werror)
target_link_libraries(test_lvgl_ui PRIVATE lvgl::lvgl m)
enable_testing()
foreach(case production smoke stats_tab quota_unknown quota_full picker_target terminal_resume terminal_hidden cycling motion_rotation motion_reaction)
    add_test(NAME lvgl_${case} COMMAND test_lvgl_ui ${case})
endforeach()

# Exercise inverse touch mapping through real UI routing, not only math helpers.
add_executable(test_motion_input test_motion_input.c
    ${UI}/ui_motion.c ${UI}/ui.c ${UI}/face.c ${UI}/face_motion.c ${UI}/face_geometry.c ${UI}/face_reaction.c ${MOTION}/bot_motion.c
    ${UI}/picker.c ${UI}/stats.c ${UI}/picker_dev.c ${UI}/stats_dev.c
    ${UI}/assets/icons.c ${UI}/assets/bot_font_22.c ${UI}/assets/bot_font_24.c
    ${LINK}/bot_link.c ${CORE}/bot_frame.c ${CORE}/bot_json.c ${CORE}/bot_model.c
    ${CORE}/navigation.c ${CORE}/gesture.c ${CORE}/bot_router.c)
target_include_directories(test_motion_input PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR} ${UI}/include ${CORE}/include ${MOTION}/include ${LINK}/include)
if(BOT_LVGL_DEV_SIM)
    target_compile_definitions(test_motion_input PRIVATE CONFIG_BOT_DEV_SIM=1)
endif()
target_compile_options(test_motion_input PRIVATE -Wall -Wextra -Werror)
target_link_libraries(test_motion_input PRIVATE lvgl::lvgl m)
foreach(case navigation contact stale)
    add_test(NAME lvgl_motion_input_${case} COMMAND test_motion_input ${case})
endforeach()

# Standalone diagnostic, deliberately not a new unit test or CTest assertion.
get_target_property(OS_REPLAY_SOURCES test_lvgl_ui SOURCES)
list(REMOVE_ITEM OS_REPLAY_SOURCES test_lvgl_ui.c)
add_executable(replay_os_ui ../../tools/replay_os_ui.c ${OS_REPLAY_SOURCES})
target_include_directories(replay_os_ui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR} ${UI}/include ${CORE}/include ${MOTION}/include ${LINK}/include)
target_compile_options(replay_os_ui PRIVATE -Wall -Wextra -Werror)
target_link_libraries(replay_os_ui PRIVATE lvgl::lvgl m)
