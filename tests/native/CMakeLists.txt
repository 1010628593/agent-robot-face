cmake_minimum_required(VERSION 3.20)
project(bot_native_tests C)

# Host-side compile of bot_core shared headers + native unit tests.
# Deliberately NOT an ESP-IDF component build: this validates pure C logic on
# macOS clang before any firmware flash. Firmware consumes the same headers.

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Werror)

include_directories(${CMAKE_SOURCE_DIR}/../../firmware/components/bot_core/include)

add_executable(test_bot_types test_bot_types.c)

enable_testing()
add_test(NAME bot_types COMMAND test_bot_types)
