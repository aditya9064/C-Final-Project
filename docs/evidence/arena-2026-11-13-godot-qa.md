# Evidence: Godot arena environment QA (feature/godot-arena)

- **Date:** 2026-11-13
- **Run by:** Koushik Cherukuri
- **Platform:** Windows 11 Enterprise, Intel integrated GPU, Godot 4.7.2 stable (Forward+/Vulkan), Blender 4.2.23 LTS
- **Scope:** the standalone Godot arena in `godot/` plus a build/test of the unchanged C++ `develop` tree on this branch.
  Full package with screenshots, per-view cover logs and benchmark logs:
  `godot/qa/arena_final_candidate/`.

## 1. C++ build and unit tests (`develop` tree; this branch adds no C++ changes)
Toolchain: CLion-bundled CMake + Ninja + MinGW GCC (`c++.exe`), Release.
```
cmake -S . -B <build> -G Ninja -DCMAKE_BUILD_TYPE=Release   → exit 0 (doctest cmake_minimum_required deprecation warning only)
cmake --build <build> --parallel                           → exit 0
<build>/tests/fireteam_tests.exe
[doctest] test cases: 27 | 27 passed | 0 failed | 0 skipped
[doctest] assertions: 88 | 88 passed | 0 failed |
[doctest] Status: SUCCESS!
```
Note: the first `ctest` invocation reported `0xc0000139` (entry point not found) because
Git for Windows' `/mingw64/bin/libstdc++-6.dll` was ahead of the toolchain's DLL on
`PATH`; with the MinGW `bin` directory first the same binary passes. Environment issue,
not code. CI builds with MSVC/Apple clang/GCC and is unaffected.

## 2. Godot asset import from a wiped cache (fresh-clone equivalent)
```
rm -rf godot/.godot
godot --headless --path godot --import        → exit 0, 0 errors
```
All generated GLBs, the 16 nature glTF models (+ 9 shared textures), the PBR textures and the
HDRI import; no stray files are produced.

Shared-texture check (2026-11-14): the PNGs Blender writes to `nature/textures/` are MD5-identical
to the ones Godot extracted from the former GLBs; with `mipmaps/generate=true` in their `.import`
files the `trees` view renders pixel-identical (mean abs diff 0.000/255) and `overview`,
`landscape`, `under_canopy` differ only by Godot's run-to-run noise (≈0.2–0.7/255, the same
as two GLB runs). Interleaved benchmarks of both versions show no regression.

## 3. Arena renders + automated cover raycast tests
```
godot --path godot --rendering-driver vulkan --resolution 1600x900 --quit-after 420 -- --view overview
GRAPHICS_PRESET: high
COVER_TEST [PASS] stand behind 1.28 low cover -> exposed (blocked=false expected=false)
COVER_TEST [PASS] crouch behind 1.28 low cover -> concealed (blocked=true expected=true)
COVER_TEST [PASS] stand behind 2.10 tall cover -> concealed (blocked=true expected=true)
COVER_TEST [PASS] stand head behind 2.10 tall cover -> concealed (blocked=true expected=true)
COVER_TEST [PASS] bullet into low cover cannot pass through (blocked=true expected=true)
ARENA_CAPTURE view=overview size=(1600, 900)
```
Full set: 17 cameras captured by `godot/qa/capture_views.sh`, cover 5/5 on every run,
0 script/runtime errors (`godot/qa/arena_final_candidate/logs/render_log.txt`,
`logs/cover_<view>.txt`).

## 4. Performance (`godot/qa/bench_views.sh`, 300 timed frames, uncapped, run alone)
| Preset | teal_spawn | under_canopy | canopy_ground | trees | walls | worst frame | frames > 33 ms |
|---|---|---|---|---|---|---|---|
| performance | 127.8 | 117.3 | 87.7 | 148.3 | 102.2 | 17.3 ms | 0 |
| high | 38.9 | 38.1 | 36.6 | 37.1 | 39.2 | 52.5 ms | 63 total |

## 5. Asset pipeline repeatability
`gen_assets.py` ×2, `gen_vegetation.py`, `gen_terrain.py` re-run; MD5 of 22 protected GLBs
identical before/after; `godot --import` 0 errors; scene renders 17/17.
Log: `godot/qa/arena_final_candidate/logs/regen_test.txt`.

## 6. Not covered
Rendering cannot run in GitHub Actions (no GPU); these results are from the owner's
machine. The cover tests probe five rays around two barriers, not every sightline.
