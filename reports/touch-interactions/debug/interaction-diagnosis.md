# Touch feedback diagnosis — 2026-09-07

The user first reported navigation working but face interaction absent, then confirmed the diagnostic firmware responds but the overall interaction design feels insufficiently alive. This is not acceptance of the current behavior.

## Physical evidence

Bridge device telemetry observed 1,635 hardware reads, 575 dual-point frames and 29 recognized taps; 1 startup invalid packet and 0 queue overflows. Current face state was working. This proves simultaneous reporting and click recognition occurred; it does not validate crossings, rotation, transient loss, or subjective animation quality. The earlier capture window expired before those gestures; cumulative counters were read afterward.

## Corrected source defects

- Working/tool amplitude reduction also halved deliberate eyelid closure. Existing dual-eye replay in working state at 1,500 ms previously produced 47.018 px eye height, and now produces 8 px closure. At 2,300 ms after release both versions restore 86 px height.
- Continuous position/channel updates restarted a smoothstep at zero slope for each input frame, making response dependent on input frequency and slowing continuous movement. Active following now uses exponential interpolation (gaze 35 ms, channels 30 ms), retaining the 500 ms release ease. Existing replay executable compiles with strict warnings.
- Read-only SQLite adapter connections used a transaction context without closing the handle. Added explicit closing. Existing Bridge integration check passed (12 checks plus 8 API checks). This is a separate service reliability issue, not proven to cause weak face feedback.

No new unit tests were added. No claim of full hardware acceptance is made. Richer character behavior requested by the user is a subsequent design revision and remains pending.

Firmware compilation passed (ESP-IDF v6.1, exit 0). The build output is `/Users/hanxin/Workspace/llm/build-touch-debug/bot_status.bin`; the CLI resolved the relative build directory outside the repository. These latest source corrections have not been flashed. The device still runs the diagnostic image used for the physical counters above.
