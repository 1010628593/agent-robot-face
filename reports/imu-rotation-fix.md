# IMU rotation hardware follow-up — 2026-09-07

## Result and authorization

The user authorized flashing and hardware testing after each project change. All firmware revisions made in this follow-up were built and flashed before hardware observation. Final flashed application: 763920 bytes (0xba810), SHA256 `0c798de42b9e94d8b6f2f154607b78faeb7011ed9fe9f8a9b427d06c15591100`. No commit/push, erase-all, eFuse change or factory restore was performed. Existing factory backup checksum was verified before flashing. Board verified at /dev/cu.usbmodem21201: ESP32-S3 v0.2, 16MB Flash, 8MB PSRAM.

## Findings and changes

- User confirmed the first geometry-rendering revision greatly improved jaggies/lag, but initial orientation was still wrong.
- Device held in the user's initial upright pose measured a raw heading around 97.03 degrees, whereas mount was 0. Raw gyro at rest was about (-3.38, 3.15, -0.38)deg/s; the old 2deg/s gate prevented startup bias calibration entirely.
- Before bias is known, permit up to 10deg/s raw gyro while retaining acceleration norm/change and one-second gravity stability checks. After calibration use the strict 2deg/s corrected gate. Actual subsequent boots reached `bias=1` within about 1.1 seconds of sampling.
- After repositioning, the user explicitly confirmed the final reference. Ten stable readings were 98.60–98.80 degrees, mean 98.69. Saved local mount 98 plus fine tenths 7 = 98.7 degrees. Generic defaults remain device-independent. Later physical poses changed; a nonzero reported compensation angle alone is not proof of a remaining leveling error. Final visual level confirmation is still pending.
- Remove duplicate angle low-pass, reduce deadband to 0.15 degrees, and use actual UI elapsed time (capped at the 250ms freshness window), with 720deg/s catch-up and shortest wrapping.
- Rotate capsule/circle/stroke/lid/arc geometry before rasterization instead of rotating a full intermediate bitmap. Pill opacity is preblended on black to avoid overlapping stroke-cap alpha seams.
- Invalidate conservative old/new primitive extents rather than the entire 406x406 surface. Actual stationary animation windows dropped from roughly 22.5–24.4ms average render submission duration to roughly 4.9–6.4ms; these are similar SIM windows, not a controlled motion benchmark.
- After establishing rendering headroom, reduce frame sampling from 33ms to 20ms (50Hz target). Final device windows included 5.8ms avg / 8.1ms max and 6.5ms avg / 7.6ms max. Initial full-screen startup refresh still peaked near 28.8ms. Refresh count depends on invalidation; these numbers are not measured panel scanout FPS.

## Verification

- Final ESP-IDF v6.1 firmware build passed; bootloader/partition/application write hashes verified by esptool, followed by successful application, touch and QMI8658 startup.
- Captured 55-second diagnostic windows after flashing. Final capture has no panic or sensor-unavailable message; bias completed and remained ready. Existing CO5300 init-command and LVGL gesture-recognition warnings remain. Custom single-point gesture routing is still active.
- Existing real-LVGL integration acceptance 13/13 passed, including rotated navigation, contact lock, stale/flat input and state reactions.
- Existing motion/QMI/reaction checks 3/3 and existing face/lifecycle checks 2/2 passed. No unit tests added; existing fake-LVGL declarations were kept compatible with production drawing APIs.
- Generated 130 actual-code preview frames. An independently linked full-invalidation reference produced byte-identical PPM frames for all 130 frames, checking dirty-region coverage against full redraw. Inputs are synthetic.
- git diff --check passed.

## Evidence and remaining acceptance

See `imu-rotation-fix/final-flash.log`, `device-final.log`, `device-fine.log` (full-surface baseline), `device-dirty-region.log`, and `lvgl-results.log`.

The user reported substantial visual improvement after the first flash. Final visual horizontal alignment, continuous rotation smoothness, panel tearing and long-run stability still need the user's physical observation. No camera recording or panel FPS claim is made. Diagnostics remain enabled for follow-up. The retained mounting reference is the user-confirmed 98.7-degree pose; do not repeatedly recalibrate from an arbitrary later pose.
