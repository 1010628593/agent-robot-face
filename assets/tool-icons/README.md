# Official tool artwork

Retrieved 2026-09-07. `sources.json` records official page, download URL, and SHA-256 of the checked-in PNG inputs. Trademarks belong to their respective owners; these images identify the integrated tools.

- Codex: OpenAI Blossom favicon from the official OpenAI brand portal (304 × 304). Replaces the hand-built knot approximation; this is the OpenAI brand mark, not a separately claimed Codex product logo.
- WorkBuddy: full-color 40-unit SVG linked by the official homepage, rasterized at 512 × 512 using sharp (density 921.6). Preserve gradients, complete silhouette, and both eyes.
- Cursor: official 192 × 192 light application icon linked by the website. The brand-page SVG is retained for reference only; it is not used by the generator.
- Hermes: complete 48 × 48 avatar from the official website's icon.png. Its limited source resolution remains visible at large sizes; no invented details or helmet-only crop.

Run `python3 tools/generate_device_icons.py` with Pillow installed. The process is offline and deterministic: preserve full artwork and aspect ratio, fit into five-sixths of the canvas, alpha-composite over black, then export 512px desktop PNGs and 96px RGB565 firmware assets. No thresholding or forced grayscale. Desktop packaging copies the 512px files directly.

WorkBuddy SVG rasterization, if needed: `sharp(input, {density: 921.6}).resize(512, 512).png()`; retain the original SVG beside the raster source. Python generation only requires the checked-in PNGs.
