#!/usr/bin/env bash
# Reusable performance QA: 300-frame uncapped benchmark at five representative
# cameras, in both graphics presets. Run ALONE (no other GPU work) for valid numbers.
# Usage:  godot/qa/bench_views.sh <path-to-godot-console-exe> [log_file]
#         (or set FIRETEAM_GODOT and omit the first argument)
set -u
GODOT="${1:-${FIRETEAM_GODOT:-}}"
[ -n "$GODOT" ] || { echo "usage: $0 <godot-console-exe> [log_file]  (or export FIRETEAM_GODOT)"; exit 2; }
HERE="$(cd "$(dirname "$0")/.." && pwd)"
LOG="${2:-$HERE/qa/arena_final_candidate/logs/bench_log.txt}"
mkdir -p "$(dirname "$LOG")"
: > "$LOG"
for PRESET in performance high; do
  for V in teal_spawn under_canopy canopy_ground trees walls; do
    if [ "$PRESET" = performance ]; then EXTRA="--preset performance"; else EXTRA=""; fi
    R=$("$GODOT" --path "$HERE" --rendering-driver vulkan --resolution 1600x900 --quit-after 600 -- --bench --view "$V" $EXTRA 2>&1)
    echo "[$PRESET] $(echo "$R" | grep BENCH_FPS)" | tee -a "$LOG"
  done
done
