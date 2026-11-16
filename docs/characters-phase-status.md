# Characters & Weapons Phase — Status (2026-11-15)

Autonomous work. Budget **$0** — only free CC0 assets + free tools (Blender 4.2, Godot 4.7).
The arena, lighting, weapons, and C++/multiplayer code were **not modified**. All work is on
branch `feature/characters` (local commits only; nothing pushed).

> **No character reference image exists.** The eight `art/references/ref_*.jpeg` (and the
> matching WhatsApp images in the school OneDrive) are all *arena* concept art. These
> characters were therefore built to the **written brief** (the user's 10-point spec + the
> established teal/orange team identity + the arena's colour language), **not** matched to a
> character sheet. No claim of matching a reference image is made.

## What the characters are

Two operatives — **male = Teal team**, **female = Orange team** — built by
`pipeline/tailor_character.py` from CC0 Quaternius assets (see
`assets/models/characters/CREDITS.txt`) with **real modelled garment geometry**, not a recolour:

- **Base:** rigged, athletic CC0 body (65-bone humanoid rig), true height **1.86 m**.
- **Hoodie:** body-derived shell (inherits the rig's skin weights), seams welded before
  offsetting, smoothed, given fabric thickness (Solidify) + a flared baggy hem + a **hood**
  draped on the back.
- **Cargo pants:** body-derived, baggy, ankle cuffs.
- **Tactical gear (modelled):** chest rig (horizontal webbing band + light-grey plate panel +
  over-the-shoulder straps), **belt** + two hip pouches + a thigh pouch, **knee pads** (dark
  with a team-colour stripe), **boots** (body-derived, with a team-colour ankle cuff),
  **gloves** (dark), **team-colour shoulder caps**.
- **Hair:** brown, parted; **female** adds a **ponytail** (down the back) and **goggles**
  (team-colour lenses resting on the forehead).
- **Weights:** body-derived pieces keep the body's weights; new geometry (hood, chest band,
  straps) is weighted by **Data Transfer** from the body; rigid pieces (pouches, pads, belt,
  goggles, ponytail, boot cuffs) are bound to a single bone. **Verified**: garments deform
  correctly through walk / run / jump / aim / fire / reload with no skin poke-through and the
  SMG stays seated on the `hand_r` bone.
- **Animations:** 12 clips bundled, clean names, cyclic ones loop (idle/walk/run/jog/aim_idle/
  jump); one-shots (aim/fire/reload/death/jump_start/jump_land) don't. No retargeting (shared
  skeleton).

Review renders: `godot/previews/` — `{m,f}_{idle_front,idle_side,idle_back,walk_side,run_side,
jump_side,death_side,reload_front,aim_front,fire_front}.png` plus `duo_{front,side,back}.png`.

## Honest limitations (what this is NOT)

This is a strong **stylised** realisation of the brief, built procedurally at $0 — not AAA /
photoreal, and not a perfect match to any sheet:

- **Fabric is smooth** — no modelled wrinkles/folds; the hoodie reads as a fitted-to-slightly-
  baggy jacket rather than a heavy draped hoodie.
- **Boots** are the body foot shape darkened + a cuff (toes still read); not a separately
  modelled boot last with sole tread.
- **Gloves are full**, not fingerless.
- **Cargo "pockets/seams"** are represented by the pouches, not modelled stitched pockets.
- **Proportions** are stylised and a little bulky (the body base is muscular; garment offset
  adds volume).
- Minor: goggles sit just above the brow; a couple of rigid pieces (belt, thigh pouch) sit
  slightly proud of the body in some poses.

## Reproduce
Download the three CC0 zips (CREDITS.txt), extract, then per character:
```
blender --background --factory-startup --python pipeline/tailor_character.py -- \
  <base>/Superhero_Male_FullBody.gltf <ual>/UAL1_Standard.glb <hair>/Hair_SimpleParted.gltf \
  <out>/male_tailored.glb 0.0 0.55 0.52 m
# female: Superhero_Female + accent 0.92 0.42 0.06 + sex flag f
```
Then `godot --import`. Preview harness: `scenes/preview.tscn` (`scripts/preview.gd`) renders the
contact sheet; `scenes/duo.tscn` renders both together; `scripts/diag.gd` prints height / gun
size / per-clip loop modes.

## Superseded files (removed)
`male_rigged.glb` (first spike) and `male_dressed.glb` / `female_dressed.glb` (recolour-shell
pass) were replaced by `male_tailored.glb` / `female_tailored.glb`; no scene or script references
them any more.

## Next (gameplay, after appearance sign-off)
AnimationTree (locomotion blend + upper-body aim/fire/reload layer); derive true two-handed
rifle aim/fire/reload (current aim/fire/reload are from CC0 pistol clips); wire to the C++
movement/networking once the look is approved.
