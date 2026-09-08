# Source-controlled narrow ESP-IDF 6.1 adaptation. Upstream stays immutable.
# Espressif Systems (Shanghai) CO LTD, Apache-2.0 (retained in derived source).
# Audio-enabled builds wipe *every* I2S DMA allocation with volatile stores before
# free, including the partially filled first block that never reaches on_recv.
# A source hash intentionally fails closed on SDK drift; review before updating.
if(CONFIG_BOT_AUDIO_ENABLE)
    idf_component_get_property(bot_i2s_dir esp_driver_i2s COMPONENT_DIR)
    idf_component_get_property(bot_i2s_lib esp_driver_i2s COMPONENT_LIB)
    set(bot_i2s_source "${bot_i2s_dir}/i2s_common.c")
    file(SHA256 "${bot_i2s_source}" bot_i2s_sha)
    if(NOT bot_i2s_sha STREQUAL "7bfc9633a327eb2b04b9c0b9d8c97bc387215ed32e9c3a19a0ca84f911b0a362")
        message(FATAL_ERROR "ESP-IDF I2S source changed; review audio DMA erasure patch before building audio")
    endif()
    file(READ "${bot_i2s_source}" bot_i2s_text)
    set(bot_i2s_old_start "esp_err_t i2s_free_dma_desc(i2s_chan_handle_t handle)\n{\n    I2S_NULL_POINTER_CHECK(TAG, handle);\n    handle->dma.buf_size = 0;")
    set(bot_i2s_new_start "esp_err_t i2s_free_dma_desc(i2s_chan_handle_t handle)\n{\n    I2S_NULL_POINTER_CHECK(TAG, handle);\n    const size_t bot_wipe_size = handle->dma.buf_size;\n    handle->dma.buf_size = 0;")
    set(bot_i2s_old_free "                free(handle->dma.bufs[i]);")
    set(bot_i2s_new_free [=[                /* Bot audio privacy: volatile stores cannot be optimized away.
                 * Driver free is reached only after DMA stops, or allocation fails.
                 * Preserve buf_size before upstream resets it above. */
                volatile uint8_t *bot_wipe = handle->dma.bufs[i];
                for (size_t bot_n = 0; bot_n < bot_wipe_size; ++bot_n) {
                    bot_wipe[bot_n] = 0;
                }
#if SOC_CACHE_INTERNAL_MEM_VIA_L1CACHE
                esp_cache_msync(handle->dma.bufs[i], bot_wipe_size, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
#endif
                free(handle->dma.bufs[i]);]=])
    foreach(bot_i2s_anchor IN ITEMS "${bot_i2s_old_start}" "${bot_i2s_old_free}")
        string(FIND "${bot_i2s_text}" "${bot_i2s_anchor}" bot_i2s_at)
        if(bot_i2s_at EQUAL -1)
            message(FATAL_ERROR "I2S DMA wipe anchor missing; refusing an unpatched audio build")
        endif()
    endforeach()
    string(REPLACE "${bot_i2s_old_start}" "${bot_i2s_new_start}" bot_i2s_text "${bot_i2s_text}")
    string(REPLACE "${bot_i2s_old_free}" "${bot_i2s_new_free}" bot_i2s_text "${bot_i2s_text}")
    set(bot_i2s_derived "${CMAKE_BINARY_DIR}/bot_i2s_common.c")
    file(WRITE "${bot_i2s_derived}.in" "${bot_i2s_text}")
    configure_file("${bot_i2s_derived}.in" "${bot_i2s_derived}" COPYONLY)
    get_target_property(bot_i2s_sources ${bot_i2s_lib} SOURCES)
    set(bot_i2s_patched_sources "")
    set(bot_i2s_replaced 0)
    foreach(bot_i2s_item IN LISTS bot_i2s_sources)
        if(bot_i2s_item MATCHES "(^|/)i2s_common\\.c$")
            list(APPEND bot_i2s_patched_sources "${bot_i2s_derived}")
            math(EXPR bot_i2s_replaced "${bot_i2s_replaced} + 1")
        else()
            list(APPEND bot_i2s_patched_sources "${bot_i2s_item}")
        endif()
    endforeach()
    if(NOT bot_i2s_replaced EQUAL 1)
        message(FATAL_ERROR "Expected exactly one I2S source target for audio DMA erasure")
    endif()
    set_property(TARGET ${bot_i2s_lib} PROPERTY SOURCES "${bot_i2s_patched_sources}")
    target_include_directories(${bot_i2s_lib} PRIVATE "${bot_i2s_dir}")
endif()
