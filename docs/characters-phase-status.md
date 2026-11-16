# Characters & Weapons Phase — Status (2026-11-15)

Autonomous work. Budget **$0** — only free CC0 assets + free tools (Blender 4.2, Godot 4.7).
The arena, lighting, and C++/multiplayer code were **not modified**. All work is on
branch `feature/characters` (local commits only; **nothing pushed/merged**).

> **Primary visual reference:** `art/references/fireteam_character_reference.png` — the supplied
> teal-male / orange-female operator turnaround (front/side/back) + two carbines. The models are
> built to match this sheet. Side-by-side ortho comparisons live in `docs/evidence/char-ref-compare/`.
> (The older `ref_*.jpeg` are arena art, unrelated.)

## Approach (salvage, not base-swap)
The CC0 Quaternius Superhero base is **kept** — with its 65-bone rig and the full 12-clip
animation library — because garments are **reshaped to clothing profiles in bone space** (boxy
torso, tube sleeves/legs via `reshape()` in `pipeline/tailor_character.py`), which erases the
muscular anatomy. A skin-offset shell read as a bodysuit; a bone-axis profile reads as clothing.
This needs **zero animation retargeting**. (The only other CC0 body on this rig is a featureless
mannequin, and switching bases would not change the clothed silhouette — see
`memory/character-reference-and-rebuild`.)

## What the characters are (matches the reference)
- **Two-tone zip hoodie:** light-grey torso + charcoal sleeves/hood, **teal/orange centre zipper**
  + raglan shoulder accents, clean hem, waisted (not a barrel).
- **Dark charcoal cargo joggers:** cuffed ankles, team-colour thigh stripe, cargo-pocket flaps.
- **Grey/white hi-top sneakers** with white sole + team accent.
- **H-harness:** two front straps + chest buckle + X across the back.
- **Belt + buckle, hip pouch, thigh holster, plain dark knee pads, fingerless gloves.**
- **Hair/goggles:** male tousled brown (SimpleParted); female high **ponytail** (hand-modelled) +
  slim team-lens **goggles** on the forehead.
- **Carbine** (`pipeline/gen_weapons.py`): fused two-tone receiver (silver upper / dark lower),
  red-dot optic on a top rail, short barrel + muzzle brake, angled magazine w/ team baseplate,
  **triangular skeleton stock** w/ team butt pad, **teal/orange accent dashes** on the receiver.
- **Weapon orientation fixed:** the gun attaches to `hand_r` with zero local rotation so the barrel
  points **forward and level** (the old `(6,92,8)` forced it vertical). Updated in `preview.gd`,
  `duo.gd`, `character_check.gd`.

## Regression (verified)
All **12 clips** present with correct loop modes (idle/aim_idle/jog/jump/run/walk loop; aim/fire/
reload/death/jump_start/jump_land one-shot); height 1.86 m; gun seated on `hand_r`. Garments
deform cleanly through walk/run/jump/aim/fire/reload; ponytail swings; no skin poke-through.
Evidence: `docs/evidence/char-ref-compare/` — `compare_{male,female}_FINAL.png` (ortho vs ref),
`anim_{male,female}.png` (motion + gun orientation), `compare_weapon_FINAL.png`,
`compare_male_baseline.png` (where it started).

## What still differs from the reference (honest)
- **Faces** are the CC0 base faces (androgynous, baked into the mesh) — not the younger male /
  distinct female of the sheet. A hard limit of this base; low-stakes for a first-person game.
- **Torso** still reads a little padded/vest-like vs the relaxed open hoodie; the hoodie is zipped
  (closed), not open over a shirt.
- **Two-handed grip** uses the CC0 **pistol** stances (no rifle anims exist free), so the support
  hand sits near the grip rather than fully out on the foregrip; fine for a compact carbine.
- Sneaker/knee shapes are simple; accent piping is colour-blocked, not modelled stitching.

## Reproduce
```
blender -b --factory-startup -P pipeline/tailor_character.py -- \
  <base>/Superhero_Male_FullBody.gltf <ual>/UAL1_Standard.glb <hair>/Hair_SimpleParted.gltf \
  <out>/male_tailored.glb 0.0 0.55 0.52 m      # female: Superhero_Female + 0.92 0.42 0.06 + f
blender -b --factory-startup -P pipeline/gen_weapons.py -- assets/models
```
Comparison harness: `pipeline/ortho_render.py` (neutral grey ortho) + `pipeline/compose.py`
(PIL side-by-side). Preview/regression: `scenes/preview.tscn`, `scenes/duo.tscn`, `scenes/diag.tscn`.

## Next (after appearance sign-off — NOT started)
AnimationTree (locomotion blend + upper-body aim layer); wire to the C++ movement/networking.
Do not integrate gameplay until the appearance is approved.
