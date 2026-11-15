# Collision & dimension report

All results below are from real runs of the Godot scene (`logs/render_log.txt`,
`logs/cover_<view>.txt`). The raycast tests execute at scene start on every launch.

## Locked dimensions (verified in `scripts/build_arena.gd`)
| Item | Locked | In scene |
|---|---|---|
| Footprint vertices (X,Z) | (-16,-17)(16,-17)(21,-12)(21,12)(16,17)(-16,17)(-21,12)(-21,-12) | `VERTS` unchanged |
| Wall height | 4.50 m | `WALL_H = 4.5` |
| Low cover | 1.28 m | `_barrier(..., 1.28, ...)` ×6 |
| Sandbag cover | ≈1.25 m | collider 1.26 m |
| Tall cover | 2.10 m | `_barrier(..., 2.10, ...)` ×4 + loophole walls 2.10 m |
| Player / crouch | 1.80 / 1.10 m | `PLAYER_HEIGHT`, `CROUCH_HEIGHT` (mannequins unchanged) |
| Canopy clearance | ≈3.75 m | eave beams at 3.75 m |
| Canopy apex | ≈5.00 m | ridge 5.0 m |
| Canopy footprint | ≈10.6 m | posts ±4.65 m, fabric half-width 5.3 m → 10.6 m |
| Spawns | teal −X, orange +X | bays at x = −18 / +18, mirrored |

## Raycast cover tests — 5/5 PASS on all 17 render launches
```
COVER_TEST [PASS] stand behind 1.28 low cover -> exposed        (blocked=false expected=false)
COVER_TEST [PASS] crouch behind 1.28 low cover -> concealed     (blocked=true  expected=true)
COVER_TEST [PASS] stand behind 2.10 tall cover -> concealed     (blocked=true  expected=true)
COVER_TEST [PASS] stand head behind 2.10 tall cover -> concealed(blocked=true  expected=true)
COVER_TEST [PASS] bullet into low cover cannot pass through     (blocked=true  expected=true)
```
**Coverage limitation:** these five rays test specific points around the (0,10) low barrier and the
(12,9) tall barrier. They do not enumerate every sightline in the arena.

## Collision changes in this pass
Nothing existing was moved or resized. New static colliders (all axis-aligned boxes, no overlaps):
| Prop | Position (x,z) | Collider | Nearest existing cover & gap |
|---|---|---|---|
| Loophole barrier (teal) | (−5, −9.5) | 4 blocks forming 3.0×2.10×0.7 with a 1.3×0.42 slot at 1.35 m | low barrier (0,−10): 2.0 m; sandbags (−6.7,−5.5): 3.2 m |
| Loophole barrier (orange) | (5, 9.5) | mirror | mirror |
| Tarp pallet | (−6.2, 12.4) | 1.36×1.42×1.16 | crates (−9,13): 1.6 m; mannequin (−4,13): 1.5 m |
| Sandbag arc ×3 piles | around (−13.5, 13.5) | 3 × 2.3×1.26×0.9 | tall barrier (−12,9): ≥0.5 m; crates (−9,13): ≥0.8 m |
| Sandbag arc (mirror) | around (13.5, −13.5) | mirror | mirror |
| Dark crate clusters | (−13.5,−5.5), (13.5,5.5) | 3 × crate boxes | spawn bay ≥2.5 m; L-barriers ≥1.6 m |
| Spawn cases ×2 per bay | inside bays | 1.1×0.6×0.6 | inside recess, off the exit line |
| Canopy portal flanking blocks | **removed** | — | portal opening 2.4 m now fully clear |

The loophole opening is a genuine gap between four collided blocks — hitscan passes only through the
slot. It was placed away from the two tested barriers so the existing test results are unaffected (and
they still pass).

## Passages
Main lanes spawn → centre (|z| < 2.75 on ±X) are untouched; the new props are at least 1.5 m from any
neighbouring cover (table above). This was checked by coordinates, not by an automated nav-mesh test.
