# Touch interactions — firmware 2.2.0

This is the current touch contract. It supersedes the long-hold/horizontal-home navigation described in the historical v1 handoff and gesture fixtures. USB remains protocol v2; Bridge, selection acknowledgements, lifecycle facts and quotas keep their existing contracts.

## Navigation

| Page | Single-finger gesture | Result |
|---|---|---|
| Face | Start at y ≤ 56, drag down, release | Agent picker |
| Face | Start at y ≥ 410, drag up, release | Usage, initially 今日统计 |
| Face | Left/right edge | Reserved; no style switching yet |
| Picker | Horizontal swipe | Preview previous/next Agent or automatic mode |
| Picker | Tap central card with both endpoints inside | Submit selection to Bridge |
| Picker | Start at bottom y ≥ 410, drag up | Dismiss to Face |
| Usage | Tap 当前任务 / 今日统计 / 额度 | Select tab |
| Usage | Start at top y ≤ 56, drag down | Dismiss to Face |

Long holds never navigate. Face horizontal navigation, usage vertical tab switching/right-swipe return, and unrestricted panel dismissal are removed. Panels do not open each other. Persistent Face and near-fullscreen cards replace the previous 32 px page replacement. The authoritative visual and motion contract is [OS motion design](os-motion-design.md).

Navigation sees raw 466 × 466 screen coordinates before Face applies its inverse rotation. Valid circular starts satisfy `(x-233)^2+(y-233)^2 ≤ 233^2`. Corresponding edge movement >12 px and vertical/horizontal ratio ≥1.4 locks 1:1 dragging, subtracting that deadzone. Release completes at 38% of 466 px, or ≥56 px and ≥650 px/s; reverse ≥650 px/s cancels first. Velocity uses only the last 80 ms. Critical damping omega 24 settles from current position/velocity, capped at 450 ms. Second touch/cancel restores the last stable endpoint. Content stays disabled until docking. Taps remain ≤250 ms with ≤12 px displacement; moving beyond slop permanently cancels tap.

Ownership is latched at first contact. Face body starts stay interactions even when they move into an edge. Top/bottom starts stay navigation candidates. Side bands x ≤56 / x ≥410 outside the top/bottom bands remain reserved. A second finger permanently disqualifies navigation/clicks for that session, including two → one. Hardware ID replacement without all-up is also treated as multi-contact. Wake-consumed contacts produce no interaction or navigation.

## Nine required interactions — firmware 3.2.0

The 3.2.0 revision adds emotional phrasing to these movements: brief asymmetric
curiosity survives the first release; attention softens the focused upper lids;
stroke/cupping comfort grows into restrained smiling eyes; the third poke
closes both eyes, then opens the eye nearest the hand before the other. A new
contact immediately owns gaze while the older blink can finish continuously.
These are short-lived touch responses, not persistent mood learning.

This table is the current acceptance contract. Earlier 2.2.x revisions only
partially implemented it; their build/telemetry results did not accept the nine
behaviors. The new image uses the current USB v3 source snapshot and preserves
its Agent/usage contracts.

| Input | Implemented response / code evidence |
|---|---|
| First face contact | 35 ms gaze acquisition, retained-velocity body spring, real head roll by shifting both eye centers around the face center. |
| Eye tap | Independent full eyelid closure, bounded recoil impulse, then gaze plus spring-driven return toward the touch. |
| Repeated region | First non-eye poke: attention; second: bilateral blink; third: stronger avoidance. Direct eye taps retain their explicit local blink even on the first tap. The sequence resets after 1.2 s. |
| Alternating sides | Side alternation redirects body velocity toward the latest side and produces real head inclination; the gaze leads. |
| Continuous slide | Gaze leads the body; damped spring retains velocity and tanh targets soften movement near the screen edge. |
| Back-and-forth strokes | Existing path/reversal recognizer ramps relaxation over 1.5 s. Signed filtered touch velocity drives light sway and head roll. The body pauses 150 ms after a long hold, comfort pauses 160 ms, then recovers. |
| Two-finger cup | Body follows the midpoint; relaxation builds. Two-to-one retains cupping comfort and follows the surviving finger until final release. |
| Pinch / spread | Separation and width change with distance; height compensates in the opposite direction to suggest volume. Pinch shape rebounds once on release. |
| Two eyes held | Each contacted eye closes independently. A surviving eye contact stays closed while the other opens into the relaxed pose; all-up returns both to baseline. |

Stable hit regions and frozen orientation are retained. Working/tool state
reduces playful movement; waiting/done/error/cancelled/disconnected keep their
business topology. No touch changes task state or triggers approval/navigation.

The exact C recognizer, pose and geometry are replayed separately for every row
in `reports/touch-table/`. `nine-interactions.gif` is an offline working-state
preview with touch markers, not a physical screen recording. Hardware results
are tracked separately in `reports/touch-table/acceptance.md`.

## Continuous expressions

### Touch-driven character revision (2.2.2)

The face responds as a small inhabitant of the circular display. All motion is
derived from contact coordinates, timing, trajectory speed, repeated-region or
alternating-side taps, stroke reversals, and two-point midpoint/separation. There
is no inferred pressure, random emotion loop, or persistent mood storage.

- Eyes acquire a contact first (35 ms exponential following). The body follows
  a damped spring with preserved velocity when the target moves. A `tanh` target
  softens the circular boundary; idle translation is bounded around 34/24 px.
- Initial contact briefly opens the eyes with restrained asymmetric attention. Movement
  speed and direction add small eye widening and tilt; the gaze leads the body.
- A poke adds a bounded impulse to the current body velocity and a 260–1,100 ms
  look back toward the hand. Eyelid closure starts from its current amount, so
  a second poke cannot abruptly reopen the eye. Repeated pokes increase recoil;
  alternating sides redirect the spring. New input immediately retargets the body.
- Stroking gradually deepens relaxation, adds a small nuzzle toward the hand,
  without a clock-driven sway. A two-finger hold relaxes toward its midpoint over 1.5 seconds.
- On release the eyes retain attention for 100 ms, then ease out over 500 ms.
  Body velocity decays through one soft rebound; pinch shape rebounds once over
  500 ms. Eyelids open without oscillation. A surviving eye contact stays closed.
- Full eye closure is retained while working or using tools. Other playful
  motion is reduced by half. Protected task states keep their existing geometry
  and only receive roughly 3 px of body displacement. State/event changes clear
  interaction state through the existing business-state synchronization.

Firmware 2.2.2 is built from the archived USB v2 source plus the face motion
implementation/header. The shared worktree's separate USB v3 upgrade is not part
of this device image. Evidence: `reports/touch-interactions/character/`.

Stable neutral-layout regions are used, never animated eye bounds: left eye x123–217 / right eye x249–343 at y169–297, center gap between the eyes, forehead above y169, and remaining left/right cheek. Eyes take precedence over other regions.

- Single tap: local eye blink, central convergence, upward forehead glance, or cheek lean/squint.
- Live contact: exponential gaze following (35 ms time constant) and bounded face displacement; no playback cooldown blocks new input. Active expression channels follow with a 30 ms time constant so incoming frames do not continually restart a zero-slope easing curve. Release still eases out over 500 ms.
- Strokes: ≥40 px cumulative path and two ≥8 px reversals start a 1.5 s relaxation ramp. Continued contact gives a small nuzzle following the hand. Release eases out over 500 ms.
- Repeated taps in the same region, with ≤450 ms between release events, progress to a small dodge from the third tap. The streak is capped at six and clears after 1.2 s inactivity. Alternating regions immediately direct attention to the latest side.
- Two fingers over the eyes close each eye independently. A remaining finger keeps only its own eye closed.
- Other two-finger touches use their midpoint and change in separation: eye separation changes by at most ±22 px, width by ±9 px; sustained contact gradually relaxes. Returning to one finger releases pinch deformation.
- Working/tool displacement, deformation and relaxation use half amplitude; deliberate eye presses and eye-tap blinks retain full closure. Waiting/done/error/cancelled/disconnected retain their state topology and only permit 15% displacement. State/event transitions clear the previous interaction. No touch changes a task, grants approval, or invents successful completion.

## Input ownership and interfaces

`bot_touch_frame_t` carries a timestamp, up to two `(id,x,y)` points, explicit cancellation, and optional final release coordinates. `bot_ui_touch_frame` first passes raw frames to `bot_navigation_feed`; unowned frames reach `bot_gesture_feed_frame` for expression/content arbitration; `bot_face_motion_interact` and `bot_face_motion_poke` produce poses in the existing C renderer and clock.

`touch_input.c` installs `custom_touch_read` on the existing ESP LVGL adapter. The callback replaces the adapter's original hardware read; it does not create another poller, I²C device, or touch task. It reads D000 as a 15-byte packet, acknowledges D000 AB, decodes records at offsets 0/7, validates active event 6 and track IDs, compacts active records, sorts by ID, and applies the BSP coordinate flags. Optional event-0 final coordinates are retained only for an unambiguous single release record.

The LVGL native pointer receives released state; all card/tag clicks pass through the same UI arbitration as swipes, so LVGL cannot produce a second release click. A 16-frame owner-context queue preserves transitions. Overflow, cover/palm, malformed packets and I/O errors cancel the session until reliable all-up. A 35 ms all-up settling window absorbs brief dropouts, preserving the original physical release timestamp for tap timing.

The existing BSP, managed touch driver initialization, LVGL version, display setup, partition table, and USB wire schema are unchanged. `bot_touch` logs read/dual/error/overflow counts and the latest raw coordinates at most once per five seconds while input arrives. They are serial diagnostics, not additional Bridge protocol fields. Driver errors do not reset the shared LCD reset pin.

Protocol references: [Waveshare two-point example](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75/blob/main/examples/arduino/10_Touch_CST9217/10_Touch_CST9217.ino), [SensorLib CST92xx implementation](https://github.com/lewisxhe/SensorLib/blob/master/src/touch/TouchDrvCST92xx.cpp). These establish a software reference, not proof of this board's physical two-point accuracy.

## Review and acceptance

`tools/replay_touch.c` is an offline diagnostic exporter using the actual recognizer, motion engine and geometry. Build instructions and input format are in its header. `reports/touch-interactions/*.touch` are replay recordings; matching JSONL and `touch-preview.png` expose the resulting geometry. They do not emulate CST9217 or prove hardware behavior.

Hardware acceptance must cover simultaneous points, crossings, either finger lifting, transient loss, rotated contacts, region taps, continuous strokes, pinch/spread, prolonged holds, both edge entrances and reverse dismissal, short/diagonal cancellation, second-finger cancellation, panel tabs, selection acknowledgements/rejection, wake consumption and state changes. Record physical evidence separately from compilation and offline rendering in `reports/touch-interactions/acceptance.md`.

Existing tests that assert long-hold navigation, home horizontal navigation, vertical tab changes or instantaneous panel dismissal describe the retired contract. Do not report the entire suite green or silently rewrite those assertions. No new unit tests were added for this change.

OS card acceptance and actual LVGL coordinate replay: [reports/os-motion/acceptance.md](../reports/os-motion/acceptance.md). Historical 2.1.0 evidence remains separate.
