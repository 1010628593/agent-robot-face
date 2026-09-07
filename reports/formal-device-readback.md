# Device applied-state readback batch — 2026-09-07

Narrow additions only: bot_link diagnostic serialization/header, main UI timer diagnostic call/comment and Picker partial-source count presentation. No geometry, motion, IMU, input-protocol, business-state or font changes.

The existing5s render diagnostic mailbox still drains through the UI owner and single USB TX owner. Optional `projection` now reports the real device authoritative selection, actual applied focus revision, actual displayed semantic state from `g_ui.agents[g_ui.selected].state`, and actual screen. A run ID is emitted only when exactly32 lowercase hex characters; absent/unsafe IDs become JSON null. Other strings come exclusively from fixed ASCII enum tables. Passing a null model omits projection for backward compatibility. Exact shape and readonly Bridge use are documented in `docs/protocol-v2.md`.

Picker shows the source health text plus `已观测活跃任务 N` for ready **or partial** sources with observed state capability. Partial health remains explicitly visible on the first line. Unavailable, disabled, missing and unobserved sources still show no invented active count. Existing Chinese subsets already contain every required character; no regeneration necessary. Stale main.c SIM-only heading corrected.

Validation: existing real LVGL executable and production sources compile with `-Wall -Wextra -Werror`; **14/14 PASS** at `reports/formal-device/readback-lvgl-results.log`. `git diff --check` passed. No new unit-test executable/target, subagent, commit or flash. Controller owns elevated firmware build if sandbox IDF discovery is blocked, then pause-wrapper flash and physical serial self-readback agreement validation. Software compilation alone does not prove delivery on the actual serial device.
