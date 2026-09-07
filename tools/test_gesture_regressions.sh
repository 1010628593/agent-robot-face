#!/usr/bin/env bash
# Host-only regression checks; no ESP-IDF, device access or serial writes.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$(mktemp -d "${TMPDIR:-/tmp}/bot-gesture-tests.XXXXXX")"
trap 'rm -rf "$BUILD"' EXIT
COMPILER="${CC:-cc}"
FLAGS=(-std=c99 -Wall -Wextra -Werror)
if [[ "${SANITIZE:-0}" == "1" ]]; then
  FLAGS+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
fi
CORE="$ROOT/firmware/components/bot_core"
for name in test_gesture test_gesture_coalesced; do
  "$COMPILER" "${FLAGS[@]}" -I"$CORE/include" \
    "$ROOT/tests/native/$name.c" "$CORE/gesture.c" -o "$BUILD/$name"
  "$BUILD/$name"
done
