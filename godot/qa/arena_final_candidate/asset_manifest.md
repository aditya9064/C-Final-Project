# Asset manifest & pipeline verification

## One authoritative generator per output
| Output (under `godot/assets/models/`) | Generator | Notes |
|---|---|---|
| crate, case, barrel, sandbag_pile, barrier_low/tall/L, spawn_arch, canopy*, rock | `pipeline/gen_assets.py` (Blender 4.2, `--factory-startup`) | *canopy.glb is still exported but **no longer used** by the scene (procedural canopy replaced it) |
| tree_a/b/c, shrub, ivy | `pipeline/gen_vegetation.py` | only `ivy.glb` is used by the scene now (tree_*/shrub superseded by the MegaKit) |
| terrain | `pipeline/gen_terrain.py` | muted palette, soft gravel apron |
| nature/* (16 models, `.gltf` + `.bin` + shared `textures/`) | `pipeline/convert_nature.py` ← Quaternius Stylized Nature MegaKit glTF | textures stored once (≈17 MB vs 54 MB embedded); converted twice → 41/41 files byte-identical |
| textures/foliage/ivy_leaf.png | copied from MegaKit `Leaves_NormalTree_C.png` | used by the ivy strands and bush recolour |

`gen_assets.py` does **not** write any tree/shrub/ivy/nature path (the earlier clobber bug). The
scene builder `scripts/build_arena.gd` instances everything; props carry separate box/cylinder colliders.

## Licences
| Source | Licence | Files |
|---|---|---|
| Quaternius — Stylized Nature MegaKit (free/standard) | CC0 1.0 Universal (public domain) | `nature/CommonTree_1..5, Bush_Common, Bush_Common_Flowers, Fern_1, Grass_Common_Tall, Grass_Wispy_Tall, Rock_Medium_1..3, Pebble_Round_1/3/5`, `textures/foliage/ivy_leaf.png` — see `nature/CREDITS.txt` |
| Poly Haven HDRI / ambientCG textures (earlier passes) | CC0 | `assets/hdri/`, `textures/` |
| Everything else | project-authored (Blender scripts / GDScript) | — |

No paid or unlicensed assets. Download provenance: `https://opengameart.org/content/stylized-nature-megakit`
(`stylized_nature_megakitstandard.zip`, 104 MB), licence text inside the archive.

## Repeatability test (run this pass)
1. `gen_assets.py` run **twice**, then `gen_vegetation.py`, then `gen_terrain.py`.
2. MD5 of 22 protected files (the then-GLB `nature/*`, `tree_a/b/c.glb`, `ivy.glb`, `shrub.glb`, `terrain.glb`)
   compared before/after: **all 22 identical** (generators are deterministic; gen_assets touches none of them).
3. `tree_b.glb` structure after regen: nodes `tree_b_wood` + `tree_b_leaves`, materials `bark` + `leafmat`,
   no merged "Cone" mesh.
4. `godot --import` after regeneration: 0 errors; all 17 QA views rendered with 0 script/runtime errors;
   cover tests 5/5.
Log: `logs/regen_test.txt` (copied from the scratchpad run).

## Import checks
- Foliage alpha: MegaKit materials are `alphaMode MASK` (cutoff 0.2, double-sided) → Godot alpha-scissor,
  no white cards. `Bush_Common` ships an autumn-red atlas → only `Bush_Common_Flowers` is used, with its
  flower surface recoloured green in-engine.
- `Fern_1` is converted but **not placed** (2.7 m native + alpha-blend material read as translucent ghosts).
- Trees are placed on the sculpted terrain via a vertex-height lookup (`_terrain_height`), so trunks meet
  the ground on the hills.
