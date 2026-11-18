#!/usr/bin/env bash
# Reusable QA capture: renders every fixed QA camera from the REAL Godot scene,
# records cover-test results per view, and copies the PNGs into the QA folder.
# Usage:  godot/qa/capture_views.sh <path-to-godot-console-exe> [out_dir]
#         (or set FIRETEAM_GODOT and omit the first argument)
set -u
GODOT="${1:-${FIRETEAM_GODOT:-}}"
[ -n "$GODOT" ] || { echo "usage: $0 <godot-console-exe> [out_dir]  (or export FIRETEAM_GODOT)"; exit 2; }
HERE="$(cd "$(dirname "$0")/.." && pwd)"            # godot/
OUT="${2:-$HERE/qa/arena_final_candidate/final}"
LOGS="$(dirname "$OUT")/logs"
mkdir -p "$OUT" "$LOGS"
VIEWS="overview topdown overview_b canopy_ground under_canopy teal_spawn orange_spawn cover_tall cover_low firing_port cargo trees landscape walls cover crate cinematic"
: > "$LOGS/render_log.txt"
for V in $VIEWS; do
  R=$("$GODOT" --path "$HERE" --rendering-driver vulkan --resolution 1600x900 --quit-after 420 -- --view "$V" 2>&1)
  P=$(echo "$R" | grep -c 'COVER_TEST \[PASS\]'); F=$(echo "$R" | grep -c 'COVER_TEST \[FAIL\]')
  E=$(echo "$R" | grep -ciE 'SCRIPT ERROR|Parse Error|Failed to load')
  echo "$V: captured=$(echo "$R" | grep -c ARENA_CAPTURE) cover_pass=$P cover_fail=$F script_errors=$E" | tee -a "$LOGS/render_log.txt"
  echo "$R" | grep -E "COVER_TEST" > "$LOGS/cover_${V}.txt"
  cp "$HERE/arena_${V}.png" "$OUT/${V}.png" 2>/dev/null
done
# Keep the committed evidence small: convert the copied PNGs to JPEG (q 0.92).
"$GODOT" --headless -s "$HERE/qa/png_to_jpg.gd" -- "$OUT" 2>&1 | grep -E "PNG_TO_JPG|ERROR"
echo "done -> $OUT"
