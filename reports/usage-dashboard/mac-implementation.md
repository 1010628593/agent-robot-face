# Mac usage Dashboard implementation

Implemented native MenuBarExtra usage-first compact panel in `host-app/Sources/UsageDashboard.swift`; existing controls retained in secondary tab. App version 3.0.0. Build now includes all Swift source files.

Features: independent current/all/agent scope; today/7d/30d; dimensions/tools four-grid; three-row quota/metric/model pages; tap drill-down and horizontal/down drag navigation; solid stage-colored accurate gauges; account main/Spark separation; nullable actual-cost currency/provenance/coverage; real dated history with null gaps; secondary source semantics and freshness; queued refresh acknowledgement with revision polling; stale response cancellation and offline caches.

Validation: `host-app/scripts/build.sh` Swift 6 optimized arm64 macOS14 compilation, resource packaging, ad hoc signing and strict codesign verification passed. No unit tests added. Actual current Bridge API sample reviewed for nullable counts, actual_costs/provenance, quotas, source metadata and periods.

An offline ImageRenderer experiment could not render native Menu/Picker controls; its unsupported placeholder images were removed and are NOT visual acceptance. Installation, running MenuBarExtra click/drag behavior, actual on-screen layout and user visual review remain root integration responsibilities. No install or restart performed by this worker.


## Parent live acceptance and subsequent polish

The installed native MenuBarExtra was inspected through CUA. Initial screenshot exposed black text on black because the environment remained light; explicit foreground + colorScheme environment repaired it, verified in a new live screenshot. Redundant visible Picker labels were hidden. Live clicks traversed4tools→Cursor→quota list→plan detail;100% used displayed a single red ring,20/20USD quota and official reset date. Separate30day/all query rendered15.5B tokens and30 dated real history bars. Source controls remained in the secondary tab and device3.0.1/v3 online was visible. User-triggered refresh showed queued then new-data confirmation. These are actual native UI observations, not offline ImageRenderer claims.

A final label refinement removes scope/time selectors from metric details and time filters from quota pages (quota windows have their own official periods), hides duplicate detail scope labels, and suppresses repeated available-quota explanatory copy. Built and signed, deployment pending at this point.

## Reference fidelity correction (2026-09-07)

User rejected the dashboard as materially different from approved designs03/02. The Mac entry surface is now a compact status card: product title/settings icon, four native product selectors, 196pt 270-degree quota gauge, reset countdown, Today Token row, and one detail disclosure entry. Existing full usage views remain reachable behind that entry; device/control UI is behind the gear. Stage colors still encode USED percentage; design02's earlier gradient/remaining semantics are deliberately superseded by the user's later decision. Missing official quota remains an uncolored dash. Build and signed local install passed; live visual validation of this new entry surface is pending.

User-selected ECharts concentric adaptation added: same-tool quota windows, maximum three rings, stable native window ordering, individual stage hues, direct tap on each ring or named center row into its quota detail. One-window layout retains the large open gauge. Full quota lists retain additional windows and unavailable-source detail access. Product icons are desaturated for consistency. Latest signed build/install succeeded; menu application restarted. Live CUA validation awaits the panel being opened.
