# Audio build verification — 2026-09-08

Fresh ESP-IDF v6.1 three-configuration builds completed. These are compile/link and image checks only; no flash, microphone acceptance or performance claim.

| Build | Bytes | SHA-256 | Audio | Probe | G0 verified |
|---|---:|---|---|---|---|
| off | 946176 | 83c3cacb15cf1771cd81ee716a3af9a6e09f2f87ef4eb369bb48cb7245c496d2 | False | False | False |
| on | 946656 | e33a90d6fa3389e4186b526f088eb3d9b965145f9bf27d3aee10ef65669b84c0 | True | False | False |
| g0 | 228160 | 3d69ef7287b92f26f01fac0dfbb7621fba52d00cdde4c52f8863b302964316ae | True | True | False |

## Reproduce

Activate `tools/idf-env.sh`. Build directories are repository-root paths, SDKCONFIG paths are explicit so the working firmware/sdkconfig is not rewritten:

```sh
idf.py -C firmware -B build-audio-off -D SDKCONFIG=/tmp/bot-audio-off.sdkconfig build
idf.py -C firmware -B build-audio-on -D SDKCONFIG=/tmp/bot-audio-on.sdkconfig build
idf.py -C firmware -B build-audio-g0 -D SDKCONFIG=/tmp/bot-audio-g0.sdkconfig build
zsh tools/g0_probe.sh --build build-audio-g0 --port /dev/cu.usbmodem21201 --validate-only
```

Configuration copies started from existing firmware/sdkconfig. OFF explicitly disables BOT_AUDIO_ENABLE. ON enables it but keeps G0_VERIFIED/PROBE/DIAGNOSTICS off. G0 enables AUDIO+PROBE, keeps VERIFIED off, and routes primary console to USB_SERIAL_JTAG solely for the development probe. All three retain core dumps disabled. The recorded serial path was used for metadata validation only; validation does not open it and future operators must discover the current device port.

G0 validator succeeded, verified current image markers, configuration, build metadata and flasher hashes, and did not open/reset/flash a device. The application header is stamped with the dirty source baseline from this implementation; these hashes are not a signed release.

Both managed dependency manifest and lock are unchanged from 231f590. The I2S source is unmodified in the SDK; audio builds use a hash-guarded derived translation unit whose volatile erasure precedes each DMA buffer free. Existing upstream LVGL section-attribute warnings remain. Initial sandbox dependency scan failed on macOS psutil/sysctl permissions; rerun with approved local build permission completed.

## Other verification

- Pure C fixed input diagnostic and real owner + scripted port lifecycle diagnostic: strict warnings, ASan/UBSan pass. See core-replay.md and service-lifecycle-replay.md.
- Actual LVGL scenario/control render results: ui/README.md and pixel-comparison.csv. This is software rendering, not panel footage.
- Existing motion suite:3/3pass. Native suite:2/6pass, same4fail as independent231f590 archive. LVGL:5/14pass, same9fail. Legacy fake-LVGL lifecycle target lacksbot_link.h on both baselines; face_motion has the same staleCANCELLED->SLEEP assertion. Full logs retained under regressions/ and ui/. No failing expectation was changed to force green.
- git diff --check passes. No frontend/backend unit tests were added; only existing seam/build declarations and standalone diagnostic runners were updated.

## Physical work still outstanding

G0 A/B sound input, actual100cycle resource/stop report, mechanical-noise correlation, precision/recall, physical responsep95, CPU/DMA/PSRAM/stack and8-hour soak. Runtime opening remains blocked by BOT_AUDIO_G0_VERIFIED=n. Firmware build success cannot enable that gate.
