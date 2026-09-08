# Environmental sound implementation ledger — 2026-09-08

Approved target: merge product branch into main, then implement v1.1 with experimental audio OFF until board G0 and device acceptance pass. User forbids new frontend/backend unit tests. Existing regression and standalone replay/build evidence are allowed.

## Baseline

- Product tip f5355b0 consolidated by 231f590; tracked build-imu removed from index, local outputs retained. main fast-forwarded to 231f590. No history rewrite or remote push.
- Old G0 report's completed status is not accepted; current audio artifacts are not physical evidence.

## Decisions

- Explicit user request to work on main takes precedence over skill worktree default.
- Application-owned exclusive RX I2S and codec adapter borrows BSP I2C/pins instead of copying entire pinned components. This exposes actual driver results without editing managed_components. Physical single-mic route and RX clock behavior remain G0 obligations. If incorrect, adapter needs board-specific correction before opening runtime gate.
- BOT_AUDIO_ENABLE=n and BOT_AUDIO_G0_VERIFIED=n by default. No autostart setting: runtime user opt-in remains required even after physical verification. Unverified requests report a visible unsupported fault.
- Main UI bridge carries no PCM and adds no remote audio command.
- Product baseline has BOT_FACE_FRAME_MS=20 (old main in design had33); preserve existing20ms rather than regress cadence. Input poll remains10ms, audio snapshots50Hz; no extra LVGL timer.
- Scoped source-hash-guarded I2S derived translation unit wipes all DMA blocks beforefree; no upstream IDF/managed source modification. Defaults retain disabled core dumps.
- IMU unavailability switches effective capture config to low-sensitivity Natural and restarts calibration/epoch, retaining requested UI preference for recovery. Degraded motion-filter flag remains informational.

## Work

- Merge/cleanup complete.
- Core, owned hardware adapter/probe, Stats controls/director, persistent owner and main integration implemented.
- Audio OFF, audio ON with G0 gate closed, and separate USB-console G0 builds pass. Final artifact metadata is recorded in reports/audio/build-verification.md.
- Standalone core and actual-owner lifecycle diagnostics pass strict warnings and ASan/UBSan. 100 synthetic-port cycles, duplicate requests, stop/open race, retry exhaustion and cleanup failure verified.
- Actual LVGL rendering covers new settings and audio/priority/control scenarios; see reports/audio/ui/README.md.
- Existing regression debt verified against independent 231f590 archive: native 2/6 pass (same4fail), motion3/3pass, LVGL5/14pass(same9fail). Legacy fake-LVGL lifecycle build lacks bot_link.h on both baseline/current; face_motion CANCELLED mapping assertion fails on both. Added only missing flag/text shims for new label calls, no new unit tests or weakened assertions.
- Physical microphone activation/flash has NOT run. User cooperation for A/B sound input remains pending; no acoustic, real lifecycle, latency, CPU, heap or8h pass is claimed.

## Final source review

Independent full-path review found no remaining P0/P1 after revision-qualified state publication, atomic stop acknowledgement, duplicate-request handling, and final LOUD/QUIET corrections. Scoped re-review confirmed those fixes. Physical acceptance and baseline regression debt remain explicitly outstanding.
