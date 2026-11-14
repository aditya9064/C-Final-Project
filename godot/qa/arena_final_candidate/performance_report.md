# Performance report

Machine: owner's laptop, Intel integrated GPU, ~15.5 GB RAM, Windows 11.
Build: Godot 4.7.2 stable, Forward+ (Vulkan), 1600×900 window, vsync disabled, uncapped.
Method: `godot/qa/bench_views.sh` — for each camera, 40 warm-up frames then 300 timed frames;
reports average FPS, worst single frame and the count of frames slower than 33.3 ms.
Raw log: `logs/bench_log.txt`. Run alone (no other GPU work).

## Performance preset (`-- --preset performance`) — target ≥ 60 FPS: **MET**
| Camera | Avg FPS | Worst frame | Frames > 33 ms |
|---|---|---|---|
| teal_spawn (spawn area) | 127.8 | 13.0 ms | 0 |
| under_canopy (centre) | 117.3 | 11.6 ms | 0 |
| canopy_ground (across arena) | 87.7 | 17.3 ms | 0 |
| trees (tree cluster) | 148.3 | 13.5 ms | 0 |
| walls (perimeter) | 102.2 | 14.8 ms | 0 |

Lowest average 87.7 FPS; no frame exceeded 17.3 ms → stable above 60 FPS at every tested viewpoint.

## High preset (default) — reported honestly, below 60
| Camera | Avg FPS | Worst frame | Frames > 33 ms |
|---|---|---|---|
| teal_spawn | 38.9 | 35.8 ms | 5 |
| under_canopy | 38.1 | 52.5 ms | 18 |
| canopy_ground | 36.6 | 40.7 ms | 19 |
| trees | 37.1 | 37.8 ms | 15 |
| walls | 39.2 | 41.1 ms | 6 |

High preset keeps MSAA 4×, SSAO/SSIL, glow and full-distance soft shadows; on this iGPU it sits at
~37–39 FPS. It is the quality tier, not the shipping target.

## Where the cost went / optimisations applied
- ~54 dense alpha-tested trees: mid and far belts do **not** cast shadows (fill-rate saving); far belt
  trimmed 26 → 18. Godot's importer generates mesh LODs for the GLBs automatically.
- Tree materials are shared per species (five GLBs, baked materials); ivy/bush paint uses one shared
  alpha-scissor material.
- Flat paint (stripes, chamfers, arc) is thin geometry with shadows off, not overlapping boxes.
- Performance preset: shadow distance 55 m, hard shadows, MSAA off, SSAO/SSIL/glow off.

Previous baseline (before this pass): Performance ≈ 89, High ≈ 41 (overview camera only).
