# Touch interactions 2.1.0 — implementation and verification

## Delivered behavior

- Top-edge down opens Agent selection; bottom-edge up opens usage at 今日统计. Picker up and usage down return to Face with a 220 ms directional slide.
- Long-hold navigation and home horizontal navigation are removed. Left/right edges remain reserved. Production tags and developer usage/quota tags use release-arbitrated taps.
- Complete two-point CST9217 input, stable hardware IDs, no parallel reader, ACK handling, cancel-on-error/overflow, 35 ms all-up settling and frozen face orientation.
- Region taps, no-cooldown repeated taps, live stroking, independent two-eye closure, pinch/spread, midpoint following and soft release. Working/tool amplitude is reduced; protected task states keep their meaning.
- Current behavior and interfaces: [touch-interactions.md](../../docs/touch-interactions.md).

## Verification performed

| Evidence | Result / boundary |
|---|---|
| ESP-IDF v6.1 production build | Passed; final image 854944 bytes, 90% of app partition remains free. See build.log and firmware.json. |
| Strict C compilation of replay exporter | Passed with `-std=c99 -Wall -Wextra -Werror`. Uses the actual recognizer, motion engine and geometry. |
| Ten navigation recordings | Top slow down and bottom up emit the intended events. Short, diagonal, interior-to-edge, reserved side, long holds and dual-contact cancellation emit no navigation. Error cancellation resumes only after all-up. See navigation-replay.json and matching .touch/.jsonl files. |
| Twelve expression recordings | Exported and visually inspected: local blink, convergence, forehead/cheek, both eyes, one-side release, pinch/spread, petting, repeated poke, held face, recovery. See touch-preview.png. These are C-geometry renders, not hardware captures. |
| Existing real LVGL suite | 6/14 passed: production projection, quota unknown/full, terminal resume, motion rotation, motion reaction. Eight fail against retired navigation or immediate-return expectations; see breakdown below. |
| Existing native suite | 2/6 passed (types and device model). Gesture/router/coalesced fixtures retain retired hold/navigation expectations. Frame parser has 20 failures against the existing protocol implementation; parser/model source bytes are unchanged from the start of this task. |
| Existing legacy face suite | Whole build blocked by its missing bot_link include dependency. The separately built motion executable fails CANCELLED→SLEEP mapping; the identical failure was reproduced against the task-start source snapshot. |
| Source hygiene | `git diff --check` passed. No new unit tests, no test assertions rewritten, no commit or push. |

The eight LVGL failures are smoke (650 ms hold opens picker), stats_tab (vertical swipe toggles), terminal_hidden (right swipe returns), three motion_input cases (home horizontal swipe opens stats), and picker_target/cycling (assert immediate Face return before the new 220 ms dismissal finishes). Existing fixtures were retained so obsolete expectations remain visible rather than masking failures.

## Device deployment evidence

The connected ESP32-S3 at `/dev/cu.usbmodem21201` was updated through the existing `tools/bridge flash` pause/resume flow. The previous entire 8 MiB application partition was read before writing and its checksum recorded in firmware.json. The ignored local `before-touch-app.bin` is the rollback artifact; do not commit or publish it. Only the application at 0x10000 was written. Bootloader, partition table and eFuses were not changed. Esptool verified the written hash; see flash-final.log.

The Bridge was initially unable to accept HTTP connections because its existing process exhausted file descriptors (`OSError: [Errno 24] Too many open files`). A project-service restart restored API and USB access; this task did not change Bridge source or claim to fix the underlying resource leak.

The post-flash Bridge readback reports firmware **2.1.0**, protocol **2**, live connection and real working-state projection. The automatic selection and selection revision were preserved. See device-after.json. Render diagnostic durations are submission measurements, not panel FPS.

## Hardware acceptance still pending

Physical two-finger accuracy is not established by a successful flash, a USB hello or an offline replay. A request for user-operated device verification is pending. Do not mark the overall hardware gate passed until physical results are available:

- Both fingers reported together; crossing paths; lifting either finger; brief dropout; rotated contacts.
- Top/bottom openings and reverse dismissal; no long-hold navigation; short/diagonal cancellation; no second-finger misnavigation; no click after a swipe.
- Visible local taps, repeated pokes, sustained strokes, two-eye closure, one-side reopening, pinch/spread and recovery.
- Agent confirmation/rejection, tab changes, task-state updates, wake consumption and repeated panel opening/closing.

Left/right style switching, sounds, vibration and persistent mood were not implemented.
