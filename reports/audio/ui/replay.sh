#!/bin/sh
# Standalone visual diagnostic; not a unit-test suite or hardware acceptance.
set -eu
runner=${1:-/tmp/robot-face-lvgl/replay_os_ui}
report=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
out=${2:-$(mktemp -d /tmp/bot-audio-ui.XXXXXX)}
mkdir -p "$out"
for trace in "$report"/*.txt; do
 name=$(basename "$trace" .txt)
 mkdir -p "$out/$name"
 "$runner" "$out/$name" < "$trace" > "$report/$name.csv"
done
python3 - "$report" "$out" <<'PY'
from pathlib import Path
import csv,sys
report,out=map(Path,sys.argv[1:])
with (report/'pixel-comparison.csv').open('w') as f:
 w=csv.writer(f);w.writerow(['scenario','frame','changed_rgb_bytes'])
 for control in sorted(report.glob('*-control.txt')):
  name=control.stem.removesuffix('-control')
  for p in sorted((out/name).glob('*.ppm')):
   a=p.read_bytes();b=(out/control.stem/p.name).read_bytes()
   w.writerow([name,p.stem,sum(x!=y for x,y in zip(a,b))])
PY
printf 'PPM frames retained in %s\n' "$out"
