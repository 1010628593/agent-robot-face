# Formal Product Implementation Plan

> For agentic workers: execute with superpowers:subagent-driven-development; user constraints override test/commit defaults.

Goal: implement approved real connected desktop face product, preserving prior hardware fixes.
Architecture: observed source adapters -> Python bridge ledger/projection -> protocol v2 USB -> UI-owner model -> face/picker/task-stat-quota. Native menu bar is a bridge client.
Tech stack: existing ESP-IDF6.1/LVGL9.4/C, Python3.11+, SQLite/pyserial/loopback HTTP, native macOS Swift.
Spec: docs/superpowers/specs/2026-09-07-formal-product.md

## Global constraints
- Preserve existing dirty IMU corrections; no unrelated reset, no unit test creation, no cloud source fakery, no repeated flash approval.
- Workers own assigned files only; no commits/staging until whole change reviewed because inherited dirty modifications are user work.
- All source status evidence explicitly distinguished from configured/unverified availability.

## Tasks
- [ ] 0. Read-only current source probes, exact runtime paths and event shapes, metadata only. Controller does hardware/installation.
- [ ] 1. face_motion/face_geometry/face_reaction + touch recognition/route: fixed pupil/color and playful gesture system. Existing real LVGL acceptance and rendered frames; controller builds/flashes batch.
- [ ] 2. bot_core v2 decode/model, bot_link transport, production bot_ui/picker/stats/Chinese font; production mode no simulator defaults. Publish exact wire examples under docs/protocol-v2.md for bridge task.
- [ ] 3. bridge service/ledger/adapters/hooks/serial/actions/CLI/install/doctor. Follow exact v2 contract, add integration scenario runner not unit tests. Controller real events and USB acceptance.
- [ ] 4. native host-app menu bar, packaging/install/launch integration. Real loopback state, source capabilities and errors; no credentials.
- [ ] 5. independent spec/code review, fixes, final build/flash/USB/source acceptance, reports. No unproven full-completion claims.

## Execution rulings
- Work in current hardware checkout on a new codex branch, retaining inherited uncommitted changes and local build configuration; separate checkout would strand calibrated state.
- Binding spec is user-approved plan; do not repeat design approval or flash approval.
- Unit-test additions and automatic commits in skill examples are overridden by explicit user constraints and preservation of inherited dirty files.
