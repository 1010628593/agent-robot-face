# Formal face implementation — 2026-09-07

Implemented approved task 1 in the existing hardware checkout. Existing IMU rotation geometry, inverse touch transform, held-contact rotation lock, sample clock, and dirty-region invalidation were retained. No firmware flash or commit was performed by this worker.

## Behavior

All geometry paints are opaque, with eye color `#C9DCFF` and black pupils on open eyes. Normal open-eye pupil visibility no longer depends on semantic state or opacity. Pupils fit the eye height; closure below 16 px occludes them geometrically. Smile and error topology first collapse to an opaque 8 px line, then bend into an arch or split into X strokes. CANCELLED has its own asymmetric lowered-eye/downward-gaze pose, distinct from sleep and disconnected. Idle gets a restrained occasional glance; work remains restrained.

Tap shuffle bag: wink, surprise, curiosity, asymmetric smirk. Stroke shuffle bag: contented closed-eye arch, rubbing squint, playful shake. Each bag is exhausted before reshuffling and swaps its first output to avoid repeating the preceding bag's last action. Durations 600–1500 ms, followed by 400 ms cooldown. Inputs during active/cooldown periods are discarded, with no queue. Only IDLE/WORKING/TOOL admit reactions; important expression changes immediately remove pet and touch overlays at the next 20 ms frame (within the 120 ms requirement). IMU reactions retain their existing state-priority suppression.

## Touch contract for the next UI worker

`bot_gesture_set_face_mode(&s_gesture,g_ui.screen==BOT_SCR_FACE)` must remain immediately before fresh DOWN, after `face_map_input`. `bot_gesture_feed` latches `pet_contact` from DOWN distance to (233,233), radius 190. A central contact stays central even if it later leaves the circle, and an edge contact stays navigation-owned even if it enters. Central contacts never produce navigation swipes. Edge contacts emit horizontal navigation only. Other pages retain original axis gestures. HOLD still fires at 650 ms, displacement 13 px cancels HOLD/TAP, and wake contact consumption remains unchanged. Stroke requires path >=40 px plus two direction reversals of >=8 px on the initial meaningful motion axis; it emits once on release.

`touch_pump` sends central tracking to `face_touch`, dispatches central TAP/STROKE to `face_pet(bool stroke,now)`, then consumes that event before `bot_route`. HOLD continues to route to Picker. All routing changes are confined to this pump and the pure recognizer. `BOT_FACE_CANCELLED` was appended before COUNT and `BOT_GESTURE_STROKE` appended after existing gesture values to preserve numeric compatibility.

## Validation and artifacts

- Existing cached real LVGL 9.4 build: `/private/tmp/robot-face-lvgl`; rebuilt actual application C sources with `-Wall -Wextra -Werror`.
- Existing suite: **13/13 PASS**; log `reports/formal-face/lvgl-results.log`. Existing navigation scenarios now start in the edge ring, including +90/+45 inverse mapping and held-contact lock. Existing reaction assertions now require the persistent centered black pupil instead of a formerly absent pupil.
- Existing `motion_preview` scenario extended to capture seven pet selections and CANCELLED, then original orientation/dizzy frames. It ran successfully. All software-rendered PPM frames: `reports/formal-face/frames/`. PNG review files: `pet-0.png`, `pet-4.png`, `cancelled.png`; inspected those actual LVGL pixels.
- No new unit tests. No browser or mobile tests. Existing SIM labels remain in this intermediate UI batch and belong to subsequent production UI work; these images are synthetic LVGL rendering evidence, not observed source activity or hardware photographs.
- Physical touch behavior, panel appearance, firmware build/flash and serial acceptance remain controller work. Ready for controller firmware build/flash batch.

## Controller hardware batch
ESP-IDF build passed; esptool flash exit0 and boot captured. Firmware 765600 bytes, SHA256 1c6c01deb441f823f656d6fb3019b09b927ddc8b6e3f09923c78f6fe6f97f7e9. Hardware log formal-face/hardware.log: steady submission averages3.0–6.7ms, normal maxima4.0–7.1ms, first-window maximum28.7ms. Update frequency depends on invalidated pixels and is not panel FPS. Physical visual/touch feedback requested and pending. Independent reviewer found diagonal distance undercount; correction assigned to next firmware batch.
