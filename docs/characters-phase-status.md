# Characters & Weapons Phase — Status (2026-11-16)

Autonomous work. Budget **$0** — only free CC0 assets + free tools (Blender 4.2, Godot 4.7).
Arena, lighting, C++/multiplayer untouched. Branch `feature/characters`, **local commits only —
nothing pushed/merged**. **NOT approved yet; not claiming a match.**

> **Primary spec:** `art/references/fireteam_character_reference.png` (gitignored; preserved via the
> side-by-sides in `docs/evidence/char-ref-compare/`). Judge every change against it.

## What changed this pass (reference rebuild #2)
The previous version painted material regions onto the *reshaped muscular body* (plus a noise
displace), which could never produce clean fabric — hence the jagged white patches the review
flagged. **New pipeline `pipeline/tailor2.py` builds each garment as its own lofted mesh** (rings
along the bone axes, regular topology), so:
- Materials assign by **(ring, segment) index** → clean panels, seams and accent strips, **never
  jagged islands**. The jagged-patch defect is gone.
- The **garment radii set the silhouette**; the hidden muscular body is **slimmed inside the
  garments** (no poke-through) and darkened (no hollow peek). Custom split normals are cleared so
  no muscle shading bleeds onto visible skin.
- Corrections to the spec I had wrong before: **sleeves are light-grey** (not charcoal — only the
  ribbed cuffs are darker), and the **hoodie is open over a dark tee** (a dark front V), not a
  closed zipped bib.

Pieces: lofted hoodie (light-grey body + sleeves, dark-tee collar V, team zipper, ribbed cuffs,
shoulder caps, back hood) · lofted cargo joggers (ankle cuffs, team thigh dash, knee pads) · hip
bridge closing the crotch · H-harness (front straps + chest buckle + back X) · belt, pouches,
thigh holster · fingerless-glove cuffs · grey/white hi-top sneakers. **Female is distinct:** narrower
shoulders, bust/waist shaping, **pulled-back scalp hair + high ponytail + face-framing bangs** (not
the male parted hair), slim goggles, orange accents.

**Weapon** (`pipeline/gen_weapons.py`): silver-dominant two-tone receiver, **round red-dot optic**,
slotted (M-LOK) grey handguard, muzzle brake, angled mag + team baseplate, triangular skeleton
stock + team butt, team receiver dashes.

**Gun orientation (P6):** solved numerically from the `hand_r` pose, not guessed —
`rotation_degrees = (-0.89, -94.88, -78.76)` points the barrel **forward and level** (optic up).
Verified in-engine across aim/fire/reload/run. (The earlier "fixed" `(0,0,0)` was wrong — it pointed
the muzzle across the body; this is the correction.) Applied in `preview.gd`/`duo.gd`/`character_check.gd`.

## Regression (verified in Godot)
All **12 clips** intact with correct loop modes; garments deform cleanly through walk/run/jump/
aim/fire/reload with no poke-through; female ponytail swings. Evidence in
`docs/evidence/char-ref-compare/`: `compare_{male,female}_FINAL.png` (ref beside render, front/side/
back), `compare_weapon_FINAL.png`, `anim_poses_FINAL.png`, `compare_male_baseline.png` (where it started).

## What still differs from the reference — honest, NOT approval-ready
- **Proportions:** the CC0 base is ~6.7 heads and stocky; the reference is ~7.5–8 heads, taller and
  leaner. Garments are leaner now but the frame is still stockier than the sheet.
- **Faces:** the baked CC0 base faces (androgynous), not the sheet's distinct young male / female.
  This is a **hard limit of the base mesh** — procedural work cannot fix it. The female now reads
  distinct by *silhouette and hair*, not by face.
- **Hoodie value** blows a little bright under the game sky (reads near-white vs the ref's light grey).
- **Two-handed grip:** the gun orientation is correct, but the hands still use the CC0 **pistol**
  stances — the support hand sits near the grip, not out on the foregrip, and the stock is not
  shouldered. **No dedicated rifle aim/reload animation has been authored yet** (P6 remainder).
- Fabric is smooth (no modeled folds); accessories (pouches/holster) are simple blocks.

## Recommended next step for the face/proportion ceiling (needs a decision)
Procedural modeling on this base cannot deliver the reference faces or its taller lean frame. The
genuinely-free option worth evaluating is **MakeHuman / MPFB2** (CC0 output) as a *mesh source* for
lean, distinct male/female bodies + faces + hair, then transferring our existing 65-bone rig's
weights onto them (keeping the 12 animations). It needs a one-time **download** (addon + assets) —
a permission gate — and carries weight-transfer risk. Flagged for the user to approve or decline;
not pursued unilaterally.

## Reproduce
```
blender -b --factory-startup -P pipeline/tailor2.py -- \
  <base>/Superhero_Male_FullBody.gltf <ual>/UAL1_Standard.glb <hair>/Hair_SimpleParted.gltf \
  <out>/male_tailored.glb 0.015 0.52 0.50 m      # female: Superhero_Female + 0.92 0.42 0.06 + f
blender -b --factory-startup -P pipeline/gen_weapons.py -- assets/models
```
Harness: `pipeline/ortho_render.py` + `pipeline/compose.py`. Godot: `scenes/{preview,duo,diag}.tscn`.
Do not integrate gameplay until the appearance is approved.
