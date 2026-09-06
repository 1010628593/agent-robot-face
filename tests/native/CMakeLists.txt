cmake_minimum_required(VERSION 3.20)
project(bot_native_tests C)

# Host-side compile of bot_core shared headers + native unit tests.
# Deliberately NOT an ESP-IDF component build: this validates pure C logic on
# macOS clang before any firmware flash. Firmware consumes the same headers.

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Werror)

include_directories(${CMAKE_SOURCE_DIR}/../../firmware/components/bot_core/include)

set(BOT_CORE ${CMAKE_SOURCE_DIR}/../../firmware/components/bot_core)

add_executable(test_bot_types test_bot_types.c)

add_executable(test_gesture test_gesture.c ${BOT_CORE}/gesture.c)

enable_testing()
add_test(NAME bot_types COMMAND test_bot_types)
add_test(NAME gesture COMMAND test_gesture)
