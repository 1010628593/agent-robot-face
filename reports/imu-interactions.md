# IMU interaction implementation / evidence

Base: PR #2 `2af70b70f506f212475a28a451f6bb45b76bacf2`.
Branch: `feat/imu-face-interactions`; stacked PR #3.
No main merge, hardware flash/erase, eFuse/PMIC write or user Agent configuration changes.

## Implemented

QMI8658 identity-first register layer, BSP-bus-reusing sensor service, gravity/gyro estimator, optional config, flat lock, bounded shake detection, dizzy/attention/settle pose composition, local Face rotation, contact angle freeze and inverse coordinates, semantic preemption and off-screen event consumption. Old misleading QMA7981 README corrected; historical disabled code and locked BSP/dependencies preserved.

## TDD and host verification

Work occurred in an isolated Linux source snapshot exported from the actual branch by CI (baseline run 34084456010). Actual pinned LVGL sources were included; no fabricated LVGL model was substituted for the real-render tests.

- RED: new motion/reaction contracts initially failed to compile/link because the implementation did not exist.
- RED: continuous shaking regression showed an early version rearmed as cooldown elapsed without first becoming quiet; fixed with explicit quiet rearming.
- RED: reinitializing an already-ready QMI handle with invalid bus callbacks retained its old ready flag; fixed by clearing the handle before validating callbacks.
- GREEN final local GCC: native 6/6, motion 3/3, face 2/2, real LVGL 10/10 CTest cases.
- GREEN final local Clang ASan/UBSan: same counts. Real LVGL uses only the inherited upstream `lv_draw_sw_mask_apply` function-type exception; application code and other checks remain enabled.
- Motion executable contains 11 groups covering calibration, in-plane/three-axis rotation, mounting/reversal, flat lock, shortest path, stale/gapped/invalid data, sample time wrap, repeated/same-direction/single peaks, cooldown and inverse coordinates.
- QMI tests cover wrong identity/no writes, configuration readback failure, partial readiness, signed conversion, torn/duplicate samples, sensor counter wrap, I/O failure/no output publication and invalid reinitialization.
- Real LVGL cases verify old/new rotated pixel regions, retained SIM, contact rotation lock, stale angle hold, priority interruption and no delayed replay from hidden screens. 130 additional synthetic preview frames were rendered by actual LVGL and inspected. They are not physical sensor recordings.

## First cross-build

PR run 34085891905 built the complete ESP-IDF v6.1 firmware at implementation commit `489c000` using the pinned official Docker image. All 2027 build steps completed, produced `bot_status.bin` (0xb9e70 bytes), and `cmp` confirmed the component lock remained unchanged. The job then failed in evidence collection because `find firmware/build-imu` was incorrect: `idf.py -B build-imu` resolves to the invocation's repository-root directory. The workflow now records `build-imu/...`; the compile success is distinct from the failed packaging step.

Inherited LVGL section-attribute warnings remain in the cross-build; no firmware dependency upgrade was used to silence them. This report does not label the final follow-up CI green before it runs. The final PR check/linked run is the source of truth for the newest commit.

## Still requires physical evidence

Actual 0x6B/0x05 identity on the user's board, correct axis normal/mount offset, sensor-ready behavior, board-specific thresholds, panel FPS, transform-buffer heap/PSRAM and task-stack margin, touch/bus concurrency, USB and eight-hour stability. Mount zero is not a calibrated board orientation.

Face-down sleep and text-page four-way rotation are deferred. Agent data remains SIM; this change does not complete real Codex/WorkBuddy/Cursor/Hermes integration or quota retrieval.

See `docs/imu-interactions.md` for commands and the safe calibration checklist. No script automatically flashes the board.
