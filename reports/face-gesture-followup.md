# Face motion follow-up — coalesced gesture input

## Baseline and scope

This follow-up is based on `aa2092dcd1969dc171e930219c72bc6886221f1a`, which merged PR #1.
The existing minimal eye-only animation, shared geometry, offline preview and UI lifecycle tests are preserved. No competing face renderer is installed over them.

PR #1 already feeds the latest MOVE before TICK/UP in the UI pump. The recognizer itself still assumes MOVE was delivered: a caller sending the first displaced coordinate in TICK/UP can incorrectly get HOLD/TAP. This patch makes `bot_gesture_feed` robust independently of that caller workaround. It does not claim that PR #1's current UI pump still exhibits the same bug.

## Change

- Check displacement on MOVE, TICK and UP before classifying an event.
- Once movement exceeds 12px on either axis, TAP/HOLD stay cancelled for that contact, even after returning to its starting point.
- A valid release may still produce a directional swipe.
- Preserve 649/650ms threshold, 12/13px boundary, one event per contact, wake consumption and unsigned-clock wrap behavior.
- Add `test_gesture_coalesced` to the existing native CMake suite and a focused no-device test script.

No Face/UI/geometry, Agent adapters, billing, wire schema, BSP, dependency locks, partitions or hardware settings change.

## Verification actually performed

Environment: Linux container, GCC and Clang. No physical device, macOS or ESP-IDF toolchain was used.
The selected source snapshot was read through the GitHub connector. The baseline recognizer and required headers were verified by Git blob SHA against the above commit.

1. RED: compiling the baseline `gesture.c` with the new test fails at `tick_displacement_cancels_hold` (first 13px displacement at 650ms). Exit 134 was expected.
2. GREEN: `bash tools/test_gesture_regressions.sh` passes all original 12 cases plus 8 new regressions.
3. `SANITIZE=1 CC=clang bash tools/test_gesture_regressions.sh` passes the same 20 cases with no AddressSanitizer/UndefinedBehaviorSanitizer findings.

The new cases cover displaced TICK, displaced UP, release-only swipe, cancelled-hold-to-swipe, exact slop, vertical movement, uint32 time wrap, and wake-contact consumption.

The full repository parser/model/router suite and `tests/face` were not rerun for this follow-up. Their sources are unchanged. ESP-IDF build, LVGL rendering, real touch, panel FPS and long-duration board testing remain pending; host tests are not substitutes.

## Reproduce locally

From repository root:

```bash
bash tools/test_gesture_regressions.sh
SANITIZE=1 CC=clang bash tools/test_gesture_regressions.sh

cmake -S tests/native -B .build/native -DCMAKE_BUILD_TYPE=Debug
cmake --build .build/native --parallel
ctest --test-dir .build/native --output-on-failure

cmake -S tests/face -B .build/face -DCMAKE_BUILD_TYPE=Debug
cmake --build .build/face --parallel
ctest --test-dir .build/face --output-on-failure
```

Then use the existing EIM ESP-IDF v6.1 environment:

```bash
idf.py --version
idf.py -C firmware -B build-face-motion build
```

No script flashes or erases the board. Preserve the original Flash backup and obtain separate user confirmation before flashing.
