# Formal device implementation — 2026-09-07

Task2 implemented in the current checkout. No commit, flash, new unit-test target, browser or mobile tests. Controller owns elevated firmware build and physical acceptance. The attempted sandbox firmware build failed in ESP-IDF cmake dependency discovery because psutil's sysctl process enumeration is forbidden; no workaround or IDF edits were made.

## Production boundaries

`CONFIG_BOT_DEV_SIM` is explicit opt-in, default `n`. Production `bot_ui_model_t` owns the real `bot_model_t`; the per-agent view is only face animation projection. Old fixture statistics and quota fields are excluded on production ESP builds. The legacy simulator, its cycling and its original English pages exist only behind this flag in `ui.c`, `picker_dev.c`, `stats_dev.c`. Production init starts link/model in unknown/disconnected state. No timeout or missing-source path selects a simulator or invents zero values.

The normal face is quiet with the previous opaque eye/pupil geometry and 20ms face clock unchanged. Real focus drives selected state, stale or absent focus produces UNKNOWN/disconnected. Run identity change increments animation transition even if state name is unchanged. Picker now has monochrome product artwork: constructed OpenAI six-band mark; actual installed WorkBuddy cat and Cursor arrow/cube; official Hermes winged helmet crop. No first-letter placeholders. The fifth entry selects Auto. Confirmation remains pending until matching ACK; failures and offline state remain visible. Product names remain in original English; UI is Chinese.

Task page has visible tappable three-tab strip (当前任务 / 今日统计 / 额度), active underline and existing swipe navigation. Current task shows short sanitized run ID, waiting input versus approval, localized tool category, run elapsed snapshot, active task count, evidence quality, source health and stale status. Today shows first3 prioritized metrics with quality/source/coverage; missing remains em dash. Time is seconds and USD micros convert to dollars. Quota handles unsupported/auth/error, missing, unlimited and sourced numeric values; numeric remainder retains units and USD scaling. Source strings are ellipsized within safe zone, no raw messages or commands are displayed. Fonts are actual22/24px Regular400 Noto Sans SC subsets, 4bpp, generated C plus OFL and reproducible script; no full font blob is distributed. Actual software pixels reviewed at `reports/formal-device/pages.png` and `task-waiting.png`.

## Protocol and ownership

Binding exact wire contract is `docs/protocol-v2.md`. Decoder requires v2 and strict fields, mode in welcome/ACK, atomic host selection message, and action select with mode/agent/expected revision. Stats include sent_at_ms for accurate local age relative to source as_of. Existing strict JSON validation and8192 JSON-byte bound retained. Selection clears mismatched focus/stats, buffers newer rev snapshots, rejects older link/seq/revision, and matching same-triple ACK preserves already-applied snapshots.

New bot_link RX task only reads USB bytes into bounded32×256 chunk queue. UI owner drains at most8 chunks per tick, parses and applies model, and owns all @bot TX (hello/action/pong). No RX-thread LVGL. Handshake retry2000ms; online/action timeout6000ms; overflow resynchronizes and starts fresh handshake; v1 mismatch produces explicit upgrade message. Production ignores demo welcome. Link disconnect clears old snapshots and pending transaction. Protocol transport uses driver ring buffers and nonblocking bounded writes.

USB secondary ESP console disabled because it would introduce concurrent ROM log writers into protocol bytes; normal ESP logs remain UART. Optional existing IMU diagnostic configuration publishes render counters into a one-slot UI callback mailbox. UI timer drains it through same TX owner as `@diag {render_updates,window_ms,avg_us,max_us}` around every5s. This is CPU render submission evidence, not panel scanout FPS. Bridge may collect diagnostic lines independently of model/link sequence.

## Preserved touch and narrow review fix

The existing `touch_pump` down-time inverse transform, rotation contact lock, radius190 ownership latch,650ms HOLD,13px slop, central stroke/tap consumption and edge-only navigation remain. Gesture path accumulator is now float Euclidean distance, retaining fractional diagonal travel rather than max-axis distance; initial-axis reversal8px hysteresis and two-reversal condition unchanged. No face geometry/motion/reaction algorithms changed in this batch.

## Existing integration and rendering validation

The same real LVGL target compiles production C sources with `-Wall -Wextra -Werror`. Existing runner has a production scenario; no new executable/unit-test target. It validates decoder welcome→model, Auto entry, all4 product icons, selection pending and unchanged active agent before ACK, accepted atomic mode/agent/revision, same-triple ACK preserving current focus/stats, tappable tabs, empty/source/stale/quota error presentation and6000ms disconnect. Synthetic inputs are explicitly software integration fixtures, not evidence of any actual source lifecycle.

Production/default-off build and render invocation:

```sh
cmake -S tests/lvgl -B /private/tmp/robot-face-lvgl -DBOT_LVGL_DEV_SIM=OFF
cmake --build /private/tmp/robot-face-lvgl -j4
BOT_SNAPSHOT_DIR=reports/formal-device/frames /private/tmp/robot-face-lvgl/test_lvgl_ui production
```

Legacy developer scenarios and production scenario together:

```sh
cmake -S tests/lvgl -B /private/tmp/robot-face-lvgl -DBOT_LVGL_DEV_SIM=ON
cmake --build /private/tmp/robot-face-lvgl -j4
ctest --test-dir /private/tmp/robot-face-lvgl --output-on-failure
```

All actual PPM exports are under `reports/formal-device/frames/`. Results logged at `reports/formal-device/lvgl-results.log`. Physical UI, USB driver/reconnect, render diagnostics delivery, flash and actual source/Bridge lifecycle require controller verification; software rendering alone is not hardware acceptance. Legacy v1 native contract fixtures have not been presented as v2 verification.

Final software check after requested queue backpressure, font include and Picker mode/count refinements: **14/14 PASS**, including the production scenario in the existing executable. `git diff --check` passed. Converter uses `--lv-include lvgl.h` for ESP-IDF-compatible includes. RX capacity8192 bytes plus driver's4096 ring and at most50ms producer wait prevents a normal maximum-size burst from being mistaken for immediate queue overload. Picker current mode/agent is visible; preview active count is shown only from ready+observed catalog evidence. No active count is inferred from absent source information.

## Controller real hardware batch
ESP-IDF build and esptool flash passed. Firmware 848304 bytes SHA256 8113141dd861493078f7a62b0a1f28cb8a78667a9a89a2f9b6e05d6a7742ede4. Real USB accepted8192-byte welcome and8192-byte ping without spuriousrehandshake: {"hellos": 1, "pongs": 22, "diagnostic_windows": 9, "max_json_bytes_sent": 8192, "scope": "real USB transport only; no source task simulated"}. Nine UI-owner renderdiagnosticwindows: steady average6.5–7.0ms, ordinarymaximum~6.7–6.9ms, startupmaximum28.38ms. These are submissiondurations, notpanelFPS. Serialclosedaftertest; sourceintegrationstillrequiresBridge. Evidence hardware-v2.json andflash.log.

Subsequent physical serialclose/reopen acceptance: same boot_id, fresh handshake_id, new welcome andpong succeeded; see formal-device/reconnect.json. Independentcurrent-code reviewer verifiedallpriorfindingsresolved, no seriousremainingdevice/model/transportissue found. PhysicalUSBunplug andMacsleep/wake were not simulatedbythischeck.
