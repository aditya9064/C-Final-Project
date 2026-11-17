# Characters & Weapons Phase — Status (2026-11-16, pass #3)

Autonomous work within an agreed bar. Budget **$0** — only free CC0 assets + free tools
(Blender 4.2, Godot 4.7). Arena, lighting, C++/multiplayer untouched. Branch
`feature/characters`, **local commits only — nothing pushed/merged**.

> **VISUAL APPROVAL GRANTED by the user, 2026-11-16** (at the 75–85% bar). Still **not pushed or
> merged** (needs separate approval) and **gameplay integration not started** (awaiting the user's
> go-ahead).

> **Quality target (user, 2026-11-16):** ~**75–85% resemblance** to the reference is enough —
> clean, attractive, recognizable, game-ready; not a perfect match. Small differences in
> hairstyle, fabric folds, accessories and facial features are acceptable.
>
> **Primary spec:** `art/references/fireteam_character_reference.png` (gitignored; preserved via the
> side-by-sides in `docs/evidence/char-ref-compare/`).

## What changed this pass (#3) — the face fix + sleeve/brightness corrections
The headline bug was the **dark blue-grey faces**. Root cause (confirmed by introspection, not
guessed): the base body's head vertex group is named **`Head` (capital H)**, but the skin selector
in `process_body()` tested `startswith("head")` — so **every head polygon failed the skin test and
fell through to the dark "tee" material** (0.075, 0.078, 0.088 ≈ blue-grey). The face was never skin
at all. Fixes:
- **Case-insensitive skin selector** (`vdom[v].lower()`), so the head routes to the skin material.
- **Fair/light skin material.** The only shipped base texture is the **"Dark" variant**, so we use a
  fresh flat warm tone `(0.72, 0.44, 0.33)` instead of inheriting the base material. The **eyes and
  eyebrows are separate meshes** that survive export, so the face keeps real features (eyes, brows,
  nose, mouth) — a natural fair-skinned face, not blue/grey/metallic/pale.
- **Darker sleeves** (user request): sleeve tubes + shoulder caps now use a charcoal `SLEEVE`
  material `(0.135, 0.14, 0.15)`, so the arms read dark from the shoulder down (raglan) against the
  light-grey torso. (This supersedes the earlier "sleeves are light-grey" note.)
- **Hoodie brightness**: torso albedo lowered `0.44 → 0.34` so under the bright arena sun it lands as
  **light grey, not blown-out white** (verified in-engine).

Everything else from pass #2 is retained: lofted clean-topology garments (no jagged patches), hip
bridge closing the crotch, H-harness + back X, belt/pouches/thigh holster, fingerless-glove cuffs,
knee pads, grey/white hi-top sneakers, team zipper + thigh dash, female pulled-back ponytail + bangs
+ slim goggles. Male = brown tousled hair + teal; female = clearly feminine + orange.

**Weapon** (`pipeline/gen_weapons.py`, unchanged this pass): silver-dominant two-tone receiver, round
red-dot optic, slotted grey handguard, muzzle brake, angled mag + team baseplate, skeleton stock.

**Gun orientation:** `rotation_degrees = (-0.89, -94.88, -78.76)` — barrel **forward and level**,
solved from the `hand_r` pose. Verified across aim/fire/reload. Applied in
`preview.gd`/`duo.gd`/`character_check.gd`.

## Regression (verified in Godot, arena lighting)
All **12 clips** present; **all 8 requested anims tested** — idle, walk, run, jump, aim, fire,
reload, death — garments deform cleanly, gun points forward in aim/fire, female ponytail reads from
side/back, no poke-through. Evidence in `docs/evidence/char-ref-compare/`:
`compare_{male,female}_FINAL.png` (ref beside render, front/side/back), `anim_poses_FINAL.png`
(all clips), `compare_weapon_FINAL.png`, `compare_male_baseline.png` (where it started).

## Assessment against the 75–85% bar
**Meets it, in my judgment.** Faces are natural fair-skinned and distinct (male youthful, female
feminine); outfit, silhouette, team colours, gear and weapon all read clearly against the reference.

Honest residual differences (all within the "acceptable" list the user set):
- Proportions are a touch stockier than the reference's tall lean frame (CC0 base limit).
- Faces are the stylized base geometry (not a bespoke likeness) — now correctly toned and featured.
- The open-collar dark-tee reads as two short bars rather than a clean zip V; minor.
- Female hair is a neat pulled-back ponytail but shorter/simpler than the reference's long sweep.
- Fabric is smooth (no modeled folds); pouches/holster are simple blocks.

No bespoke rifle animation was authored — the current brief omits it. Aim/fire/reload reuse the CC0
pistol-style two-hand stance (support hand near the grip rather than out on a foregrip, stock not
shouldered) with the corrected forward gun orientation. Hi-top sneakers rebuilt as a flat toe-box +
thin sole (the earlier faceted toe-ball is gone); all procedural spheres now smooth-shaded.

## Reproduce
```
blender -b --factory-startup -P pipeline/tailor2.py -- \
  <base>/Superhero_Male_FullBody.gltf <ual>/UAL1_Standard.glb <hair>/Hair_SimpleParted.gltf \
  <out>/male_tailored.glb 0.015 0.52 0.50 m      # female: Superhero_Female + 0.92 0.42 0.06 + f
blender -b --factory-startup -P pipeline/gen_weapons.py -- assets/models
```
Previews: Godot `scenes/preview.tscn` → `previews/*.png`; sheets via `pipeline/compose.py` vs
`ref_crops/`. Do not integrate gameplay until the appearance is approved.
