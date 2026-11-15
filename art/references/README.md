# Arena concept-art references (not committed)

The Godot arena in `../../godot/` was matched against eight concept-art renders of the
intended arena. **Those images are not redistributed in this repository** because their
redistribution rights have not been confirmed; `.gitignore` excludes everything in this
folder except this README. The Godot project does not load them — it builds and runs
without any file here.

## Supplying your own references
Drop images into this folder (any name, e.g. `ref_01.jpeg` …); they stay local and
untracked. The per-view comparison in
`../../godot/qa/arena_final_candidate/reference_comparison.md` refers to them by the
numbers below:

| Reference | What it shows | Arena feature it informed |
|---|---|---|
| `ref_01/02` | top-down + prop sheet: crates, barrels, sandbags, barriers | prop set and layout |
| `ref_03` | elevated three-quarter view; wooded surroundings, loophole barrier, tarp pallet | overall silhouette, exterior trees, `_barrier_loophole`, `_tarp_pallet` |
| `ref_04` | teal/orange gateway insets, striped tall barrier | chamfered team gateways, single diagonal stripe on cover |
| `ref_05` | eye-level toward the canopy; low barriers and sandbags | canopy posts/beams/fabric, cover heights |
| `ref_06` | inside the teal gateway, banners | spawn bay detail, banners |
| `ref_07` | plan view with white ring + orange arc, curved sandbag walls | floor markings, `_sandbag_arc` |
| `ref_08` | under the canopy, orange gateway | light bars, fabric underside |
