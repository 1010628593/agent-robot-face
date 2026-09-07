# USBv3 device implementation evidence

2026-09-07. Production changes are confined to firmware and device documentation; no flash or physical acceptance was performed by this implementation subtask.

- All link envelopes, hello min/max and strict decoder envelope version are 3; firmware hello reports 3.0.0.
- Usage/request/ACK schema, independent revision and request32hex matching, authoritative conflict recovery, UI-owner TX, timeout/pending presentation and stale-source handling are implemented.
- `host_now_ms` is required, bounded and separate from source `as_of_ms`; quota countdown never uses the token collection date.
- Hierarchical dashboard uses fixed LVGL widgets; unchanged snapshots produce no label/arc mutations. No `lv_obj_clean()` runs in production Stats refresh.
- Overview matches approved structure with existing monochrome product artwork, thin grid separators, near-black Stats-only surface; list/details show source values, actual fees/coverage/provenance, numeric tokens and cache definitions.
- Native production LVGL replay target compiled using `-Wall -Wextra -Werror`. Selected `device-*.png` images are direct software renders, with explicit sample data. They are not hardware photographs and do not prove collector coverage.
- `device-render/contact-review.csv`: two→one contact leaves detail at depth2; first single downward contact changes2→1 onUP; second1→0 onUP; third rootdown enters panel spring and reaches Face (phase0,progress0). The existing continuous face interaction and navigation implementation were not rewritten.
- Production C parser accepted the Bridge real snapshot: `device-wire-decode.txt`. Strict shape/range diagnostics and bounded maximum numeric/label fixture results are in `device-wire-boundaries.txt`. No new unit test suite was added.
- Initial ESP-IDF attempt in the sandbox failed in the component manager's psutil/sysctl process lookup, before source compilation. The main task owns the authorized elevated ESP build and subsequent flash/handshake/HIL checks. An elevated GCC misleading-indentation issue reported by the main task was fixed by splitting the quota/model/history count assignments into braced statements.

The compact device retains dated nullable history in its model but does not draw a synthetic time series. Full daily history is provided by the paired Mac/API. Existing legacy CTest scenarios encode retired interaction semantics and were not reported as passing this implementation.
