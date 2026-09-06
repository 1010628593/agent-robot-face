# Minimal face animation implementation record

Base repository commit: `d17012e4f2c83c584ac952c04c9b85804f7066f6`.
Scope: implement the approved eye-only animation design in existing ESP-IDF/LVGL UI; preserve selector, statistics routes and SIM provenance. No hardware write, branch force-push, source adapter or account mutation.

## Changes

- Pure-C deterministic motion engine with 18 visual poses and the existing eight-state mapping.
- 220ms interruptible blends, centered gradual blinking, gaze, touch, real smile arcs and X strokes.
- DONE/ERROR one-shot introduction followed by a held final pose; duplicate state/key does not replay.
- No decorative rings/discs or permanent Face labels. SIM stays visible at x209/y24, above page headings.
- One local drawing surface, fixed primitive buffers, 33ms sampling cap and pixel-level dirty comparison.
- Removed SIM-state-driven screen recreation; unified clocks on LVGL ticks.
- Process movement before HOLD and final displacement before UP; keep the existing gesture table.
- Native test target plus offline viewer exported from actual C geometry.

## Verification performed

Environment: Linux container, GCC 14 / Clang 17, CMake; no board access or ESP-IDF installation. Repository inspection used the GitHub connector. Host validation used a selected-file mirror: current UI/types, motion/geometry and the inspected gesture/router behavior. It is not a full-repository checkout or full-repository test result.

- RED: motion test initially failed to compile because the new engine did not exist.
- RED: original UI lifecycle failed the assertion that a SIM status update must not rebuild the screen.
- RED: badge safe-area assertion caught overlap with the selector heading; corrected to y24.
- GREEN: `cmake -S tests/face -B .build/face -DCMAKE_BUILD_TYPE=Debug`, build and CTest: **2/2 executables passed**. Motion executable evaluated 157647 assertions over mappings, timings, interruptions, terminal states, touch, wraparound and all 18 geometry sequences.
- GREEN: same two executables built with Clang AddressSanitizer + UndefinedBehaviorSanitizer: **2/2 passed**.
- UI lifecycle uses a test-only fake LVGL/BSP/indev seam; validates control flow and draw descriptors, not LVGL rasterization or a physical panel.
- Offline viewer: Chromium/Playwright, 1280px desktop and 390px mobile, injected self-contained HTML because browser URL navigation is blocked in this environment. Verified 19 canvases, expression selection, Done/Error timeline scrub, no horizontal overflow, no page errors. Screenshots visually inspected.
- Browser review caught a negative frame index during selection/RAF timing; clamped elapsed frames at zero and retested.

## Not verified / BLOCKED_HARDWARE

- Full ESP-IDF v6.1 firmware build with the repository's locked BSP/LVGL.
- Actual display-driver draw correctness, refresh throughput, USB/power behavior or touch-controller timing.
- The existing complete native/Bridge/source integration suites.
- Hardware flashing, battery/AMOLED lifetime, eight-hour stability, or live four-Agent data.

These are local follow-up acceptance tasks, not completed claims. The UI remains SIM. See `docs/face-animation.md` for commands and the hardware checklist.
