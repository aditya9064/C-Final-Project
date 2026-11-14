# Rendering architecture: SFML executable, Godot arena, and how they relate

**Author:** Koushik Cherukuri (rendering owner). **Status date:** 2026-11-13.

This document explains what rendering-related code exists in the repository today,
which parts actually build and run, how they relate to each other, and what the team
must decide before any of it becomes "the renderer" of Fireteam: Skirmish. It is
deliberately conservative: nothing below claims an integration that has not been
built and tested.

## 1. What exists right now

| Component | Location | Technology | Builds / runs today? | Relationship to the game |
|---|---|---|---|---|
| **Course C++ game** (`fireteam_skirmish` + `fireteam_core` + `fireteam_tests`) | `src/`, `include/fireteam/`, `tests/`, `CMakeLists.txt` | C++20, CMake, SFML 2.6.2 (window/2D), ENet 1.3.18, doctest | **Yes.** CMake configure + build + 27 doctest cases pass (see §5). | The graded deliverable. `main.cpp` opens an SFML window with the input → update → render loop (issue #5) and draws a placeholder; `fireteam_core` holds Aditya's engine-agnostic `Vector2`, `Entity`, `GameWorld`, `GameState`. |
| **Godot 3D arena environment** (this PR) | `godot/` | Godot 4.7 (Forward+/Vulkan), GDScript, Blender 4.2 asset pipeline | **Yes**, as a standalone Godot project. Renders, has collision, passes 5 automated cover raycast tests, benchmarked. | **Not connected to the C++ executable.** It is a complete, reviewable 3D environment; no gameplay, networking or C++ runs inside it yet. |
| raylib first-person prototype | *not in this branch* (kept locally, see §4) | C++17, raylib 5.5 | Built locally earlier; excluded from this PR. | A throw-away look-development prototype that preceded the Godot arena. |
| Unreal Engine 5 project skeleton | *not in this branch* (kept locally) | UE5 C++ | **Never compiled** (no UE5 toolchain available). | Superseded by Godot; excluded from this PR. |

The one-sentence summary: **the C++/SFML program and the Godot arena are two separate
programs that do not call each other.** The proposal, README and CI describe the SFML
program; the Godot folder is an environment built to the agreed arena spec and the
concept-art references, waiting on an integration decision.

## 2. Why Godot and Blender were used for the arena

The proposal chose SFML because it is a gentle first graphics library. SFML, however,
is a **2D** library: it has no 3D camera, meshes, lighting, shadows or 3D physics. The
arena the team designed (octagonal walled arena, central tensioned canopy, 2.10 m / 1.28 m
cover whose whole point is *standing vs crouching* line-of-sight) is inherently 3D, so a
2D renderer cannot represent its core gameplay idea.

Godot 4 was chosen for the environment because:

- It is free and MIT-licensed, runs on Windows/macOS/Linux, and the Forward+ renderer
  gives PBR materials, real-time shadows, SSAO/SSIL and HDRI skies out of the box.
- It has a built-in 3D physics server. The five cover tests in `build_arena.gd` are
  real `PhysicsDirectSpaceState3D.intersect_ray()` calls — the exact primitive a hitscan
  weapon needs — so "does 1.28 m cover hide a crouching player?" is *measured*, not assumed.
- It supports C++ through **GDExtension** (`godot-cpp`), so the course requirement that
  gameplay be written in C++20 can still be met if the team adopts it (§3).
- It can run headless (`--headless --import`) and from the command line with arguments,
  which made automated screenshots, benchmarks and tests possible.

Blender 4.2 LTS (headless Python) was used to **generate** the prop meshes (barriers,
crates, barrels, sandbags, ivy, terrain) and to convert the CC0 vegetation kit to
Godot-ready glTF files with shared textures. Generating assets from scripts instead of hand-modelling means the
whole asset set is reproducible from `godot/pipeline/*.py` (verified: regeneration produces
byte-identical GLBs), and every asset's licence is known (all CC0 or project-authored).

## 3. What must change if Godot becomes the final renderer

This is the decision the team (and ideally Dr. Ukegbu, since the proposal says SFML)
needs to make. If **Godot is adopted**, the following changes are required. None of them
have been started.

### 3.1 Build system
- Add `godot-cpp` (the Godot 4.7 branch) as a submodule or `FetchContent` dependency and
  build the gameplay as a **GDExtension shared library** (`libfireteam.<platform>.<target>`)
  plus a `godot/fireteam.gdextension` descriptor. godot-cpp ships a CMake build, so this
  can stay inside the existing `CMakeLists.txt` as a new target that links `fireteam_core`.
- The SFML dependency and the `fireteam_skirmish` smoke-test executable would be
  retired (or kept temporarily as the "no-Godot" build). ENet stays — see 3.3.
- CI: download the Godot 4.7 headless binary in the workflow, run
  `godot --headless --path godot --import` (asset import check) and a headless scene run
  that fails the job on any `COVER_TEST [FAIL]` or `SCRIPT ERROR`. Rendering itself cannot
  run on GitHub runners (no GPU), so screenshot QA stays a local, documented step.

### 3.2 Gameplay code (`fireteam_core`)
- `Vector2`, `Entity`, `GameWorld`, `GameState` are engine-agnostic and **do not need to
  change** to be unit-tested; they would be *driven* from Godot instead of from SFML. The
  natural mapping is `GameWorld` owned by a `Node3D` extension class whose
  `_physics_process(delta)` calls `GameWorld::update(delta)`.
- Positions: the simulation can keep `Vector2` as the ground-plane (X, Z) position; a
  thin adapter maps it to each entity's `Node3D.position` (Y comes from the floor, 0.15 m
  slab top) for rendering. Collision and hitscan, however, must be **3D** — walls, cover
  heights and crouching only exist in 3D. The cleanest way to keep `fireteam_core` testable
  is an abstract `IRaycaster { virtual bool blocked(Vector3 from, Vector3 to) = 0; }`
  implemented once over Godot physics and once as a fake for doctest.
- Input: Godot's `Input` singleton replaces `sf::Event` polling in `Player::handleInput`.
- Rendering: `Entity::render()` was already removed from the design on `develop` ("the
  renderer will read entity data"), which fits Godot exactly — nodes read entity state.

### 3.3 Networking
- Aditya's `NetworkManager` (RAII wrapper over raw ENet, PR #17) works unchanged inside
  a GDExtension; ENet is plain C and does not care which window system is present.
  Alternatively Godot's `ENetMultiplayerPeer` + RPCs could be used, but that would
  replace C++ networking code with engine features and weakens the course deliverable.
  **Recommendation: keep the C++ ENet `NetworkManager`.**

### 3.4 Tests and course requirements
- The doctest suite keeps running exactly as now (it never needed a window).
- Add a Godot headless test target that runs the arena scene and the cover tests.
- Course compliance: the proposal's tool list (section 4) and the README tech-stack
  table must be updated and the change explained in the final report; the instructor
  should confirm that a C++ GDExtension satisfies "written in C++".

### 3.5 Alternative: keep SFML as the graded program
If the team decides **not** to adopt Godot, nothing in this PR blocks that: the Godot
folder is self-contained, touches no C++ source, and can be presented as the 3D
environment / visual target while the SFML program remains the deliverable. In that case
the arena geometry (octagon vertices, cover positions and heights in
`godot/scripts/build_arena.gd`) is still the authoritative map layout to port to 2D.

## 4. About the raylib and Unreal folders

A raylib C++ prototype (`prototype/`) and an Unreal 5 project skeleton (`unreal/`) exist
on the rendering owner's machine from earlier exploration, together with a local,
**uncommitted** `CMakeLists.txt` change that added a raylib `arena` target. They are intentionally **not part
of this PR**: the raylib target would add a fourth `FetchContent` dependency to every CI
build (raylib needs extra X11 packages on Linux), and the Unreal code has never been
compiled. The CMake change is preserved as a git stash + patch locally; if the team wants
the raylib prototype in the repo it should come as its own small PR.

## 5. What has been verified (and how)

| Check | Command | Result (2026-11-13, Windows 11, Intel iGPU) |
|---|---|---|
| C++ configure + build (`develop` tree, unchanged by this branch) | CLion-bundled CMake 3.x + Ninja + MinGW GCC | configure OK, build OK (only the known doctest `cmake_minimum_required` deprecation warning) |
| C++ unit tests | `fireteam_tests` | 27 test cases, 88 assertions, all pass |
| Godot asset import from a wiped cache | `godot --headless --path godot --import` | 0 errors; all 16 generated GLBs, 16 converted glTF models with 9 shared textures, 11 PBR textures and the HDRI import |
| Arena renders + cover tests | `godot --path godot --rendering-driver vulkan --resolution 1600x900 --quit-after 420 -- --view overview` | screenshot written, `COVER_TEST [PASS]` ×5, 0 script errors |
| Full QA capture (17 cameras) | `bash godot/qa/capture_views.sh <godot>` | 17/17 rendered, cover 5/5 on every run |
| Benchmarks | `bash godot/qa/bench_views.sh <godot>` | Performance preset 88–148 FPS (0 frames > 33 ms); High preset 37–39 FPS |
| Asset pipeline repeatability | run all generators twice, compare MD5 | 22/22 protected GLBs byte-identical |

Evidence package: `godot/qa/arena_final_candidate/` (reports, screenshots, logs) and
`docs/evidence/arena-2026-11-13-godot-qa.md`.

## 6. Not done / open items

- No gameplay, player movement, weapons, hit detection, AI, networking or HUD exists in
  the Godot project; the mannequins are primitive placeholders.
- No GDExtension or any C++ runs inside Godot. `godot/cpp/README.md` holds the class
  mapping plan only.
- The decision in §3 is open. Until it is made, the Godot arena is an environment
  prototype, and the README/proposal continue to describe SFML as the renderer.
- On `feature/rendering` (not this branch) the SFML `main.cpp` loop from #5 runs until the
  window is closed, while the CI workflow executes `fireteam_skirmish` as a smoke test; on a
  headless runner that process never exits. A one-frame `FIRETEAM_SMOKE_TEST` exit has been
  prepared and validated for that branch's own PR.
- The concept-art reference images are not redistributed (rights unconfirmed); see
  `art/references/README.md`.
