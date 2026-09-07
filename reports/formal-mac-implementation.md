# Native Mac menu implementation — 2026-09-07

Task 4 implementation is complete in `host-app/**`; installation and actual menu interaction remain controller-owned acceptance.

## Delivered

- Native SwiftUI MenuBarExtra window, 380×560 scrollable Chinese panel, no full desktop window or custom glass.
- Device connection/firmware/protocol/errors, focus task short ID/state/reason/age, auto and pinned selection with revision conflict refresh and pending acknowledgement.
- Four monochrome icons extracted from the same committed firmware C image assets; bundle face icon follows pale opaque eyes and black pupils.
- Source installed/configured/observed separation; per-kind capability, active-session null handling, validation channels, gaps, event/scan age, synchronizing/backlog display.
- Real metric precision, coverage, source, age and staleness; missing numbers unknown, empty quotas unavailable. No fake preview or simulator data.
- User configuration opening, install/repair-hooks/doctor, service bootstrap/kickstart recovery, USB pause/resume and independent app/Bridge login toggles.
- URLSession local proxy bypass only, fixed loopback port17940, token read only at mutation, no token persistence/logging. Single polling owner with mutually exclusive fetch; panel roughly1.5s, background15s, stale snapshot explicitly offline, online actions disabled.
- Fixed CLI arguments, 30-second maintenance timeout, discarded raw output; diagnostics show API metadata only.
- Reproducible build/install/uninstall scripts and `docs/mac-app.md`. Scripts do not auto-launch; uninstall guards bundle identifier and preserves Bridge state.

## Verification performed

`host-app/scripts/build.sh` succeeded with Apple Swift6.4 / CLT macOS SDK, targeting arm64 macOS14. Build includes adhoc codesign and `codesign --verify --strict`.

The installed SDK introduces a State macro whose SwiftUIMacros plugin is absent in CommandLineTools. An explicit `ViewState<Value> = SwiftUI.State<Value>` property-wrapper typealias avoids that macro overload while preserving SwiftUI state ownership and Observation. No SDK files were modified.

No unit tests added. No app launched, installed, login item changed, source repaired, USB paused or service restarted by this worker. The controller owns these authorized real acceptance steps. A local GET probe yielded no response during controller service work; it is not recorded as runtime API acceptance.

## Controller handoff

Run `host-app/scripts/install.sh`, then `open "$HOME/Applications/Agent Robot Face.app"`. Inspect the menu popup through CUA, expand source/stats/diagnostics, confirm real API/device state, pin then restore auto, pause/resume USB, and verify retention/offline error treatment if service recovery is exercised. Login-item OS approval can only be reported after actual system confirmation. Source lifecycle completeness is determined by Bridge evidence, never by this app build.

Application is local adhoc-signed, not notarized or distribution-signed. macOS14 deployment compatibility is compiled, not exercised on a separate macOS14 host.

## Controller review corrections

Controller installed and opened the initial build. Following read-only review, the worker corrected diagnostics to consume actual `{kind, at_ms}` error objects, display a Chinese explanation with exception kind and exact occurrence timestamp, and deduplicate using stable kind+timestamp identity. Source gap/coverage codes now have readable Chinese text, including source-history synchronization, skipped oversized records and reader errors. Product names preserve their proper casing in focus, statistics and event rows.

Reopening investigation inspected the installed SwiftUI public `.swiftinterface`: MenuBarExtra exposes `isInserted`, which controls menu-item presence, but no popup-presentation binding. The menu-only app has no ordinary window after dismissal; CUA `noWindowsAvailable` is consistent with that lifecycle. The supported user affordance remains clicking the menu-bar eyes icon, documented in `docs/mac-app.md`. No private API traversal or extra workspace window was added. The worker did not reinstall or restart the app or Bridge while applying these corrections.
