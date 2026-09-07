# Usage Dashboard integration acceptance · 2026-09-07

Approved visual reference: approved-design-03.png. Device render PNGs use explicitly selected sample review data and real LVGL production rendering, not hardware screenshots.

## Completed checks

- Existing Bridge integration:12 lifecycle/PTY/installer scenarios passed, and live isolated loopback API checks passed. Updated expected protocol handshake to v3; no new unit tests.
- Usage archive replay/correction acceptance: real179 day/model rows,72 subject/period/page projections bounded, replay does not double count, actual zero cost retained, partial fee provenance/coverage retained, obsolete models removed and empty authoritative snapshots cleared.
- New API integration through isolated live HTTP server:18 subject/period combinations, inclusive1/7/30day history, invalid query rejection, bearer refresh, strict empty refresh body and Origin denial passed.
- Real POSIX PTY:25 framed messages; v3 handshake/snapshots, independent browse selection, duplicate request idempotence, stale revision rejection, page bounds and8192byte envelope budget passed.
- Persistent Node watcher started, first179 rows ingested, three refresh requests coalesced to revision2, child exited on supervisor shutdown. Initial refresh starvation under continuous fs.watch activity was reproduced and corrected to a fixed maximum event merge window; recheck passed.
- Cursor official usage export is independently read with existing in-memory login. It no longer depends on external Tokscale CSV refresh. Final normalized rows preserve native disjoint cache-write/uncached/read/output semantics; fully numeric rows reconcile with total.
- Codex and Cursor direct official quota reads succeeded. Earlier Codex connection timeout was transient. Proxy retry was rejected by automatic approval and was not executed; no proxy workaround was used. Hermes local-provider quota and WorkBuddy quota remain explicitly unavailable. No real actual-cost values were present in accepted native sources; fee display remains unknown.
- Firmware ESP-IDF final build completed (3.0.0), all flash segments hash-verified and reset. Bridge user LaunchAgent upgraded to3.0.0 with pinned Node path. Real USB handshake returned firmware3.0.0/protocol3/connected/noerror. Further projection/touch and Mac acceptance will be appended below.

Temporary acceptance scripts run against isolated stores and readonly normalized snapshots; they are not installed or shipped as unit tests. Reports contain no prompts, tool arguments, chat bodies or credentials.

## Runtime integration and first user feedback

Another concurrent task flashed an isolated v2 motion fix after this task's initial v3 flash, producing a verified v2.2.4 hello / v3 Bridge mismatch. Current shared-source combined3.0.1 artifact preserved that user-requested motion fix and Dashboard; explicit authorization in this task allowed its flash. SHA256 e81c1555395d45c6cf2b3a0b31bd551916dde915159a15b73a02d0394901dae7, write hash verified. Real readback: firmware3.0.1, protocol3, connected/noerror, usage data_rev26, current/today. No parallel task was messaged or its code reverted.

The user rejected first physical UX: visually disconnected, stuttering, excessive labels. This is not accepted hardware UX; a refinement is in progress. Live local usage API30query timing: mean3.83ms,max9.34ms. Firmware was updating all widget styles/positions on each host clock revision; targeted redraw reduction is being implemented.

Live native Mac CUA inspection exposed black text on black background despite preferredColorScheme(.dark). Explicit foreground color and environment color scheme fixes were built/installed; recheck pending. Original pureImageRenderer was not counted as visual evidence.

## Design correction + same-tool concentric rings — 3.0.2

User rejected the previous design fidelity and chose the ECharts ring structure for multiple quota periods of one tool. Corrected overview spacing/icons, dedicated three-row information layout,64px detail value, direct metric tabs and compact native Mac entry card. See device-design03-final/verification.md for actual LVGL sample render and contact replay evidence. Static chrome pixel counts were independently checked by root;550–1800ms final frames are identical. No physical FPS inferred from replay.

ESP-IDF build passed. Immutable binary `firmware-3.0.2-usage.bin` SHA256 `c139b39d1ddccca18c4ab56ab9e2addcb50a8e1ea80ac4a435f89e12bf9323a4` was flashed with Bridge serial pause/resume. Esptool write hash verified. Real Bridge readback confirms firmware3.0.2/protocol3/connected=true/error=null and usage data_rev246. Initial Face CPU submission diagnostic83 updates/5079ms, avg3656µs/max4541µs is a Face sample only, not Stats interaction performance or physical FPS. New user physical layout/touch acceptance pending.

Mac signed build/install/restart passed for compact status card and same-tool concentric rings. Actual latest live UI verification still awaits menu opening. Prior CUA acceptance does not cover this redesign.
