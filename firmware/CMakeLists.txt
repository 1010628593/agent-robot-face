# Bot Status firmware — ESP-IDF project (v6.1, matches evidence/idf61-build.md)
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

add_compile_options(-Wno-format)
project(bot_status)

include(${CMAKE_CURRENT_LIST_DIR}/bsp_dma_buffers.cmake)
