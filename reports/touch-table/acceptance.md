# Latest revision: 3.2.0 — emotional layering

The user physically tried 3.1.0 and reported: responsive, but poor design,
lacking emotion and layers. That version was not accepted as satisfactory.
3.2.0 changes the expressive sequence itself: asymmetric curiosity continues
through first release, focused lids soften under attention, comfort develops
into smiling eyes, and teasing recovery opens the nearer eye before the other.
New contacts take gaze priority over an earlier peek. Independent dual-eye
closure suppresses the global smile so the surviving finger keeps its own eye
closed and the released side can open.

The exact C-geometry storyboard was generated and visually inspected. Nine
scenarios across eight states were re-exported. Strict C compilation and
ESP-IDF firmware compilation passed. Existing terminal-resume, motion-rotation
and motion-reaction checks passed 3/3 after these changes. The broader suite's
nine documented failures below are still unresolved; this is not an all-green
regression claim. No new unit tests were added.

Current image / source / logs: firmware-3.2.0.bin, source-3.2.0.tar.gz,
firmware-3.2.0.json, build-3.2.0.log, flash-3.2.0.log. The current device baseline
was verified as 3.1.0 / USB v3 immediately before updating. Physical subjective
acceptance of 3.2.0 has not been received. Waiting/done/error/cancelled/offline
retain the original business-state interaction restrictions.

The following sections record 3.1.0 implementation and measurements, not
subjective acceptance of 3.2.0.

# Nine-touch behavior completion — 3.1.0

The user explicitly reported the nine-row interaction table was not implemented.
Earlier telemetry and successful builds only proved parts of the input/render
path; they did not deliver or accept the full behavior table. This revision
implements the missing distinctions and records each row separately.

At diagnosis the device was running 3.0.1 / USB v3, not the previously observed
2.2.2 / USB v2. The new image uses a frozen current v3 firmware source snapshot;
it does not roll back the usage UI or Agent protocol. Source and binary hashes
are recorded in firmware.json; source-3.1.0.tar.gz is the reproducible snapshot
(excludes managed ESP-IDF dependencies). Application partition backup retained.

## Row-by-row source / replay evidence

All scenario inputs and geometry output are in this directory. The exporter
calls the real gesture, face-motion and geometry code. The following numbers
are working-state observations from the replay, not physical panel measurements.

| Required interaction | Source implementation and replay evidence | Physical acceptance |
|---|---|---|
| First contact | 35 ms gaze, delayed spring body, separate head roll. `01-contact`. | Pending |
| Eye tap | Local closure, recoil impulse, then spring/gaze peek. `02-eye-tap`. | Pending |
| Repeated region | `03-repeat`: first tap keeps 86/86 px eye height at 580 ms, second closes to 8/8 at 900 ms; third has stronger avoidance. Direct eye taps preserve the explicit first local blink. | Pending |
| Alternating taps | `04-alternate`: successive sides reverse body pursuit; alternation count reaches 5; real eye-center roll accompanies the direction. | Pending |
| Continuous movement | `05-follow`: gaze leads retained-velocity spring motion with bounded edge approach. | Pending |
| Back-and-forth petting | `06-stroke`: reversal-based relaxation ramp, signed touch velocity drives sway and roll. 150 ms body / 160 ms comfort release pause precedes recovery. | Pending |
| Cupping and one-side release | `07-cup-release`: after left contact leaves, working face center moves from x236.824 (2,280 ms) to x244.206 (2,800 ms) toward the surviving right contact; comfort remains. | Pending |
| Pinch and spread | `08-pinch-spread`: separation/width change with distance, inverse height change suggests volume, release rebounds once. | Pending |
| Both eyes held, one side lifts | `09-eyes-release`: both 8/8 px before lift; 65.205/8 px after left lift at 2,500 ms; both 86 px after final release. | Pending |

The same nine scenarios are replayed in idle, working, tool, waiting, done,
error, cancelled and disconnected states. Protected expressions keep their
semantic topology and only permit bounded displacement. A separate roll field
moves eye centers; business eyelid tilt is not used as a substitute for head
inclination. Pose blending includes roll and existing spiral fields.

## Verification performed

- Strict C exporter build passed: C99, Wall, Wextra, Werror.
- ESP-IDF v6.1 frozen v3 firmware build passed. See build.log.
- Nine-row animated working-state geometry preview generated and still inspected:
  nine-interactions.gif / nine-interactions.png. Orange dots mark touch inputs.
  These are offline renderings, not video of the device.
- Existing real LVGL checks: 5/14 pass, 9 failures remain. Current production
  label expectation fails at test_lvgl_ui.c:133; old hold/tab/return timing and
  rotation assumptions fail in the remaining eight cases. See
  existing-lvgl-checks.log. No tests/assertions were added or rewritten. The
  suite is not green and is not used to claim full regression acceptance.
- Source hygiene: diff --check passed on touched files.
- Current 8 MiB app partition backed up; only 0x10000 app written through Bridge
  pause/resume; esptool hash verified. Device readback is connected, firmware
  3.1.0, protocol 3, working-state projection with selection revision 22 intact.

## Still required on hardware

The nine subjective/visible behaviors, crossing points, either finger leaving,
transient point loss, rotation, edge-navigation conflicts, first wake touch,
Agent confirmation/failure, usage navigation and state updates must be assessed
separately from offline output. A physical check for repeated cheek pokes,
stroking and independent eye release is currently requested; live-trace.jsonl
records samples. Do not mark this table physically accepted merely because
input counts or a new firmware hello are present.
