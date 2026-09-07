# IMU Face Interactions Implementation Plan

> For agentic workers: execute this plan task by task; preserve the prior UI work in PR #2. User approved automatic leveling and shake/dizzy interactions on 2026-09-07.

**Goal:** Add QMI8658 motion input, continuous Face leveling and bounded dizzy/attention/settle reactions without changing Agent semantics.

**Architecture:** One independent sensor task reuses the BSP I2C handle and publishes latest-only timestamped samples. Pure C filtering and detection run on one owner; UI consumes snapshots, composes a device reaction over the current Agent pose, and rotates a local face layer. Sensor faults must not stop display/touch.

**Tech Stack:** Existing ESP-IDF v6.1 / BSP 3.0.1 / LVGL 9.4.0; C99 host tests; real LVGL software rendering.

**Spec:** Approved conversation design: automatic leveling first; repeated shake -> dizzy (not X/error); movement attention and settle bounce optional. No voice, robot hardware, Agent commands or new permanent Face labels.

## Global Constraints

- Branch from PR #2 head 2af70b70f506f212475a28a451f6bb45b76bacf2; do not merge/force-push main.
- Standard Waveshare 1.75-B uses QMI8658 at schematic address 0x6B. Do not enable the obsolete QMA7981 component or invent GPIO wiring.
- WHO_AM_I must be checked before sensor configuration. Only sensor registers may be written. Reuse BSP-owned bus; no PMIC/reset/GPIO/bus reconfiguration.
- No flashing, erasing, eFuse operations, accounts or hooks. Host tests are not physical-board evidence.
- WAITING/ERROR/UNKNOWN/CANCELLED suppress playful reactions; new terminal states preempt reactions. Return to the latest Agent state, not a saved old state.
- Flat pose holds last reliable heading; six-axis gravity cannot determine an absolute yaw/user direction on a horizontal screen.
- Locked dependencies and wire states remain unchanged. Firmware IMU code and host simulator code are separate.

## Task 1: Baseline + sensor contract

Files: new `firmware/components/bot_motion/`; new `firmware/components/bot_imu/`; new `tests/motion/`; update obsolete `firmware/README_gravity_sensor.md`.

- [ ] Read BSP's exported I2C API and vendor QMI8658 register reference; record identity, auto-increment, range/ODR and data readiness assumptions.
- [ ] Write fake-register-bus tests for wrong identity/no writes, init failure, signed sample conversion, data-not-ready and transaction errors; run RED before production implementation.
- [ ] Implement a bus-independent bounded QMI register layer, then ESP-IDF transport; no indefinite waits or UI calls in the sensor task.
- [ ] Prove tests GREEN and retain logs. Leave board identity/axis verification pending until hardware evidence exists.

## Task 2: Motion estimator + reaction detector

Files: `bot_motion.h`, `bot_motion.c`, `test_motion.c`.

- [ ] Tests: rest identity, +/-90 and 180 degree leveling, shortest-path wrap, flat hysteresis, stale/invalid/gapped samples, timestamp wrap, repeated peaks versus single bump, cooldown and gravity-only rotation not counted as shaking.
- [ ] Implement finite-input validation, static calibration, gyro gravity prediction and accelerometer correction with motion gating, angle confidence and smoothing.
- [ ] Implement bounded device-level reaction clocks; no protocol changes. Make thresholds named and synthetic-testable, not claims of measured thresholds.
- [ ] Run GCC and Clang sanitizer tests before integration.

## Task 3: Face integration + touch geometry

Files: existing `face.c`, `ui.c`, `bot_face.h`; new local `face_reaction.c`; `tests/face`, `tests/lvgl`.

- [ ] Add failing tests for visible rotation, no ring/text reintroduction, pupil-follow in inverse-rotated coordinates, contact rotation lock, semantic preemption, no delayed replay after hidden screens and sensor loss.
- [ ] Use local-layer rotation centered at (233,233), not whole framebuffer streaming; invalidate old/new bounds and preserve SIM badge visibility.
- [ ] Keep Picker/Stats gestures stable during contacts. Do not create side-effectful motion controls.
- [ ] Exercise actual LVGL rendering and inspect software snapshots; retain the existing narrowly-scoped upstream sanitizer exception, never broaden it to app code.

## Task 4: Firmware wiring + evidence

Files: `firmware/main/main.c`, CMake/Kconfig, docs and CI.

- [ ] Start sensor service after BSP display/bus startup; UI polls nonblocking latest sample using one monotonic clock.
- [ ] Add configuration for sensor enable, axis mapping/zero reference and diagnostic logging; no secrets or unbounded raw logs.
- [ ] Cross-build with existing ESP-IDF v6.1 and locked components, without writing hardware. Run native/face/LVGL/motion suites on the final tree.
- [ ] Publish actual commands, green/red evidence, soft-rendered snapshots and a safe local axis/gesture calibration checklist. Label physical verification BLOCKED_HARDWARE.
