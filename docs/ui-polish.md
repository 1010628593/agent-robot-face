# UI polish: persistent faces and dependable navigation

This is an incremental update to `docs/face-animation.md`. The approved eye-only Face design, four simulated Agent entries, screen size, semantic protocol and firmware dependency lock remain unchanged. It is not a live-Agent integration milestone.

## Persistent animation state

`face.c` now keeps a fixed array of four `face_track_t` records. Each record owns the existing motion engine, last protocol state, transition revision and local event generation. `face_sync(now)` observes all four entries on every UI poll, including when Picker or Stats is displayed. It changes a motion target only on a genuine state/revision edge.

Selecting an Agent switches the displayed track rather than resetting its semantic event clock. A completed event observed while Stats is open has already aged when Face becomes visible; it does not play a late celebration on return. A new transition revision still permits a new event. Heartbeats must continue to keep `transition_id` unchanged.

Only the selected Agent is rasterized. The single current/next primitive buffer and 33ms sampling cap remain. Hidden tracks add only fixed in-memory state, not timers, rendering work, network requests or background tasks. Destroying the face surface releases its owner's touch effect and clears the surface reference.

## Touch and selection

The gesture recognizer remains the authority for the 650ms hold, displacement and swipe thresholds. A Picker confirmation additionally requires both DOWN and UP coordinates inside the central card: `161 <= x < 305`, `154 <= y < 298`. This matches the current 144x144 card at (161,154). Touching the title, neighbor cards or outer edge does not confirm. A small drift from outside to inside also does not confirm.

Horizontal browsing, returning/cancelling, Face-to-Stats and vertical Usage/Quota switching retain their original behavior. Selection is an observation change, not permission to execute a tool. The existing immediate selection acknowledgment remains a SIM transaction.

## Statistics correctness and legibility

- The heading changes with the active tab and selected Agent.
- Unknown, unavailable or negative invalid quota does not get a numeric meter.
- Unlimited quota is a text state, not a zero-percent meter.
- Exactly 100% used means `0% LEFT`; only values above 100% are `OVER`.
- Quota names and values have separate full-width rows, preventing `SHORT WINDOW` and `UNLIMITED` from being cropped to ellipses.
- Content containers have explicit zero-style geometry, rather than inheriting theme padding.
- Deleted Stats views clear their page-owned references.
- Demo statistics say `SIMULATED DATA`; the old apparent refresh affordance is not shown when no real refresh action exists.

This does not make missing usage samples accurate, connect a provider balance API, or implement real refresh. Source accuracy, coverage and staleness still belong to the Bridge/protocol integration work.

## Testing

All sources in `tests/native` and `tests/face` are now built on full GitHub Actions checkouts. The native parser/model test targets explicitly link `libm` so Linux links the `floor()` calls used by production parsing.

`tests/lvgl` is a separate real LVGL 9.4 software-rendering target. It exercises all three production views, actual LVGL widgets/layout/indev processing and a memory framebuffer. It covers eight scenarios including 100 screen-change cycles, returning to terminal faces, header synchronization, central selection, quota edge cases and measured text dimensions.

```bash
cmake -S tests/native -B .build/native -DCMAKE_BUILD_TYPE=Debug
cmake --build .build/native --parallel
ctest --test-dir .build/native --output-on-failure

cmake -S tests/face -B .build/face -DCMAKE_BUILD_TYPE=Debug
cmake --build .build/face --parallel
ctest --test-dir .build/face --output-on-failure

cmake -S tests/lvgl -B .build/lvgl -DCMAKE_BUILD_TYPE=Debug
cmake --build .build/lvgl --parallel
mkdir -p .build/snapshots
BOT_SNAPSHOT_DIR="$PWD/.build/snapshots" ctest --test-dir .build/lvgl --output-on-failure
```

For sanitizer commands and the explicitly documented upstream LVGL callback exception, read `tests/lvgl/README.md`. No exception applies to the application sources or the standalone core/face tests. Do not call the compatibility run a clean strict-UBSan run.

## Local hardware gate

Use the existing EIM v6.1 environment and build before any flash:

```bash
idf.py --version
idf.py -C firmware -B build-face-motion build
```

No remote test flashes the device. Preserve the original Flash backup and get separate approval for flashing. Hardware checks still required: physical touch accuracy, orientation, actual panel refresh, stack/heap margins, USB behavior and eight-hour stability. Software-rendered screenshots are not hardware photographs or FPS evidence.
