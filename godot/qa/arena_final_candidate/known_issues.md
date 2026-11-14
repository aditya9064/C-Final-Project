# Known remaining issues (honest list)

## Visual — differs from the references
1. **Stylized vs painterly:** trees, bushes and rocks are clean CC0 stylized assets; the concept art is
   painterly with softer, slightly darker/more varied greens. Leaf colour is a touch brighter and more
   uniform than the refs.
2. **Canopy fabric** has correct sag, beams and colour but no seam/fold texture or stitching; the eave
   frame reads heavier than the thin tensioned edge in the refs.
3. **Props are low-detail Blender meshes:** olive cases have no latches/rivets, the tarp pallet is a
   boxy approximation (flat flaps), barrels have no rims/bungs, dark crates get their X-bracing from
   thin bars rather than modelled panels.
4. **Portal** is pillars + stepped lintel; the refs show a deeper reveal with inner edge detail.
5. **Loophole barrier** edges are sharp (built from boxes) — the beveled profile of the other barriers
   is missing there.
6. **Floor** has slab joints, ring and arc, but no crack/discolouration texture or leaf litter; grass
   tufts are simple blades.
7. **Exterior horizon** is a plain pale band — no distant wooded ridges/mountains as in ref_03.
8. **Ivy corner clusters** are smaller than the refs' heavy corner drapes; wall light strips read small
   from a distance.
9. **One boulder** at the SW slab corner partially intersects the concrete apron (visible in
   `landscape.jpg`).
10. Placeholder mannequins remain (by instruction) and look like blockouts next to the finished props.

## Technical
- **High preset is 37–39 FPS** on this iGPU (Performance preset meets the ≥60 target at 88–148 FPS).
- Cover raycast tests cover five specific lines around two barriers, not all sightlines.
- Passage clearance was checked by coordinates, not an automated nav test.
- Terrain colour resolution is the 1.25 m vertex grid (colour bands can show at grazing angles).
- `canopy.glb` and `tree_a/b/c.glb`/`shrub.glb` are still generated but unused (kept so the generators
  stay deterministic; harmless).
- Tree LODs rely on Godot's importer-generated mesh LODs; no hand-authored impostors.
