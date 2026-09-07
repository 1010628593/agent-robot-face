#!/bin/sh
set -eu
# Reproducible: NotoSansSC[wght].ttf from google/fonts/ofl/notosanssc.
# Use a static Regular (wght=400) instance; see README.md for variable-font conversion.
# Input is supplied externally; full font is deliberately not distributed.
font=${1:?Pass NotoSansSC ttf path}
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
python3 - "$root" <<'PY'
import pathlib,re,sys
r=pathlib.Path(sys.argv[1]);chars=set()
for p in (r/'firmware/components/bot_ui').glob('*.c'):
 if p.stem.endswith('_dev'):continue
 for s in re.findall(r'"([^"\n]*)"',p.read_text()):chars.update(c for c in s if ord(c)>127)
(r/'tools/fonts/characters.txt').write_text(''.join(sorted(chars)))
PY
fontconv() {
 if [ -n "${LV_FONT_CONV:-}" ]; then node "$LV_FONT_CONV" "$@"; else npx --yes lv_font_conv@1.5.3 "$@"; fi
}
for size in 22 24; do
 fontconv --font "$font" --size "$size" --bpp 4 --format lvgl --lv-include lvgl.h --range 0x20-0x7e --symbols "$(cat "$root/tools/fonts/characters.txt")" --no-compress --no-prefilter --output "$root/firmware/components/bot_ui/assets/bot_font_$size.c"
done
