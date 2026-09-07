# Progressive acceleration / inertia implementation

Implemented mounted, gravity-subtracted acceleration delivery; separate damped
face and gaze springs; continuous startle/bracing/stretch; accumulated fatigue;
asymmetric orbiting eyes; delayed recovery blink and checking glance. Touch,
non-playful states, navigation and long frame gaps reset inertia.

Validation:
- ESP-IDF v6.1 firmware cross-build passed.
- Existing motion/QMI/reaction suite: 3/3 passed; no unit tests added.
- Clean real-LVGL build: 6/14 passed. An isolated copy with the new inertia
  compositor call removed has the same 8 failing navigation/contact cases.
  See both logs; full UI regression acceptance is not claimed.
- Four synthetic 3Hz, five-second movement traces produced maximum horizontal
  offsets of 1.18 / 4.56 / 9.75 / 17.98 pixels at 0.15 / 0.55 / 1.1 / 2.0g.
  Peak fatigue was 0 / 0.1719 / 0.9573 / 1. All returned to center and zero
  fatigue after nine seconds of rest. CSV uses the actual estimator/compositor.
- git diff --check passed.

Initial implementation had no flash; see the physical follow-up below. Sensor-axis
sign, sensitivity, overshoot feel and actual display smoothness need board review.
Firmware SHA256: `36a5850fe275c72ce10091fe19fe163846479951ffe8a0e76cef02bcd84ae450`


## Physical follow-up

- Read and retained the complete 8MiB app partition before writing; checksum in
  `hardware/backup.sha256`.
- Original inertia firmware 2.2.0 flashed with esptool hash verification. Bridge
  resumed and device diagnostics confirmed live working/tool Face state.
- User physically confirmed reactions, but reported insufficient exaggeration
  and excessive acceleration thresholds.
- Tuned deadband, intensity, fatigue, spring gain/damping and alternating-peak
  detector. Three alternating peaks now require 0.38g with 0.14g release inside
  1100ms. Single-bump filtering and cooldown remain.
- Isolated v2 build of enhanced firmware 2.2.1 passed. Latest shared-source build
  was blocked by concurrent protocol-v3 parser indentation errors. Shared protocol
  work was not reverted; the exact v2 source archive accompanies the binary.
- 2.2.1 application write verified by esptool. First attempt encountered a
  transient exclusive serial lock, made no write; retry succeeded after that
  process exited. Bridge readback confirms connected=true, firmware=2.2.1,
  protocol=2, paused=false, state=working, screen=face.
- Existing motion/QMI/reaction checks after threshold adjustment: 3/3 passed.
- New physical sensitivity/overshoot approval is pending user feedback.
- USB protocol diagnostics prove connection/render activity, not measured panel
  FPS. Raw IMU telemetry was not present on this USB log; acceleration values in
  CSV are synthetic and must not be reported as physical measurements.

Enhanced firmware SHA256:
`09a113d0c39276535b703e97ce6c289132cd3d1577e56bcade411e6031464d03`

### Second physical review / current flashed version

User reported 2.2.1 still sluggish, reaction boundaries unclear, recovery too long.
Updated to responsive 2.2.2: lower input thresholds, faster intensity attack,
stronger damping, explicit bracing onset, 1300ms dizzy duration, faster fatigue
recovery and removal of delayed glance. Offline traces at all four amplitudes
return to zero fatigue and <0.001px offset by 1.5s after movement stops.
Existing motion/QMI/reaction checks: 3/3 passed; isolated firmware build passed.
2.2.2 write hash verified and live bridge reports firmware=2.2.2, connected=true,
paused=false. Initial readback was on Stats, so user was instructed to return to
Face for the physical review. Final physical feel remains pending.
Firmware SHA256: `d4f66c502bb01bb9525e7448f93b56fefd7d382dcacd5473e28ebfd58206950e`.

2.2.2 bounded observation: 25 API samples, all connected; 8 unique Face render windows. User visual acceptance still pending.

### Spiral eyes 2.2.3

User requested spiral pupils for high-intensity dizziness. Implemented two-turn
counter-rotating spirals with intensity morph, bounded geometry and existing
quick recovery. Firmware build and 3 existing motion checks passed. Actual
geometry SVG preview uses 82 of 96 primitives. Flash write hash verified;
bridge readback file: hardware/bridge-spiral.json. Physical spiral legibility
and worst-case high-intensity render performance remain to be reviewed.
SHA256: `96272ff505aa89f8e89917083bd17aa22826f5c18e20e37a64cde3ca85136cfc`.

### Gradient and horizontal-drift defect investigation

Pure 3Hz lateral translation with zero gyro reproduced false rotation up to
15.7548deg at 0.45g and full spiral at that same moderate amplitude. Root causes:
acceleration correction admitted translation into gravity; per-sample stillness
missed slow cumulative direction change; low-threshold shake events applied full
reaction amplitude; fatigue saturated independently of amplitude band.

Fixes: stationary-window gravity correction with anchored direction, gyro-only
prediction during movement, strength-scaled events, peak-envelope grade hysteresis
and amplitude-capped fatigue. Same five-input replay now has zero angle drift;
spiral gains are 0 / 0 / 0 / 0.0767 / 0.9285 at 0.12 / 0.25 / 0.45 / 0.8 / 1.3g.
Existing motion/QMI/reaction checks: 3/3 pass. @motion telemetry added for physical
verification; not yet captured from the corrected compatible build.

The isolated v2 fix (2.2.4) was flashed and verified. Meanwhile the running bridge
upgraded to v3, leaving protocol_mismatch after this flash. A current shared-source
v3 build including the fix passed, but its flash was rejected by automatic
approval review because it also contains concurrent protocol-v3 changes whose
scope needs explicit approval. No retry or indirect flash was attempted after
rejection. Device remains v2 fix; bridge is v3; compatible hardware acceptance is
BLOCKED pending user authorization for this concrete artifact:
`hardware/inertia-gradient-v3.bin`
SHA256: `943746c96ea7a160bb24bf328b06a773cb605222a2d9166702e612f8d1783e6f`.
