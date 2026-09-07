# Touch-driven character 2.2.1

User approved the revised character response on 2026-09-07. Implementation lives
in face_motion.c and its header; replay_touch.c exposes gaze/tilt for inspection.
No new unit tests were written.

## Behavior

- Fast eye acquisition, slower damped body following with velocity preserved on
  retargeting, smooth bounded approach to edges, speed-sensitive attention.
- Local blink plus recoil and peek-back, increasing repeated-region dodge and
  alternating-side response.
- Stroke relaxation and nuzzling; dual midpoint cupping; complete independent
  eye closure; pinch/spread with one release rebound and 500 ms relaxation.
- Protected task geometry and existing business-transition clearing retained.
  Touch never grants permission, changes selection, or changes business state.

## Verification

- Strict C exporter compilation passed (-std=c99 -Wall -Wextra -Werror).
- Actual C recognizer/motion/geometry scenario replay covers idle, working,
  tool, waiting, done, error, cancelled and disconnected. Idle/working/tool
  dual eye hold reaches 8 px closure at 1,500 ms. See replay-summary.json.
- The same sinusoidal trajectory sampled every 10, 20 and 40 ms has body X
  minima differing by 0.030 px and maxima differing by 0.013 px, and identical
  final release pose. See sampling-summary.json. These are synthetic traces,
  not measured hardware trajectories.
- Native geometry preview was rendered and inspected; preview.gif animates
  four scenarios, preview.png is a still. These are not panel captures.
- ESP-IDF v6.1 isolated USB v2 production compilation passed. Image 865,952
  bytes; source base, overlay hashes and image hash are in firmware.json.
- Existing real LVGL suite on the isolated v2 image source: 6/14 passed.
  Eight failures: smoke (old hold entry), stats_tab (old vertical tab gesture),
  picker_target/cycling (immediate dismissal), terminal_hidden (old right exit),
  three motion_input cases (old page/rotation interaction assumptions). The
  passing checks include production, quota unknown/full, terminal resume,
  rotation and motion reaction. See lvgl-v2-checks.log. No fixtures rewritten.
- Shared worktree suite additionally fails its v2 welcome fixture because
  another task is converting the shared parser to USB v3. That source was not
  flashed. This deployment uses reports/os-motion/source-v2.tar.gz plus the
  motion/header overlay and hello version 2.2.1. Current inertia/face sources
  were byte-identical to the snapshot at build time.

## Deployment and physical acceptance

Backed up the currently installed 8 MiB app partition, then flashed only the
application at 0x10000 using Bridge pause/resume. Esptool write hash verified.
Bridge readback confirms connected=true, firmware=2.2.1, protocol_version=2.
Rollback and image checksums are in firmware.json; binary artifacts are local.

Physical sampling before this revision had confirmed 575 dual frames and 29
recognized taps. The user reported that it responded but lacked character.
That does not accept this revision. A new physical interaction check was
requested after flash; live-trace.jsonl records the available diagnostics.
Crossing contacts, either-finger lift, transient loss, rotation, subjective
motion quality, navigation conflict and state-change acceptance remain pending
unless subsequently recorded below.

## New firmware live evidence

The initial physical trace contains recognized clicks with expression=working,
raw_count=1, motion_count=1, nonzero gaze target and changed left/right eye heights
(44.71/75.44 px in a sampled response). The hardware and renderer path is active.
This is not a claim that subjective quality or the full hardware checklist passed.

New firmware telemetry subsequently recorded 31 recognized taps and 280 dual
hardware frames. One live tool-state sample has raw_count=2, motion_count=2,
left=8.0 and right=8.0. Samples span working/tool transitions with no FIFO
overflow (one startup invalid packet only). One sample started in reserved
side band edge=3 and correctly had pet=0/motion_count=0. Subjective approval
and the complete crossing/dropout/rotation checklist are still pending.

## 2.2.2 continuity correction

The user found 2.2.1 changed but still unnatural; it was not accepted as final
quality. Repeated poke replay identified a concrete discontinuity: at the
second release, eye height jumped 8 -> 86 px and face center changed from
234.923/231.381 to 231.444/233.369 in the same millisecond. 2.2.2 preserves
8 px closure and 232.411/233.152 position across that event. Pokes now add
velocity to the body spring and start eyelid interpolation from the sampled
closure. Periodic stroking sway and redundant displacement envelopes were
removed; contact widening and lid tilt were restrained.

Strict exporter compilation and isolated firmware build passed again. Existing
LVGL checks remain 6/14, with the same eight navigation/rotation/timing failures.
The matching image and manifest are firmware-2.2.2.bin / firmware-2.2.2.json.
Physical subjective acceptance of 2.2.2 is pending.
