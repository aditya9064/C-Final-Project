# Characters & Weapons Phase — Status (2026-11-15)

Autonomous work session. Budget: **$0** — only free CC0 assets and free tools
(Blender 4.2, Godot 4.7) were used. The existing arena, lighting, weapons systems,
and C++/multiplayer code were **not modified**. Work is on branch `feature/characters`
(local commits only; nothing pushed).

## Done

- **Weapons (original):** `pipeline/gen_weapons.py` procedurally builds two-tone SMGs
  (`smg_teal.glb`, `smg_orange.glb`) with a dedicated accent material slot — matching
  the concept-sheet rifles. Rendered via `scenes/showcase.tscn`.
- **CC0 assets acquired & verified** (see `assets/models/characters/CREDITS.txt`):
  Quaternius Universal Base Characters + Universal Animation Library 1 & 2, all CC0,
  downloaded free from itch.io on 2026-11-15.
- **Rigged, animated characters in Godot:** `male_dressed.glb` (Teal) and
  `female_dressed.glb` (Orange), built by `pipeline/assemble_character.py` +
  `pipeline/dress_character.py`:
  - Realistic **athletic** base bodies (not blocky), ~14k tris, 65-bone humanoid rig,
    **native height 1.86 m** (verified in-engine) — rendered at true 1:1 scale, no rescale.
  - Brown hair + brown eyebrows; uniform charcoal tactical suit with team-colour
    (teal/orange) forearm accents and dark boots.
  - 12 animation clips bundled per character, **no retargeting needed** (base and
    animation library share the identical skeleton). Cyclic clips loop correctly in-engine
    (idle/walk/run/jog/aim_idle/jump → `loop_mode=LINEAR`; one-shots stay non-looping).
  - Each SMG attaches to the `hand_r` bone (`BoneAttachment3D`) at true size and moves
    with the rig.
  - Rendered front/side/back (`duo_front/side/back.png`, `char_*.png`).

### Corrected this session (bugs found via in-engine diagnostics)
- **Scale:** an earlier `0.63` scale (from a bad Blender bbox) shrank characters to 1.17 m
  and made the gun look oversized. Measured true height = 1.86 m → scale set to 1.0.
- **Looping:** cyclic clips had imported with `loop_mode=0`. Godot's glTF importer only
  loops clips whose name ends `_loop` (which it then strips), so the pipeline now names
  cyclic actions `idle_loop`/`walk_loop`/… → import as clean `idle`/`walk`/… that loop.
- **White eyebrows:** base eyebrow mesh shipped white → now tinted to hair brown.

### Animation coverage (requirement: idle/walk/run/jump/aim/fire/reload/death)
| Required | Status | Source clip |
|---|---|---|
| idle | ✅ | `idle` (UAL1 Idle_Loop) |
| walk | ✅ | `walk` (UAL1 Walk_Loop) |
| run  | ✅ | `run` (UAL1 Sprint_Loop) + `jog` |
| jump | ✅ | `jump_start` + `jump_loop` + `jump_land` |
| death | ✅ | `death` (UAL1 Death01) |
| aim / fire / reload | ⚠ placeholder | currently the CC0 **pistol** clips (`aim`/`fire`/`reload`). Rifle-specific two-handed versions do NOT exist in free UAL1/UAL2 — must be derived (see below). |

## Remaining work / known limitations (honest)

1. **Outfit fidelity (main remaining task):** current clothing is a *recoloured, slightly
   inflated shell of the body* — a fitted tactical **suit**, not the concept's **baggy
   hoodie + cargo pants + utility pouches + chest straps + knee pads + gloves + boots**.
   The shell reads clean from the front/back but shows seam/boundary artifacts from the
   side (it self-intersects at concave areas) and the accent zones are segment-aligned,
   not garment-shaped. Detailed tactical gear is a dedicated **modelling** task (not a
   recolour) — the three realistic options are: (a) accept the stylized suit for now and
   move to gameplay integration; (b) model/weight real garment geometry onto the rig;
   (c) source a CC0 base that already ships clothed (likely needs retargeting, since the
   current base was chosen precisely because it shares the animation skeleton). **This is
   the decision point to confirm before more outfit work.**
2. **Rifle aim/fire/reload:** only one-handed pistol motions exist for free. Plan:
   derive two-handed rifle aim/fire/reload by scripting the left (support) arm onto the
   SMG foregrip over the pistol timing. Mixamo was excluded (requires an Adobe login).
3. **Weapon grip alignment:** the SMG is attached to `hand_r` but the grip offset is
   rough and the left hand does not yet grip the foregrip.
4. **Accessories not yet modelled:** male — none extra; female — orange goggles/headband.
5. **Boots/feet:** feet are currently bare-ish (dark) — boots are not yet modelled.
6. **Godot AnimationTree:** clips are present on an AnimationPlayer; a blend-tree /
   state machine (locomotion blend + upper-body aim/fire/reload layer) is not yet built.

## Reproduce
Download the three CC0 zips (CREDITS.txt), extract, then:
```
blender --background --factory-startup --python pipeline/dress_character.py -- \
  <base>/Superhero_Male_FullBody.gltf <ual>/UAL1_Standard.glb <hair>/Hair_SimpleParted.gltf \
  <out>/male_dressed.glb 0.0 0.55 0.52
```
(female: Superhero_Female + Hair_Long + accent 0.92 0.42 0.06). Then `godot --import`.
