# Audio UI integration replay — 2026-09-08

These are synthetic audio/model snapshots passed through the production LVGL UI, not microphone or hardware acceptance. No unit tests were added. PPM frames stay in `/tmp`; only the two visually inspected images, source traces and CSV summaries are retained here.

## Result

- All three existing LVGL host executables compile with `-Wall -Wextra -Werror`.
- LOUD uses a smooth 0–90 ms attack, holds from 90–180 ms, then smoothly releases through 480 ms. These durations start at UI acceptance after the correlation wait.
- `sustained-quiet`: SUSTAINED is accepted first; a subsequent QUIET replaces its active envelope while preserving LOUD priority. At 1500 ms the QUIET response differs from the no-event control by 3004 RGB bytes; it is identical again at 1900 ms, well before the original 3-second sustained envelope would finish.
- `loud`: image equals the no-event control initially; visible differences at 1200/1300 ms; returns byte-for-byte to control at 1600 ms.
- `rhythm`: a real injected beat causes a bounded visible difference at 1150 ms; matches control again at 1300 ms. This does not prove acoustic rhythm detection.
- `wait`, `error`, `done`, `touch`, `motion`, `hidden`: every captured image matches its identical model/touch/motion control without audio events. The CSV records the expected task/touch/motion/hidden suppression. Hidden events do not play on return to Face.
- `close`: explicit OFF at 1200 ms cancels the local reaction immediately; the previous raster remains at 1210 ms, then the next 20 ms Face frame at 1220 ms is byte-for-byte identical to OFF control. No blocking hardware operation occurs in UI.
- `settings`: touch opens options, selects Natural, displays injected FAULT, expands diagnostics with N/A rather than fabricated digital zero, and closes the overlay. Original Stats state remains underneath. On return to Face, an explicit change produces a small upright actual-state acknowledgement for at most 1200 ms; it disappears by the 2300 ms capture.
- `options-fault.png` and `face-feedback.png` were visually inspected: glyphs, diagnostic line fit, transient label placement and expression rendering are readable.

The whole input trace is available per scenario (`name.txt`) with its matching no-event control (`name-control.txt`). `pixel-comparison.csv` reports differing RGB bytes; zeros mean identical full rendered frames, not an approximate visual comparison. CSV rendering CPU times describe this host only.

## Exact replay commands

Run from repository root (the installed LVGL component is the same local dependency used for the existing host build):

```sh
cmake -S tests/lvgl -B /tmp/robot-face-lvgl -DFETCHCONTENT_SOURCE_DIR_LVGL="$PWD/firmware/managed_components/lvgl__lvgl"
cmake --build /tmp/robot-face-lvgl -j6
sh reports/audio/ui/replay.sh /tmp/robot-face-lvgl/replay_os_ui /tmp/audio-ui-full
sips -s format png /tmp/audio-ui-full/settings/0010.ppm --out reports/audio/ui/options-fault.png
sips -s format png /tmp/audio-ui-full/settings/0016.ppm --out reports/audio/ui/face-feedback.png
```

The runner's additional commands are `agent <state enum>`, `mode <0|1|2>`, `audio <service> <event> <id> <beat> <epoch>`, `motion <linear_g> <gyro_dps>`, `stats`, and `face`. Numeric touch rows retain the original runner format. Injected audio features stay fresh, while event timestamps remain fixed until the next injection. Physical capture, background DSP, and service lifecycle are outside this runner.

## Existing regression baseline

Both current tree and an independent archive of baseline commit `231f590af57aaa32f6e5496f626a520768160983` produce the same 5/14 passes and 9/14 failures. The archive uses the same local LVGL source. Failures are retained in `existing-regressions-current.log` and `existing-regressions-baseline.log`:

- lvgl_production
- lvgl_smoke
- lvgl_stats_tab
- lvgl_picker_target
- lvgl_terminal_hidden
- lvgl_cycling
- lvgl_motion_input_navigation
- lvgl_motion_input_contact
- lvgl_motion_input_stale

Baseline reproduction:

```sh
mkdir -p /tmp/audio-ui-baseline
git archive 231f590af57aaa32f6e5496f626a520768160983 firmware/components tests/lvgl tools | tar -x -C /tmp/audio-ui-baseline
cmake -S /tmp/audio-ui-baseline/tests/lvgl -B /tmp/audio-ui-baseline-build -DFETCHCONTENT_SOURCE_DIR_LVGL="$PWD/firmware/managed_components/lvgl__lvgl"
cmake --build /tmp/audio-ui-baseline-build -j8
ctest --test-dir /tmp/audio-ui-baseline-build --output-on-failure
ctest --test-dir /tmp/robot-face-lvgl --output-on-failure
```

Still unverified: hardware start/stop, physical sound and movement correlation latency, IMU dizzy interaction on device, calibrated sensitivity/false positives, measured 45°/90° target orientation under audio, and 8-hour stability. Existing baseline rotation/motion tests are not silently counted as passing acceptance.
