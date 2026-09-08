#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief G0 audio contract probe (hardware verification only).
 *
 * Runs a bounded, self-contained verification of the audio contract on real
 * hardware. It intentionally does NOT start LVGL, the UI or the IMU, so the
 * measured numbers are not polluted by display or motion work.
 *
 * Output is emitted over the console log with the "g0" tag. A machine-readable
 * summary is printed as a single line prefixed with "@g0 ".
 *
 * Interaction required by the operator:
 *   phase A (~2s)  keep the room quiet, do not touch the board
 *   phase B (~4s)  clap / talk near the board so the microphone sees a signal
 *
 * This function prints its report and returns. It does not start any service.
 */
void bot_audio_g0_probe(void);

#ifdef __cplusplus
}
#endif
