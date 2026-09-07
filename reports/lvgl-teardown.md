# Headless LVGL device ownership

Run `34080925483` passed all 16 GCC test executables. The Clang real-LVGL scenarios completed their behavioral checks but stopped during global `lv_deinit()` cleanup: `lv_ll_clear_custom` called the typed `lv_indev_delete` through a generic `void (*)(void *)` callback.

The harness now deletes the input device and display it owns directly through `lv_indev_delete(pointer)` and `lv_display_delete(display)`, then calls `lv_deinit()`. This uses the normal typed public APIs, exercises widget deletion callbacks, and leaves the global device lists empty for final cleanup. It does not skip cleanup, disable leak checks, change application lifecycle code, or add another sanitizer suppression.

The only compatibility exception remains the documented upstream mask-dispatch function check in `tests/lvgl/lvgl-ubsan.ignore`. Strict checking without that exception is still not clean on pinned LVGL 9.4.0. See current PR checks for the result after this ownership correction.
