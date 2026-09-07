# Emotional touch layering (3.2.0)

The user found 3.1.0 responsive but emotionally flat. The current motion keeps
three visible stages: curious asymmetric opening and softened lids; comfort
building toward a modest smile; and teasing followed by staggered single-eye
peeking. Smile blending yields to independent two-eye contact, so one-side
release still works. The hand regains gaze ownership immediately on a new
contact. See `reports/touch-table/emotion-storyboard.png` and the animated replay.

# Nine-touch completion revision (3.1.0)

Head inclination is now a distinct `roll` channel that moves the eye centers;
it does not reuse the business eyelid `tilt` shape. Repeated and alternating
pokes use separate progression, and two-to-one contact retains cupping comfort.
Stroke sway comes from filtered signed touch velocity. See the current
[nine-row contract](touch-interactions.md) and `reports/touch-table/` for replay
and device evidence. Older 2.2.x behavior below is historical.

# Minimal Face Motion

Current touch behavior is defined in [Touch interactions 2.1.0](touch-interactions.md). The historical simulator and verification notes below describe the earlier renderer delivery; current production sources are documented in README.md.

This change implements the final approved direction: **the black display is the face**. It supersedes the older Face screen drawing in the handoff pack, not the Agent selector, statistics, protocol or source adapters.

## Scope and provenance

Face Home contains only pale blue-white eyes. No face disc, border/ring, permanent Agent name, status text, question mark, tool icon or mouth. A small `SIM` badge remains in the circular safe area while this repository's UI uses `sim_init`/`sim_tick`. Removing that badge without replacing the simulator with an observed data source is not an acceptable production change.

The existing firmware currently simulates its UI model. This PR improves presentation and touch handling; it does **not** connect four real Agents, fetch quota, or alter the USB protocol. It does not change ESP-IDF v6.1, BSP, LVGL versions, PMIC, partitions, display initialization, microphone or gravity sensor.

## Implementation

| File | Responsibility |
|---|---|
| `face_motion.c` / `bot_face_motion.h` | Pure C, deterministic, time-based poses; no LVGL, allocation, I/O or permissions |
| `face_geometry.c` / `bot_face_geometry.h` | Convert poses into at most 24 bounded rectangle/line/triangle/arc primitives |
| `face.c` / `bot_face.h` | One LVGL drawing surface, state synchronization and local invalidation |
| `ui.c` | Existing routes and SIM driver; continuous touch input and one shared LVGL clock |
| `tests/face` | Native motion and UI lifecycle tests, with an explicitly fake LVGL seam |
| `tools/dump_face_frames.c` | Export actual C primitives for visual inspection |
| `tools/preview_face.py` | Self-contained offline Canvas viewer; no duplicate JavaScript animation model |

Only the UI-owner context may call `face_build`, `face_tick`, `face_touch` and `face_suspend`. The BSP's existing LVGL timer remains the sole owner. No second LVGL task, framebuffer streamer, timer-handler loop or per-frame object creation is introduced.

The application keeps two fixed geometry buffers (current and next) in static storage, avoiding large stack scratch in the LVGL task. LVGL itself still allocates its normal draw tasks; this is not a claim that the entire graphics stack is allocation-free.

## Visual contract

Resolution is 466 x 466. Neutral eye centers are (170,233) and (296,233); width 74, height 108, center separation 126. Eye ink is `#C9DCFF` at 88% opacity. The face surface occupies x=72, y=108, width=322, height=244, safely inside the round panel. Black is `#000000`.

Working eyes use real sloped upper masks. Smile eyes are rounded arcs, not flattened rectangles. X eyes use rounded strokes, not font characters. Gaze pupils share one direction vector, avoiding unintended crossed eyes. Pupils fade out as eyelids close. No blur or persistent decorative halo is required.

Normal rendering is capped at one sample per 20ms (about 50Hz target, **not measured panel FPS**). Missed frames are skipped rather than replayed. Geometry is rounded for dirty comparison; unchanged pixel geometry does not invalidate the surface. The whole display is not cleared/recreated on a state update.

## Protocol-state mapping

| Existing state | Face expression | Behavior |
|---|---|---|
| idle | IDLE | Slow drift and natural blink |
| working | WORKING | Focused sloped eyes, quiet micro-motion |
| tool | TOOL | Coordinated left/right gaze with pauses |
| waiting | WAITING | Slightly larger attentive eyes; never converted to a smile on touch |
| done | DONE | One smile/bounce, then hold the smile |
| error | ERROR | One short shake, then hold X eyes until state changes |
| cancelled | SLEEP | Quiet closed eyes; cancellation is not an error |
| unknown / invalid | DISCONNECTED visual | Dim asymmetric eyes indicating uncertainty; does not prove physical disconnection |

The existing 18-value `bot_face_t` enum is retained. The preview additionally exposes blink, directional glances, curious, thinking, happy, surprised and attention. They are presentation poses, **not new invented Agent protocol states**. The shipped SIM cycler still runs the eight protocol states every seven seconds; the engine's DONE/ERROR do not themselves expire to idle.

## Timing and key poses

All times use the same `lv_tick_get()` uint32 millisecond epoch and unsigned elapsed subtraction.

- Expression change: 220ms smoothstep from the currently sampled pose, including interruption midway through an earlier transition.
- Natural blink: every 5200ms scheduling block, a seeded start offset 2400..4099ms; close 80ms, hold 50ms, reopen 130ms. Both eyes compress around their centers.
- Explicit blink preview: close at 880ms of a 2000ms cycle; closed at 960..1010ms; open by 1140ms.
- Directional look: keyframes at 0/160/520/760/1000/1200ms with gaze weights 0/1/1/0.9/0/0.
- Tool scan: 0/180/580/850/1320/1600ms with horizontal gaze 0/-0.8/-0.8/0.8/0.8/0.
- Working: 1600ms horizontal and 3200ms vertical quiet motion; no progress percentage is implied.
- Waiting: 2400ms breathing geometry; still visibly waiting when touched.
- Done: smile weights 0/0.3/1/1/1/1 at 0/120/300/520/1100/1600ms; vertical bounce 0/2/-6/0/-1/0px. Final smile persists.
- Error: cross forms over 240ms; shake keyframes 0/100/240/340/410/480/560ms with x offsets 0/0/0/-5/5/-2/0px. Stable X eyes afterwards.
- Whole-face drift: 1.2px over 240s/300s, independent of status changes. This mitigates static pixels; it does not guarantee AMOLED burn-in prevention or replace screen power management.

`bot_face_motion_set(m, expression, event_key, now)` returns false for an identical expression/key pair. Heartbeats must keep that key unchanged. A new run/terminal event can provide a new key even if the visible state remains DONE.

`bot_sim_agent_t.transition_id` is local presentation metadata, not a wire-schema addition. SIM increments it only on a state advance. A later live integration should update it on a real logical transition/run, never on every snapshot.

## Touch and navigation

The 2.1.0 input path retains both CST9217 points through one adapter-owned read and arbitrates every UI click with navigation. Top-edge down opens Agent selection; bottom-edge up opens usage. Picker up and usage down dismiss to Face. Long holds never navigate; horizontal home edges are reserved. Usage tabs are tapped directly.

Region taps, sustained strokes, repeat taps, independent eye contacts, two-finger pinch/spread and midpoint tracking are continuous motion overlays. Existing state topology is protected. See [the complete current contract](touch-interactions.md) for ownership, coordinates, timing, dropout handling and replay diagnostics.

## Build and inspect on macOS

From repository root:

```bash
cmake -S tests/face -B .build/face -DCMAKE_BUILD_TYPE=Debug
cmake --build .build/face --parallel
ctest --test-dir .build/face --output-on-failure
python3 tools/preview_face.py
open .build/face/preview.html
```

The viewer offers all 18 expressions, pause/restart and a millisecond scrubber. It contains snapshots exported by the real C motion/geometry code, not another hand-authored JS state machine. Canvas/LVGL antialiasing and arc rasterization can differ; it is not a panel capture or touch simulator.

Optional sanitizers:

```bash
cmake -S tests/face -B .build/face-asan -DCMAKE_C_COMPILER=clang \
  -DCMAKE_BUILD_TYPE=Debug \
  '-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer'
cmake --build .build/face-asan --parallel
ctest --test-dir .build/face-asan --output-on-failure
```

Then activate the **existing EIM ESP-IDF v6.1 environment**, preserve any local changes and build the firmware in a distinct directory:

```bash
idf.py --version
idf.py -C firmware -B build-face-motion build
```

Do not change the locked BSP/LVGL or clear the whole board to fix a compile issue. This change has not been compiled against the full ESP-IDF toolchain or flashed in the remote development environment. Follow existing backup and human-confirmed flashing procedures locally.

## Hardware review checklist

- [ ] Compile the actual firmware using the locked dependencies and ESP-IDF v6.1.
- [ ] Preserve original Flash backup; inspect build output before approving a flash.
- [ ] Check neutral eyes, blink center, sloped focus, smile arcs and X eyes on the AMOLED.
- [ ] Confirm no rings, cropped labels, residual old pixels or ghost pill under smile/X eyes.
- [ ] Confirm SIM remains readable without overlapping selector/stats headings.
- [ ] Holds never navigate; top down/bottom up open their panels, and adding a second finger cancels navigation.
- [ ] Swipe through selector/stats and return repeatedly without crash or terminal-animation restart.
- [ ] Measure actual panel FPS, heap and task-stack margins; do not infer them from host tests.
- [ ] Run sustained state changes and eight-hour idle/working/waiting use before claiming long-term stability.
# 2026-09-07 touch character update

Touch now gives the face an ordered response: eyes acquire first, the body
follows with retained velocity, and release leaves a brief look-back and soft
rebound. Regional pokes recoil and peek back; repeated/alternating taps, stroke
relaxation and two-finger cupping are driven by actual touch frames. Deliberate
eye closure is complete in idle/working/tool states; protected task expressions
retain their topology. See [touch interactions](touch-interactions.md) and the
offline geometry animation in `reports/touch-interactions/character/preview.gif`.
