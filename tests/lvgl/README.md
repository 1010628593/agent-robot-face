# Real LVGL host regression

This target builds the repository's actual Face/Picker/Stats C files against **LVGL 9.4.0**, pinned to `c016f72d4c125098287be5e83c0f1abed4706ee5`. Only the physical display, time source, and BSP touch-device getter are virtual. Widgets, input processing, layout, drawing, object deletion and framebuffer output use real LVGL. It is not the fake-LVGL lifecycle target in `tests/face`.

```bash
cmake -S tests/lvgl -B .build/lvgl -DCMAKE_BUILD_TYPE=Debug
cmake --build .build/lvgl --parallel
mkdir -p .build/snapshots
BOT_SNAPSHOT_DIR="$PWD/.build/snapshots" ctest --test-dir .build/lvgl --output-on-failure
```

CMake fetches the pinned source, so the first configure needs GitHub access. To reuse an existing **verified 9.4.0** source directory, add `-DFETCHCONTENT_SOURCE_DIR_LVGL=/absolute/path/to/lvgl`; do not point it at an arbitrary newer/older release. This does not touch firmware `managed_components` or its lock file.

Tests cover the complete three-screen route, central-card confirmation hit testing, heading updates, quota semantics and full-label width, terminal-pose restoration, off-screen event timing and 100 page-change cycles. PPM output is 466x466 **software-rendered pixels**, not a photograph, panel-FPS measurement, or physical touch test.

## Sanitizers and one explicit upstream exception

A strict Clang `-fsanitize=address,undefined` run found an upstream incompatible-function-pointer call at `lv_draw_sw_mask.c:102`, inside `lv_draw_sw_mask_apply`. LVGL stores typed radius/line mask callbacks behind a common callback with a `void *` parameter. This fails the function-type sanitizer before the application tests can run.

The original failure is preserved in GitHub Actions run `34080580899`, artifact `host-ui-clang-sanitized`. It is **not fixed**, and must not be described as a clean strict-UBSan result.

The CI compatibility run explicitly enables `BOT_LVGL_MASK_CALLBACK_COMPAT=ON`, passing `lvgl-ubsan.ignore` **only to the LVGL library target**. Its sole entry disables the `function` sanitizer check at `lv_draw_sw_mask_apply`. Address, leak and all other enabled UB checks remain enabled; application/test code receives no suppression. The option is OFF by default, and configuring ON emits a warning.

```bash
CC=clang CXX=clang++ \
CFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
CXXFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
LDFLAGS='-fsanitize=address,undefined' \
cmake -S tests/lvgl -B .build/lvgl-sanitized \
  -DCMAKE_BUILD_TYPE=Debug -DBOT_LVGL_MASK_CALLBACK_COMPAT=ON
cmake --build .build/lvgl-sanitized --parallel
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir .build/lvgl-sanitized --output-on-failure
```

To reproduce strict checking, omit the compatibility option in a new build directory. Revisit/remove the exception when the project deliberately updates LVGL and verifies its callback signatures; do not silently upgrade the firmware dependency to make this test pass.

No command here flashes, erases, changes PMIC rails, installs Agent hooks, reads tokens, or fetches account data. The UI remains explicitly simulated.
