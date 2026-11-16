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
  - Realistic **athletic** base bodies (not blocky), ~14k tris, 65-bone humanoid rig.
  - Brown hair; grey/charcoal tactical suit with team-colour (teal/orange) accents and
    a light-grey chest panel.
  - 12 animation clips bundled per character, **no retargeting needed** (base and
    animation library share the identical skeleton).
  - Each SMG attaches to the `hand_r` bone (`BoneAttachment3D`) and moves with the rig.
  - Rendered front/side/back (`duo_front/side/back.png`, `char_*.png`).

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

1. **Outfit fidelity:** current clothing is a *fitted tactical suit* (recoloured body
   shell), not yet the concept's **baggy hoodie + cargo pants + utility pouches +
   chest straps + knee pads + gloves + boots**. Those are added geometry and are the
   main remaining modelling task. The female torso accent is currently patchy and needs
   cleaner material regions.
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
