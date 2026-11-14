# Fireteam: Skirmish — Godot 4.7 arena environment

The 3D arena for **Fireteam: Skirmish** as a standalone Godot 4.7 project: a chamfered
octagonal walled arena with a central tensioned canopy, teal/orange spawn gateways,
concrete and sandbag cover at gameplay-tested heights, props, a wooded exterior, PBR
materials and HDRI lighting — built to match the team's concept-art references (kept locally — see
`../art/references/README.md`; the project does not need them to run).

> **Scope.** This is the **environment only**. It is not wired to the C++/SFML program in
> `../src`, contains no gameplay, networking, weapons or AI, and the mannequins are
> primitive scale placeholders. How it could become the game's renderer is in
> [`../docs/rendering-architecture.md`](../docs/rendering-architecture.md).

## Requirements
- **Godot 4.7.x** standard build (no C#/.NET needed). The console build (`*_console.exe`)
  is handy for the command-line runs below because it prints to the terminal.
- A **Vulkan**-capable GPU (Forward+ renderer). Works on an Intel integrated GPU.
- Blender and Python are **only** needed to regenerate assets (see `pipeline/README.md`).

## Run it
```bash
# interactive: open godot/project.godot in the editor and press F5, or from the repo root:
<godot> --path godot --rendering-driver vulkan --resolution 1600x900
<godot> --path godot --rendering-driver vulkan --resolution 1600x900 -- --preset performance
```
First launch imports the assets (~1 minute). Every launch prints the five cover raycast
results (`COVER_TEST [PASS] ...`) to stdout.

| Input | Action |
|---|---|
| Mouse / W A S D / Q E / Shift | free-fly camera: look / move / down-up / faster |
| **1** / **2** / **3** | tactical angled view / reset free-fly / orthographic top-down |
| **Esc** | release the mouse |

### Fixed QA cameras, screenshots, benchmark
```bash
# one fixed camera -> godot/arena_<view>.png (gitignored), then quit
<godot> --path godot --rendering-driver vulkan --resolution 1600x900 --quit-after 420 -- --view teal_spawn
# 300-frame FPS benchmark at a camera (run alone)
<godot> --path godot --rendering-driver vulkan --resolution 1600x900 --quit-after 600 -- --bench --view under_canopy -- --preset performance
# all 17 QA cameras + cover logs -> qa/arena_final_candidate/ ; all benchmarks -> logs/bench_log.txt
bash godot/qa/capture_views.sh <godot>      # or: export FIRETEAM_GODOT=<godot>
bash godot/qa/bench_views.sh <godot>
```
Views: `overview topdown overview_b canopy_ground under_canopy teal_spawn orange_spawn
cover_tall cover_low firing_port cargo trees landscape walls cover crate cinematic`.
`--headless` cannot render (no GPU), so screenshots need a windowed run.

## How the arena is built
Everything is assembled procedurally by **`scripts/build_arena.gd`**, the single entry
point (attached to the root node of `scenes/arena.tscn`; the scene file itself only holds
the `WorldEnvironment` — sky, tonemap, SSAO/SSIL, glow). The script is organised in
numbered sections (search for the `=====` banners):

| # | Section | What it owns |
|---|---|---|
| 1 | Arena geometry & dimensions | `PLAYER_HEIGHT`/eye constants, palette, octagon `VERTS`, `WALL_H`/`WALL_T` |
| 2–3 | Materials, helpers | triplanar PBR + flat paint materials; `_box/_cyl/_mesh/_collider_*`, stripes, terrain height lookup |
| 4 | Ground & terrain | slab, sculpted `terrain.glb`, joints, white ring + orange arc |
| 5 | Walls and boundaries | eight collided wall panels, corner columns, inner-face detail, ivy |
| 6 | Canopy and central portal | posts, eave/ridge beams, generated fabric roof, light bars, banners, walk-through portal |
| 7 | Spawn zones | teal bay at x = −18, orange bay at x = +18 |
| 8 | Cover, collision and props | barriers (tall/low/L/loophole), sandbags, crates, barrels, tarp pallet |
| 9 | Trees and environment | seeded CC0 woodland belt, bushes, rocks on the terrain |
| 10 | Placeholder test humans | temporary 1.80 m mannequins at the tested spots |
| 11 | Lighting | sun, canopy fill, warm sky fill |
| 12 | Rendering-quality presets | `high` (default) / `performance` |
| 13 | Cameras, capture, benchmark | the `VIEWS` table, `--view`, `--bench` |
| 14 | Cover raycast tests | five automated checks |

Design decisions worth knowing before editing:
- **Visual meshes and collision are separate.** Blender GLBs are looks only; every cover
  piece gets a simple `StaticBody3D` box/cylinder sized to its *gameplay* dimension.
  Hitscan and movement therefore see clean, predictable shapes.
- **All gameplay numbers live in one file**, so a reviewer can verify the whole layout
  from `build_arena.gd` alone, and gameplay code can import the same constants later.
- Painted stripes/markings are thin flat polygons a few millimetres off the surface
  (no decals, no z-fighting); exterior props are grounded on the terrain by a vertex
  height lookup; the forest uses a fixed random seed so every run is identical.

## Dimensions and cover (as built, metres)
| Element | Value |
|---|---|
| Footprint | chamfered octagon 42 (X) × 34 (Z), 5 m chamfers; vertices `(±16,±17) (±21,±12)` |
| Walls | 4.50 high × 0.45 thick, collided; 0.7 m corner columns |
| Canopy | posts 0.46 m at X,Z = ±4.65; eave/clearance 3.75; apex 5.0; fabric ≈10.6 × 10.6 |
| Portal | 0.8 m pillars, 2.4 m opening, lintel 2.9 m, walk-through |
| Spawns | bays at x = ±18: opening 5.5 wide × 3.4 high, 2.1 deep, facing the centre |
| Tall cover | 2.10 m — `(±12, ±9)`, loophole walls `(−5,−9.5) (5,9.5)` |
| Low cover | 1.28 m — `(±8, ±4)`, `(0, ±10)`; sandbags ≈1.26 m |
| L cover | 1.45 m — `(±15, ±8)` |
| Player scale | height 1.80, standing eye 1.63, crouch 1.10, crouch eye 0.92, radius 0.32 |
| Centre ring | white annulus R 8.08–8.34 with a 70° orange arc |

### Cover raycast tests (run every launch, section 14)
Real physics rays against the collision above — the same call a hitscan weapon makes:
1. standing eye → standing eye over 1.28 m cover: **exposed** ✔
2. standing eye → crouching eye behind 1.28 m cover: **concealed** ✔
3. standing eye → standing eye behind 2.10 m cover: **concealed** ✔
4. standing eye → top of head behind 2.10 m cover: **concealed** ✔
5. bullet at 0.6 m into the low barrier: **blocked** ✔

## Rendering presets and measured performance
Intel iGPU, 1600×900, uncapped, 300 timed frames per camera (`qa/bench_views.sh`):

| Preset | What it enables | FPS across 5 cameras |
|---|---|---|
| `high` (default) | MSAA 4×, SSAO + SSIL, glow, soft shadows to 140 m | 37–39 |
| `performance` (`-- --preset performance`) | MSAA off, no SSAO/SSIL/glow, hard shadows to 55 m | 88–148, worst frame 17 ms, 0 frames > 33 ms |

Full report: `qa/arena_final_candidate/performance_report.md`.

## Folder layout
```
godot/
├── project.godot              Godot 4.7 project (Forward+)
├── scenes/arena.tscn          root scene: WorldEnvironment + build_arena.gd
├── scripts/build_arena.gd     THE arena builder (see table above)
├── assets/models/*.glb        props generated by pipeline/gen_*.py (Blender)
├── assets/models/nature/      CC0 Quaternius trees/bushes/rocks as .gltf + shared textures/ (+ CREDITS.txt)
├── assets/hdri/sky.hdr        CC0 Poly Haven sky
├── assets/MANIFEST.md         every asset, its source and licence
├── textures/                  CC0 ambientCG PBR sets + foliage atlas
├── pipeline/                  Blender/Python generators, fetcher, setup (README inside)
├── qa/                        capture/bench scripts + arena_final_candidate/ evidence
└── cpp/README.md              GDExtension integration PLAN (nothing built yet)
```
`.godot/` (import cache), `arena_*.png` captures and `assets/blend/` are generated locally
and gitignored.

## Assets and licences
All third-party content is **CC0 1.0** (ambientCG textures, Poly Haven HDRI, Quaternius
Stylized Nature MegaKit); everything else is generated by the project's own Blender
scripts or written in GDScript. Details and provenance: `assets/MANIFEST.md`,
`assets/models/nature/CREDITS.txt`.

## What is unfinished
- Not integrated with the C++ game; no gameplay in Godot (`../docs/rendering-architecture.md`).
- Placeholder mannequins; no character meshes or animation.
- Visual gaps versus the references and technical limits are listed honestly in
  `qa/arena_final_candidate/known_issues.md` (stylised rather than painterly vegetation,
  simple prop detail, High preset below 60 FPS on integrated graphics, cover tests cover
  two barriers, not every sightline).
