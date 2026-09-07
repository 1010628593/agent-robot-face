# Official icon correction — 2026-09-07

`comparison.png` shows official sources, previous firmware imagery, and regenerated assets. `device/0000.png` is the production LVGL usage overview rendered with explicit sample data, not a hardware capture. Other device frames replay the existing usage review trace.

Changed: replace hand-built OpenAI knot, grayscale WorkBuddy, old Cursor artwork, and cropped Hermes helmet with documented official artwork. Both firmware picker/stats reference the regenerated existing descriptors. Desktop packaging now uses high-quality PNGs rather than decoding the firmware RGB565 data.

Validation: all four 96px assets contain 18,432 bytes of RGB565 data; source hashes recorded; desktop resources match the generated 512px files; macOS app build and ad-hoc signature verification passed. Diagnostic LVGL render completed. The first strict replay build encountered an existing logical-op-parentheses warning in stats.c; the separate diagnostic compile allowed that warning without changing the source. No unit tests added/run, browser UI validation, installation, or device flashing performed.

Official source limitations: Codex uses OpenAI's brand-portal Blossom mark. Hermes website icon is 48px; larger renders interpolate it without inventing detail.

## Device deployment

User authorized device update. ESP-IDF v6.1 build passed after adding braces/newline around an existing stats.c conditional to resolve misleading-indentation without behavior changes. Application-only flash at 0x10000 completed; esptool verified the written hash and reset the device. All four current RGB565 arrays were found byte-for-byte in the deployed binary. SHA-256 is recorded in firmware-official-icons.sha256.

Post-reset Bridge readback: connected=true, firmware=3.0.1, protocol=3, error=null, paused=false; new rendering window reported 77 updates, projection=working/face, usage data_rev=178. This icon-only deployment retains the protocol firmware version. No physical screen photograph was captured.
