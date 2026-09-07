# QMI8658 face interactions

## Scope and integration

The device-level motion layer adds automatic Face leveling, shake/dizzy, movement attention and settling bounce. It does not modify Agent states, trigger tools, change selection, approve requests, read credentials or alter the wire schema. Face remains two eyes on black; the SIM badge remains because the existing Agent model is simulated.

This change is based on PR #2 (`2af70b7`), not a replacement for it. Picker/Stats remain in their existing fixed orientation; their four-direction rotation and face-down sleep are **not implemented in this increment**. No fourth screen, permanent rings, captions or motion commands are added.

## Hardware contract and sources

The standard/B Waveshare 1.75 schematic and hardware reference identify **QMI8658 at 7-bit I2C 0x6B**, sharing GPIO14 SCL / GPIO15 SDA. The sensor is board-mounted; ESP32-S3 itself does not contain it. The old QMA7981 README/component are not an applicable hardware implementation.

Sources inspected:
- Waveshare `HARDWARE_REFERENCE.md`: https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75/blob/main/HARDWARE_REFERENCE.md
- Vendor schematic: https://files.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.75/ESP32-S3-Touch-AMOLED-1.75.pdf
- BSP public `bsp_i2c_get_handle()` declaration: https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75/blob/main/firmware/brookesia/components/waveshare__esp32_s3_touch_amoled_1_75/include/bsp/esp32_s3_touch_amoled_1_75.h
- Register/scale reference: Lewis He's MIT-licensed SensorLib `src/SensorQMI8658.hpp`, inspected blob `2a75b6d6cc9ab7eba2a9134a4805e1978ccd77e3`: https://github.com/lewisxhe/SensorLib/blob/master/src/SensorQMI8658.hpp

The QMI register implementation here is small independent C code, not an Arduino/SensorLib dependency. WHO_AM_I register 0x00 must return 0x05 **before any writes**. Polling uses little-endian auto-increment (CTRL1=0x40), +/-4g, +/-1024dps, ODR code 6 (nominal accel 125Hz / gyro 112.1Hz; six-axis clock follows gyro). No FIFO, hardware gesture, calibration command, chip reset or interrupt configuration is used. Async burst data is rejected when a second read of the 24-bit sensor timestamp differs; duplicate samples are not integrated twice. This is not a claim of cycle-perfect hardware synchronization.

## Software ownership and fault behavior

`bot_imu` owns only a sensor device handle on the BSP-owned bus. It starts after display initialization. It never creates/deletes the shared bus, changes GPIO or PMIC rails, or calls LVGL. The single worker has a 4096-byte task stack, priority 3, about 10ms polling and 10ms per-transaction timeout. Sample timing uses elapsed time, not an assumed fixed rate.

A one-slot FreeRTOS queue carries the latest filtered view, not an unbounded sample backlog. The UI peeks without waiting and converts sensor timestamps into the existing LVGL clock. Data older than 250ms disables new motion reactions/rotation while the last visual angle and ordinary Face/touch continue. Five I2C failures or one second without a usable sample ends an attempt; at most three lifetime attempts with 1s/2s backoff occur. There is no reset loop or bus recovery that interrupts touch. After final failure, reboot is required to retry.

## Estimation and interaction contract

- Rodrigues gyro prediction maintains a gravity direction in sensor coordinates; accelerometer correction is gated by norm/residual/motion, preventing a shake from being interpreted directly as tilt.
- Gyro bias is estimated only after a quiet one-second window with at least 50 samples. Before calibration, raw gyro magnitude may be up to 10deg/s because it contains the unknown bias; after calibration the corrected limit is 2deg/s. The existing acceleration norm, per-sample change and full-window gravity stability checks still apply. Keep the device still at startup. Gravity is retained, not subtracted as accelerometer bias. Six-axis data cannot distinguish every extremely slow rotation from bias.
- Screen-plane gravity enters the flat lock below 0.22g and exits above 0.35g. Near-flat devices hold their last reliable angle. This cannot infer absolute yaw or where the user is when the screen lies horizontally.
- The fused gravity angle uses a 0.15-degree deadband without a second low-pass. UI catch-up takes the shortest wrapped path, bounded at 720deg/s using actual frame elapsed time (up to 250ms). No 358-degree spin across +/-180. Gravity fusion, flat lock and stale-sample rejection remain active.
- Three alternating linear-acceleration peaks above 0.60g inside 850ms trigger dizzy; a single bump or same-direction impulses do not. The detector also uses a 0.18g release threshold, 70ms peak refractory period and gyro-speed gate. These are **initial engineering thresholds, not measured on this board**.
- Dizzy lasts 2400ms from trigger, then returns to the current Agent pose. A 7400ms trigger-to-rearm cooldown plus 500ms quiet requirement prevents endless replay during continuous shaking. No event queue accumulates.
- Movement after >1500ms quiet produces 600ms attention. Settling for >350ms produces a 450ms damped eye bounce. These are motion heuristics, not guaranteed pickup/placement or person detection.
- WAITING, ERROR, DONE, UNKNOWN and CANCELLED suppress playful reactions. Their arrival consumes the active reaction so it cannot reappear when the Agent returns to WORKING. Events received while Picker/Stats are visible are consumed, not played late on return.

## Rendering and touch

`face_reaction.c` only composes a temporary pose. Dizzy uses moving pupils, uneven eyelids and a recovery blink, not X eyes (reserved for errors). The existing per-Agent expression clocks are not reset by a physical action.

Face rotates primitive coordinates around display center (233,233) before LVGL rasterization. Capsules use round-ended strokes, circular pupils use circles, lids rotate as triangles, and smile arcs rotate their center and angles. The 406x406 fixed surface encloses the original 322x244 bounds at every angle. After the initial frame, conservative old/new primitive bounds invalidate only changed regions, including stroke caps and antialias padding. Animation sampling is every 20ms (50Hz target, not guaranteed panel FPS). There is no object transform or intermediate rotated face bitmap. Pill fading is preblended against black before drawing an opaque stroke, preventing alpha from accumulating where LVGL end caps overlap the stroke body. Host rendering validates geometry; actual ESP32 frame rate and panel tearing still require hardware measurement. The root and SIM label are not rotated.

A DOWN latches the visible Face angle until UP/cancel/page departure. Raw touch positions are inverse-rotated using that same matrix, including the final release point. Face swipe directions are interpreted in the leveled Face coordinates. Text pages retain their existing coordinates and central-confirm hit area. Motion never produces a navigation/approval command.

## macOS build and first-board calibration

Use the existing EIM v6.1 environment; do not install another Python/IDF or modify managed components.

```bash
idf.py --version
idf.py -C firmware -B build-imu build
idf.py -C firmware -B build-imu menuconfig
```

Menu: **Component config -> Bot motion interactions**.

| Option | Default | Purpose |
|---|---|---|
| BOT_IMU_ENABLE | y | Disable to retain the previous non-motion UI |
| BOT_IMU_MOUNT_DEG | 0 | Sensor gravity angle when the intended face is upright |
| BOT_IMU_MOUNT_FINE_TENTHS | 0 | Fine mounting adjustment in tenths of a degree |
| BOT_IMU_REVERSE_ROTATION | n | Correct opposite PCB/sensor normal if required |
| BOT_IMU_DIAGNOSTICS | n | At most one six-axis/orientation log per second |

**Zero mount offset is a reference convention, not a verified physical axis mapping.** Before judging automatic leveling:

1. Build, retain the original Flash backup, and obtain explicit user approval before flashing. No command in this document automatically flashes.
2. Enable diagnostics for the initial check. Confirm the log reports WHO_AM_I=05 at 6B and sensible data. If identity fails, do not bypass the check or re-enable QMA7981.
3. Hold the screen in the intended upright position, rest it for at least two seconds. A one-time `alignment reference: raw=... mount=... residual=...` log is emitted after gyro calibration and 1.5 seconds of stability, even with diagnostics disabled. This reports a reference only; it never silently zeros the current pose. The accelerometer norm should be close to 1g and gyro close to zero; `bias=1` indicates the quiet calibration completed.
4. Record `raw` from the alignment reference while physically upright and set BOT_IMU_MOUNT_DEG to its rounded value. `raw` is independent of the existing mount value and reversal; do not substitute the residual angle for it. With diagnostics and the initial mount=0/reverse=n only, the stable `angle` also equals the mounting reference. Rebuild only after recording the initial orientation. Set the reversal option only if rotating the enclosure makes the face turn in the same world direction rather than compensating.
5. Check upright, left/right 45 and 90 degrees, upside-down, flat and back upright. Flat should lock rather than hunt. In-plane mount offset/reversal assume the IMU is on the board plane; unexpected Z/XY behavior requires checking board revision/axis mapping, not random sign edits.
6. Test a normal tap/swipe, gentle pickup/placement and gentle repeated shake without pulling the USB cable. Do not demand violent shaking. WAITING/ERROR must remain recognizable and navigation must not trigger from motion.
7. Check low FPS, blank transform buffers, resets, sensor/touch bus contention and eight-hour stability before considering the feature hardware-validated.

The Agent simulator still advances states every seven seconds. Test playful reactions while the selected Agent is IDLE/WORKING/TOOL; no dizzy response in WAITING/ERROR is intentional.

## Reproduce host verification / actual-code preview

```bash
for suite in native motion face lvgl; do
  cmake -S tests/$suite -B .build/$suite -DCMAKE_BUILD_TYPE=Debug
  cmake --build .build/$suite --parallel
  ctest --test-dir .build/$suite --output-on-failure
done
mkdir -p .build/motion-frames
BOT_SNAPSHOT_DIR="$PWD/.build/motion-frames" .build/lvgl/test_lvgl_ui motion_preview
```

The preview emits PPM frames from actual LVGL software rendering with synthetic movement. It is not a camera recording, sensor replay, macOS test or board-FPS measurement. Clang real-LVGL sanitizer testing retains the narrow, documented upstream callback exception from PR #2; strict unmodified upstream UBSan is not claimed green.

## Measured local board reference (2026-09-07)

With the user confirming the enclosure was in its intended upright orientation, 1Hz diagnostic readings measured a raw gravity heading of approximately 97.03 degrees. After the user repositioned and confirmed the final reference, ten stable samples measured 98.60–98.80 degrees (mean 98.69). This local sdkconfig uses `BOT_IMU_MOUNT_DEG=98` plus `BOT_IMU_MOUNT_FINE_TENTHS=7`; the generic Kconfig default stays 0 because mounting is device-specific. Raw gyro at rest was approximately (-3.38, 3.15, -0.38)deg/s, which explained why the previous 2deg/s pre-calibration gate never reached `bias=1`. Diagnostic logging is enabled for this hardware acceptance run.

The user has authorized building, flashing and hardware testing after each project change in this development session; do not repeat per-flash approval requests.

With diagnostics enabled, `bot_status` also logs refresh submission rate and average/maximum render duration per five-second window. These are measured LVGL render timings including its synchronous waits, not proof of panel scanout FPS or lack of tearing.

## Progressive inertial personality (2026-09-07)

The latest sensor view additionally carries gravity-subtracted acceleration in
mounted display coordinates (`screen_accel`, g). The Face compositor inverse
rotates this vector into its current local frame. Two spring pairs animate face
position and gaze separately, preserving velocity on direction changes. Integration
uses real frame time in <=5ms steps, velocity/displacement limits and a damped
boundary bounce. A >250ms frame gap resets the integrator. Stale sensor input
applies zero force so residual motion decays rather than replaying old acceleration.

Acceleration magnitude continuously blends curiosity, startle, bracing, squash /
stretch and unequal tired eyelids. Positive energy changes produce a short startle;
sustained acceleration accumulates fatigue independently of the existing three-peak
dizzy event. Acceleration zero crossings do not count as rest: strong fatigue decay
requires 350ms quiet. Fatigue adds orbiting gaze, convergence and asymmetric eyes.
After sustained shaking stops, a slow blink at 600–1100ms and a checking glance at
1400–2600ms accompany recovery. These are parameterized animation heuristics,
not claims about physically measured displacement or emotion recognition.

Touch contact, page departure, Agent switching and non-playful Agent states reset
the inertia presentation. No Agent state, action, approval or navigation is generated.
The existing fixed Face surface and important-state priority remain in use.

Offline trace: `tools/replay_inertia.c` feeds synthetic 3Hz acceleration through the
actual motion estimator and pose compositor. `reports/inertia-interactions/replay.csv`
contains four amplitudes, five seconds of movement and nine seconds of recovery.
Initial amplitudes and spring coefficients require physical direction/sensitivity
review on the mounted device; this trace does not validate hardware or panel FPS.

### Physical feedback tuning

The first physical review reported visible but insufficiently rich reactions and
high thresholds. Updated tuning lowers energy deadband from 0.07g to 0.035g,
full intensity from 1.4g to 0.8g, and fatigue onset from 0.20g to 0.12g.
The alternating-peak detector now uses 0.38g / 0.14g high/release thresholds
and a 1100ms window; three alternating peaks, refractory period and cooldown
remain. Face force gain is 4100, spring stiffness 82, damping 8 and maximum
spring offset 34px. Gaze has its own 260/135/11 gain/stiffness/damping.
These supersede the earlier initial engineering values in this document.

Because the shared workspace was concurrently moving to protocol v3, physical
firmware 2.2.1 is built from the retained v2 component snapshot plus the updated
motion files, without rolling back shared changes. Its exact source is retained
in `reports/inertia-interactions/hardware/enhanced-source.tar.gz`; managed ESP-IDF
components remain pinned by the included dependency lock. This artifact must not
be described as a build of the entire latest shared workspace.


### Second physical feedback: responsive 2.2.2 (current tuning)

User found 2.2.1 sluggish, with unclear reaction boundaries and lengthy recovery.
2.2.2 supersedes the preceding coefficients: energy deadband 0.025g, full intensity
0.45g, intensity response rate 24/s, fatigue onset 0.08g, fatigue decay 0.85/s
after 120ms quiet. Face gain/stiffness/damping are 7000/180/20; gaze 360/220/25.
The stronger damping makes release crisp rather than extending the overshoot.
Bracing has an explicit smooth onset band (intensity 0.45–0.80).
Alternating shake detection is 0.22g high, 0.09g release, three peaks in 1000ms;
dizzy lasts 1300ms and cooldown is 3000ms. Recovery blink is 180–460ms after rest;
the delayed checking glance is removed. Offline replay returns all four amplitudes
to zero fatigue and <0.001px horizontal offset after 1.5 seconds of rest.
Exact isolated source: `reports/inertia-interactions/hardware/responsive-source.tar.gz`.
Physical sensitivity and single-bump rejection need the next user review.


### High-intensity spiral eyes (2.2.3)

High instantaneous intensity or accumulated fatigue now morphs round pupils into
mirrored, counter-rotating two-turn spirals. The compositor supplies spiral gain
and phase; the shared geometry emits 40 round-ended black segments per eye.
Pupil circles shrink as the spiral expands. Each spiral fits an inscribed circle
with stroke margin; strong dizziness keeps eye height readable until recovery
blink. The fixed geometry budget is 96 primitives (full spiral preview uses 82,
with room for both pupils during morph and eyelid masks). Ordinary/important
Agent poses default to zero spiral. Existing quick recovery remains unchanged.
`hardware/spiral-preview.svg` is a snapshot of actual geometry, not a board photo.


### Gradient / gravity contamination fix (2.2.4)

Reproduction with no gyro rotation and 3Hz lateral acceleration showed up to
15.75deg fictitious orientation at 0.45g; the previous gravity correction gate
accepted translation. Gravity correction now requires 300ms stationary evidence,
including a window-anchored direction tolerance (0.02), rather than only small
per-sample changes. During movement the gyro predicts gravity; no acceleration
reset occurs on a sampling gap. Fixed mounting calibration is never rewritten.
All five synthetic translation amplitudes now maintain zero angle. Existing real
rotation, flat handling and mounting checks remain passing.

Interaction strength uses a 220ms peak envelope and four entry thresholds:
0.06 / 0.18 / 0.40 / 0.75g after the 0.025g deadband. Entry dwell is 60ms,
exit dwell 140ms with 75% hysteresis. Fatigue has an amplitude-dependent ceiling;
moderate movement cannot become full spiral merely by accumulating time.
Alternating-peak events carry measured strength instead of always using full
amplitude. Spirals require grade 4 plus sufficient amplitude and fatigue, with
50ms attack / 120ms release. @motion USB diagnostics at 4Hz expose actual
angle, residual acceleration, envelope, grade, fatigue, spiral, freshness and
calibration readiness for the physical verification run.
