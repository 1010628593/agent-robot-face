# UI polish implementation record — 2026-09-07

Base: PR #2 branch `fix/face-gesture-coalesced-input`, starting at `f2bedff27a63ffebe2ea1177fccd0558b17ed4eb`. Main remains the previously merged PR #1. No force push or hardware write.

## Reproduction before fixes

GitHub Actions run `34079512050`, test-first commit `e9582a542680459d22222677bba0e9d1a984c851`:

- Full native Linux build reproduced missing `libm` linkage in parser/model targets.
- Actual production UI compiled and linked against pinned LVGL 9.4.0.
- Real LVGL tests reproduced stale Usage/Quota headings, quota empty meters for N/A, exhaustion incorrectly labeled OVER, and confirmation from any Picker tap.
- Smoke test's initial hold duration was itself wrong: first DOWN was sampled at +10ms, so 650ms helper duration represented only 640ms held. Corrected the test to 660ms without changing the 650ms product threshold.

Run `34079991081`, commit `86d91e3e2baad1e32055a854bb1e3cd82027bee4`:

- Native core 6/6 and motion/fake-LVGL lifecycle 2/2 passed on the full checkout.
- Real LVGL 7/8 passed; `terminal_hidden` reproduced late replay when a DONE state arrived while Stats was visible.
- Inspected the exported real-LVGL PPM images. Long quota labels were cropped; changed quota name/value into separate rows and added measured text-fit assertions.
- Strengthened `terminal_resume` to use a non-blinking outgoing ERROR pose; a blink could otherwise conceal a weak pixel assertion.

## Changes

- Fixed four basic UI regressions and the native math-library linkage.
- Added per-Agent fixed motion records and off-screen semantic-edge synchronization.
- Kept the minimal Face appearance and all three-screen navigation routes.
- Removed apparent refresh text from demo statistics, without inventing a refresh implementation.
- Added full-checkout CI with pinned Actions/LVGL, software-rendered snapshots, real input/layout tests and a GCC/Clang sanitizer matrix.

## Measured results at production commit 53e01dc

Run `34080580899`:

- GCC: native core **6/6**, face motion/lifecycle **2/2**, real LVGL **8/8** passed.
- Clang ASan/UBSan: native core **6/6** and face motion/lifecycle **2/2** passed.
- Strict real-LVGL sanitizer execution stopped in upstream `lv_draw_sw_mask_apply` at an incompatible function-pointer cast. The problem is in pinned LVGL, not resolved by this application patch. It remains an explicit limitation.

The subsequent compatibility configuration excludes only the `function` check at that named upstream callsite, only for the LVGL target. It does not suppress application code, AddressSanitizer, leak checking or other UB checks. It is OFF by default and ON explicitly in the compatibility CI variant. See `tests/lvgl/README.md` and the current PR checks for the separately reported compatibility-run result.

## Evidence and boundaries

GitHub Actions stores `LastTest.log` for all three suites and PPM snapshots. The snapshots were downloaded and visually inspected. They are real LVGL software rendering, not physical screen captures.

No ESP-IDF cross-build, flashing, physical touch, power or long-duration hardware validation was performed in this round. No live four-Agent data, account usage or quota access was added. SIM provenance remains visible. Core and UI tests passing must not be equated with the full product being finished.

Reproduction commands and the local hardware gate are in `docs/ui-polish.md`. Existing source-adapter and Bridge milestones are not marked complete by this work.
