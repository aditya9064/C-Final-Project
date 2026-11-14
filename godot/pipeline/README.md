# Asset pipeline — Blender 4.2 (headless Python) → GLB → Godot

Everything in `godot/assets/models/` is **reproducible from these scripts**; nothing was
hand-modelled. You only need this folder to *change or regenerate* assets — the
committed GLBs and textures already work.

```
fetch_assets.py     → downloads the CC0 textures (ambientCG) + HDRI sky (Poly Haven)
gen_assets.py       → Blender: barriers, crates, cases, barrels, sandbags, canopy*, rock*, spawn arch*  → GLB
gen_vegetation.py   → Blender: ivy strands (+ legacy tree_a/b/c, shrub*)                               → GLB
gen_terrain.py      → Blender: sculpted vertex-coloured terrain                                        → GLB
convert_nature.py   → Blender: Quaternius MegaKit glTF → .gltf/.bin + one shared textures/ folder
godot --import      → Godot imports GLBs/textures (headless OK)
build_arena.gd      → instances the meshes, applies materials + collision, builds the scene
```
`*` = still generated for determinism but not instanced by the current scene.

## Tools and local configuration (not committed)
| Tool | Version | How the scripts find it |
|---|---|---|
| Blender | **4.2 LTS** (tested 4.2.23) | you pass the path on the command line; `setup.ps1` can install a portable copy and records it in `config.local.json` (gitignored). Env var `FIRETEAM_BLENDER` overrides. |
| Godot | 4.7.x console build | `qa/*.sh` take it as argument 1 or read `FIRETEAM_GODOT`. |
| Python | 3.x | `fetch_assets.py` only (stdlib). |

`config.json` is a **template with empty paths**; never put a machine path in it.

```powershell
# Windows: install/verify a portable Blender 4.2 (no admin) and write config.local.json
powershell -ExecutionPolicy Bypass -File godot\pipeline\setup.ps1
```

## Regenerate (from the repo root; `<blender>` = blender.exe, `<abs>` = absolute repo path)
```bash
python -I godot/pipeline/fetch_assets.py                     # textures + HDRI (skips existing)
<blender> --background --factory-startup --python godot/pipeline/gen_assets.py     -- <abs>/godot/assets/models
<blender> --background --factory-startup --python godot/pipeline/gen_vegetation.py -- <abs>/godot/assets/models
<blender> --background --factory-startup --python godot/pipeline/gen_terrain.py    -- <abs>/godot/assets/models
<blender> --background --factory-startup --python godot/pipeline/convert_nature.py -- <megakit>/glTF <abs>/godot/assets/models/nature
<godot> --headless --path godot --import
```
`--factory-startup` matters: Blender's bundled glTF exporter only loads reliably with
factory settings in a headless run. The MegaKit zip (`stylized_nature_megakitstandard.zip`,
CC0) is downloaded from opengameart.org and extracted to a scratch folder; only the
converted GLBs are committed.

Generators also save editable `.blend` sources to `assets/blend/` (gitignored, with a
`.gdignore` so Godot ignores them). Outputs are deterministic — regenerating produces
byte-identical GLBs (verified: 22/22 hashes, `qa/arena_final_candidate/logs/regen_test.txt`).

## What each script does
- **`fetch_assets.py`** — stdlib downloader for three ambientCG 1K texture sets and one
  Poly Haven 2K HDRI; size-checks files; `--force` re-downloads. Edit the dicts at the top
  to add sets.
- **`gen_assets.py`** — procedural props: bevelled concrete barriers (tall/low/L), framed
  crate and equipment case, ridged barrel, wrinkled sandbag pile, plus the legacy canopy,
  rock and spawn arch. Exports Y-up GLBs with simple named materials.
- **`gen_vegetation.py`** — ivy strand (`ivy.glb`, used on the wall tops) and the legacy
  procedural trees/shrub. Objects are named `*_wood` / `*_leaves` so the scene can assign
  bark vs alpha-cutout leaf materials per surface.
- **`gen_terrain.py`** — displaced grid with vertex colours (grass / dry grass / dirt /
  gravel apron), flat under the arena. The scene reads its vertices to ground props.
- **`convert_nature.py`** — joins each MegaKit model into one mesh, bakes transforms,
  exports `.gltf` + `.bin` with `GLTF_SEPARATE` so the kit's shared atlases are written
  once to `nature/textures/` (≈17 MB) instead of being embedded in every model (54 MB).
  The committed `textures/*.png.import` files set `mipmaps/generate=true` (what Godot
  used for the embedded textures); keep them, or distant foliage will alias.
- **`verify_blender.py`** — self-test used by `setup.ps1` (imports bpy, exports a GLB).

## Notes / limits
- Meshes carry no UVs by design; Godot applies world-space triplanar materials.
- `build_arena.gd` loads `.glb` for project props and falls back to `.gltf` for the nature
  kit (`_load_scene`). The old `.obj` outputs are obsolete.
- Characters are not part of this pipeline (placeholder mannequins are primitives in
  the scene).
