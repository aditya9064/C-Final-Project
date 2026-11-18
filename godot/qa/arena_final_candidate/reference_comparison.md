# Reference comparison — Fireteam: Skirmish arena (final candidate)

References: the team's concept renders `ref_01..08` (8 files, 7 distinct; `ref_02` is byte-identical to `ref_01`). They are **not committed** (redistribution rights unconfirmed); `art/references/README.md` describes each one.
Every reference was opened and inspected at full resolution before and after this pass.
Screenshots: `final/<view>.jpg` (1600×900, Godot 4.7.2 Forward+/Vulkan, real scene). Pass-start
Pass-start baselines for the same cameras are kept in the owner's local archive (not committed).

Severity: **Critical** = defines the reference look / was flagged by the owner; **Major** = clearly
visible from normal viewpoints; **Minor** = detail-level. "Geo" = does the fix touch gameplay geometry.

| # | QA view (`final/`) | Reference(s) & viewpoint | Matches now | Still differs | Severity (before → after) | Corrective action taken | Geo | Status |
|---|---|---|---|---|---|---|---|---|
| 1 | `overview.jpg` | ref_03 (elevated 3/4), ref_01 top-right | Chamfered octagon, warm walls with cap, orange X-canopy centred, teal/orange gateways on ±X, mixed cover, wooded belt, boulders, ivy on tops | Trees are clean stylized (rounder, brighter) than the painterly refs; no distant mountain ridge | Critical → Minor | CC0 broadleaf tree belt (3 depth layers), muted terrain, canopy rebuilt, ivy densified | no | verified |
| 2 | `topdown.jpg` | ref_07, ref_04 (left), ref_01 (right-mid) | Octagon footprint, canopy square inside white ring, **orange arc**, teal/orange floor squares with white inset, curved sandbag walls, tarp pallet, crate clusters, barrel groups, loophole barriers, trees/rocks outside | Reference has slightly more L/Z-shaped barriers; our cover count is the locked layout | Major → Minor | Added orange arc, curved sandbag walls ×2, tarp pallet, loophole barriers ×2, dark crate clusters ×2 (new props only in open floor) | new colliders added, none moved | verified |
| 3 | `overview_b.jpg` | ref_03 (opposite corner) | Same as #1 from the other diagonal; gateways read from both sides | As #1 | Major → Minor | — | no | verified |
| 4 | `canopy_ground.jpg` | ref_05, ref_08 (eye height toward canopy) | Four dark posts + plates, eave beams, sagging orange fabric with raised centre, diagonal ridge beams, portal (pillars + lintel) visible through the posts, banners, trees over the walls | Fabric has no seam/fold texture; eave frame reads heavier than the refs; no leaf litter on floor | Critical → Minor | Canopy rebuilt procedurally (fabric grid with catenary sag, ridge beams, hub, plates, bolts, eave cable); flanking blocks that hid the portal removed | portal walk-through now fully clear (improvement) | verified |
| 5 | `under_canopy.jpg` | ref_05/06/08 (under-roof) | Rich orange underside, ridge beams with **under-mounted light bars**, hub, banners with white diagonal stripe + hem + pleats, portal behind | Light bars are simple emissive boxes (no fixture detail) | Critical → Minor | As #4 + 4 light bars + banner stripes/pleats | no | verified |
| 6 | `teal_spawn.jpg` | ref_04 (teal inset), ref_06 (inside teal), ref_05 | **Chamfered octagonal frame**, white diagonal stripes on jambs, white top line, recessed concrete back wall with teal/white paint splash, linear overhead light in housing, teal floor with white inset rectangle, olive cases staged | Reference back wall carries faint graffiti/stencils; ours is paint stripes only | Critical → Minor | Frame moved proud of concrete, chamfer fills, jamb stripes, back-wall splash, cases replace abstract spheres | no (bay footprint unchanged) | verified |
| 7 | `orange_spawn.jpg` | ref_04 (right-mid), ref_08 | Mirror of #6 in orange | As #6 | Critical → Minor | As #6 | no | verified |
| 8 | `cover_tall.jpg` | ref_04 (orange-stripe tall barrier), ref_08 | Bare beveled concrete with **one diagonal team stripe**, grip slots, 2.10 m height vs 1.80 m mannequin, dark X-braced crates + olive cases behind, orange gateway | Reference barrier tops are more strongly chamfered | Critical → Minor | Removed solid team band + steel cap; single diagonal stripe per face | no | verified |
| 9 | `cover_low.jpg` | ref_05 (low barriers + sandbags) | 1.28 m low barrier (standing mannequin exposed), sandbags, teal gateway, banners, canopy edge | Low barrier stripe is thinner than ref | Major → Minor | Diagonal stripe; tarp pallet moved out of this sightline | no | verified |
| 10 | `firing_port.jpg` | ref_03 / ref_05 (loophole barrier) | 2.10 m wall with a **real 1.3×0.42 m slot** (sill 1.35 m), sill lip, diagonal stripe; view through the slot | Edges are sharp (box-built, no bevel) | Major (missing) → Minor | New `_barrier_loophole` from four collided blocks (true opening) | new cover piece ×2 in open floor | verified |
| 11 | `cargo.jpg` | ref_03 (tarp pallet), ref_01 (crates/barrels) | Olive equipment cases (deeper olive), tarp-covered pallet on wooden pallet with drooping flaps, olive barrel group, canopy + banners behind | Tarp is blocky (flat flaps), cases lack latches/rivets, barrels lack rims | Major → Minor | Olive re-tint, dark X-brace style, tarp pallet added | tarp collider added | verified |
| 12 | `trees.jpg` | ref_05/08 (trees over wall) | Natural branching trunks, tapered bark with normal map, dense layered crowns, several species, varying heights 6.5–12 m, soft shadows | Leaf colour a touch brighter/more uniform than refs; stylized rather than painterly | Critical → Minor | Quaternius MegaKit CC0 trees replace procedural leaf cards | no | verified |
| 13 | `landscape.jpg` | ref_03 / ref_07 (exterior) | Continuous rolling terrain, muted olive/dry grass with dirt, boulders, bushes, three-depth tree belt, trunks meet the ground on hills | Horizon is a plain pale band (no distant ridges/mountains); exterior ground fairly uniform at distance | Major → Minor | Terrain palette muted + dry-grass drifts, gravel ramp softened, props grounded via terrain-height lookup | no | verified |
| 14 | `walls.jpg` | ref_06/08 (ivy, seams, lights) | Warm beige concrete in sun **and** shade, panel seams, corner column, steel cap, base grime, connected ivy clusters at tops/corners | Ivy corner clusters smaller than refs; light strips small from this angle | Major → Minor | Warm sky-fill, warmer recessed panel, ivy re-textured with natural leaf atlas and densified | no | verified |

## Elements the references define, and their status
- Arena silhouette (chamfered octagon 42×34, 5 m chamfers, 4.5 m walls) — **present, unchanged**.
- Orange tensioned canopy with dark structure, under-lights, banners — **present** (procedural fabric + frame).
- Concrete walk-through portal — **present**, opening now unobstructed.
- Teal/orange recessed gateways with chamfered frames, lights, painted floors — **present**.
- Cover family: low, tall, L, loophole, curved sandbags, crates (olive + dark X-brace), barrels, tarp pallet — **present**.
- Floor: warm slabs, white ring + orange arc — **present**.
- Exterior: trees, rocks, shrubs, grass, ivy — **present**.
- Mood: warm sun, blue sky, soft shadows — **present**.

No similarity percentages are claimed; the judgement above is visual inspection of each pair.
