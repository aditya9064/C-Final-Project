# Arena final candidate — QA package

Evidence for the Godot arena environment on `feature/rendering`. Everything here was
produced by running the real Godot scene (`godot/qa/capture_views.sh`,
`godot/qa/bench_views.sh`), not by hand.

| Document | Contents |
|---|---|
| `reference_comparison.md` | per-view comparison against the concept-art references (not committed, see `art/references/README.md`), severity, fix, status |
| `implementation_summary.md` | what changed in the final production pass, what was preserved, files touched |
| `asset_manifest.md` | generators, licences (CC0), repeatability test |
| `collision_report.md` | locked dimensions, raycast results, new colliders |
| `performance_report.md` | 5 cameras × 2 presets, frame-time stability |
| `known_issues.md` | honest remaining visual/technical limitations |
| `final/*.jpg` | 17 Godot screenshots (14 required views + 3 extras), 1600×900, JPEG q0.92 |
| `logs/` | render log, per-view cover-test output, benchmark log, regeneration log + hashes |

Screenshots are stored as JPEG to keep the repository small; `capture_views.sh` writes
lossless PNGs first and converts them with `godot/qa/png_to_jpg.gd`.

## Reproduce
From the repository root (`<godot>` = Godot 4.7.x console executable, or set `FIRETEAM_GODOT`):
```
# interactive walk-through (WASD + mouse, Shift = fast, Q/E = down/up)
<godot> --path godot --rendering-driver vulkan --resolution 1600x900
<godot> --path godot --rendering-driver vulkan --resolution 1600x900 -- --preset performance

# one fixed QA camera -> godot/arena_<view>.png (gitignored; also prints the COVER_TEST lines)
<godot> --path godot --rendering-driver vulkan --resolution 1600x900 --quit-after 420 -- --view teal_spawn

# all 17 QA cameras + cover logs into this folder (PNG -> JPEG)
bash godot/qa/capture_views.sh <godot>

# benchmarks (run alone, no other GPU work) -> logs/bench_log.txt
bash godot/qa/bench_views.sh <godot>
```
Views: overview, topdown, overview_b, canopy_ground, under_canopy, teal_spawn, orange_spawn,
cover_tall, cover_low, firing_port, cargo, trees, landscape, walls, cover, crate, cinematic.

## Regenerate assets
Needs Blender 4.2 LTS; see `godot/pipeline/README.md` (tool paths are per machine and not
committed — `pipeline/setup.ps1` or the `FIRETEAM_BLENDER` environment variable).
```
<blender> --background --factory-startup --python godot/pipeline/gen_assets.py     -- <abs>/godot/assets/models
<blender> --background --factory-startup --python godot/pipeline/gen_vegetation.py -- <abs>/godot/assets/models
<blender> --background --factory-startup --python godot/pipeline/gen_terrain.py    -- <abs>/godot/assets/models
<blender> --background --factory-startup --python godot/pipeline/convert_nature.py -- <megakit>/glTF <abs>/godot/assets/models/nature
<godot> --headless --path godot --import
```
