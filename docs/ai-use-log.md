# AI Use Log

Per the course AI-use policy, this log records significant AI tool use
across the project: who used it, where, why, what was kept/changed, how it
was checked, and what was learned. Add an entry per significant use as you
go — it's much harder to reconstruct this at the end.

Format (one entry per use):

```
### [Date] — [Person] — [Tool/Model]
- **Where:** file/function/report section
- **Why:** what problem or question prompted this
- **What I kept/changed:** summary of what was used vs. rewritten
- **How I checked it:** docs consulted, tests run, manual verification
- **What I learned:** the actual takeaway
- **Excerpt:** short prompt/response snippet (optional but encouraged)
```

---

### 2026-11-04 — Aditya Miriyala — GitHub Copilot (Copilot SDK in VS Code)
- **Where:** Project ideation; `docs/proposal.md`; initial repository
  scaffolding (`CMakeLists.txt`, `.github/` templates/CI, `README.md`).
- **Why:** Brainstormed and stress-tested project-scope options against
  the assignment's grading rubric before committing to an idea; drafted the
  proposal document; set up initial repo structure/tooling.
- **What I kept/changed:** Kept the overall project concept (networked
  tactical shooter with AI-filled lobby slots) and proposal structure after
  review; adjusted role assignments and scope split based on team input.
  Wrote and locally built/tested the CMake + SFML + ENet skeleton myself
  rather than trusting it blindly — see "How I checked it."
- **How I checked it:** Ran `cmake -S . -B build`, `cmake --build build`,
  `ctest`, and the smoke-test executable locally before committing, to
  confirm the fetched dependencies (SFML, ENet, doctest) actually configure,
  compile, link, and run rather than just "looking right."
- **What I learned:** ENet's own `CMakeLists.txt` uses directory-scoped
  `include_directories()` instead of `target_include_directories()`, so its
  headers don't automatically propagate to consuming targets in other
  directories — had to add `${enet_SOURCE_DIR}/include` explicitly to our
  targets.

### 2026-11-04 — Aditya Miriyala — GitHub Copilot (Copilot SDK in VS Code)
- **Where:** `include/fireteam/Vector2.hpp`, `src/Vector2.cpp`,
  `include/fireteam/Entity.hpp`, `src/Entity.cpp`, `tests/test_vector2.cpp`,
  `tests/test_entity.cpp`, `fireteam_core` library target in `CMakeLists.txt`
  (issue #2).
- **Why:** Asked Copilot to write the engine foundation with line-by-line
  explanations as a teaching exercise, so I could study the concepts
  (operator overloading, abstract classes / pure virtual functions, virtual
  destructors, deleted copy operations, const-correctness, `unique_ptr`
  ownership) before implementing the dependent classes myself.
- **What I kept/changed:** Kept the implementation after reading every
  line and the explanatory comments. [Update this line if you change
  anything while studying it.]
- **How I checked it:** Built with `cmake --build build`, ran all 19 tests
  via `ctest` / `./build/tests/fireteam_tests` (47 assertions pass), and ran
  the test binary under AddressSanitizer -- see
  `docs/evidence/asan-2026-11-04-entity-vector2.md`.
- **What I learned:** (1) A class with virtual methods needs a virtual
  destructor or `unique_ptr<Entity>` would skip the derived destructor.
  (2) `operator+=` returns `Vector2&` (a reference to `*this`) so it can be
  chained, while `operator+` returns a new value and is `const`. (3) Pure
  virtual (`= 0`) makes the class abstract; `std::is_abstract_v` can verify
  that at compile time in a test. (4) Apple clang's ASan has no
  LeakSanitizer, so leak detection has to run on Linux.
- **Excerpt:** Prompt: "you write it all, line by line explained and ill
  study it and you write the tests as well." Response included the
  explanation that deleting a `Player` through an `Entity*` without a
  virtual destructor "would only run ~Entity() and skip ~Player(), leaking
  whatever Player owns."

### 2026-11-13 — Aditya Miriyala — GitHub Copilot (Copilot SDK in VS Code)
- **Where:** `docs/proposal.md`, `README.md`, CMake/rendering smoke test, and CI.
- **Why:** The team chose a 3D first-person direction and the professor
  approved the core-scope change. The project plan and rendering dependency
  needed to reflect that decision without expanding the required game beyond
  a two-player LAN arena.
- **What I kept/changed:** Kept the team's 3D FPS decision and narrowed the
  core to one arena, first-person controls, one hitscan rifle, and a
  two-player ENet match. Replaced the SFML rendering plan with raylib; bots,
  grenades, and 5v5 remain stretch goals.
- **How I checked it:** Verified the raylib 5.5 release tag. Ran
  `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`,
  `cmake --build build --parallel 4`,
  `ctest --test-dir build --output-on-failure`, and
  `./build/fireteam_skirmish`; the build succeeded, all tests passed, and the
  3D renderer smoke test opened, rendered, and closed successfully. Updated
  `docs/proposal.pdf` from the revised Markdown source and confirmed it is a
  readable three-page PDF. GitHub CI has not yet run for this branch.
- **What I learned:** The pivot affects more than rendering: gameplay
  positions and hit detection must become 3D, while rendering should remain
  separate from gameplay so logic can still be tested without a window.

### 2026-11-13 — Aditya Miriyala — GitHub Copilot (Copilot SDK in VS Code)
- **Where:** `include/fireteam/Vector3.hpp`, `src/Vector3.cpp`,
  `include/fireteam/Entity.hpp`, `src/Entity.cpp`, and vector/entity tests.
- **Why:** The FPS pivot requires 3D positions, movement, and direction math
  for gameplay and hit detection.
- **What I kept/changed:** Added a tested `Vector3` value type with arithmetic,
  dot/cross products, length, normalization, and distance; migrated `Entity`
  and its tests from `Vector2` to `Vector3`. Kept `Vector2` available for
  future 2D UI use.
- **How I checked it:** `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`,
  `cmake --build build --parallel 4`, and
  `ctest --test-dir build --output-on-failure` passed.
- **What I learned:** A 3D cross product gives a perpendicular direction,
  which will be useful for camera-relative movement, while dot products
  support aiming and hit-direction calculations.

### 2026-11-13 — Aditya Miriyala — GitHub Copilot (Copilot SDK in VS Code)
- **Where:** `include/fireteam/Player.hpp`, `src/Player.cpp`,
  `include/fireteam/NetworkProtocol.hpp`, `src/NetworkProtocol.cpp`, tests,
  CMake, and README.
- **Why:** The FPS needs a defined, validated wire format for client controls
  and host-authoritative player/match state, plus a gameplay entity that can
  consume those controls without depending on the renderer.
- **What I kept/changed:** Added `Player`, with validated frame-rate-independent
  movement, aim updates, and a fire-request result for later combat handling.
  Its `PlayerInput` is shared with the protocol. Added versioned little-endian
  packet encoding for movement/aim/fire input and fixed two-player snapshots.
  Validation rejects malformed headers, invalid ranges, non-finite world
  values, impossible health, duplicate player IDs, and invalid empty slots.
  The ENet transport remains in approved PR #17 and is not duplicated here.
- **How I checked it:** `cmake --build build --parallel 4`,
  `ctest --test-dir build --output-on-failure`, and
  `./build/tests/fireteam_tests` passed: 46 test cases and 166 assertions.
  `./build/fireteam_skirmish` initialized ENet, rendered the raylib smoke
  scene, and exited successfully; this is not a playable game loop.
- **What I learned:** ENet moves packet bytes but does not define application
  meaning; a versioned, explicitly encoded protocol is needed so peers can
  reject invalid data before it reaches gameplay. The shared input type also
  avoids separate movement rules for local and future networked controls.

<!-- Add further entries above this line as the project progresses. -->

### 2026-11-18 - First-person HUD adaptation - GitHub Copilot
- **Why:** The user asked to adapt useful work from the open teammate PRs
  while preserving first-person multiplayer and the newer player models.
- **Generated changes:** Copilot adapted Koushik's HUD layout from PR #20 to
  the existing C++ session snapshot. It displays the team, health, phase,
  winner, and crosshair without inventing ammo, scores, or timers.
  Learner and teammate review have not yet been confirmed.
- **Validation:** Godot integration tests exercised host and client HUD state,
  elimination, result, and multiplayer loopback; a windowed capture was
  inspected. PR #20 contains additional work that is not integrated here.

### 2026-11-18 - First-person animation integration - GitHub Copilot
- **Why:** The user supplied a rigged arms-and-weapon GLB and chose to use it
  for the first-person viewmodel, confirming permission to use and share it.
- **Generated changes:** Copilot imported the unchanged model, positioned it
  in front of the camera, and connected draw, visual fire, and visual reload
  animations. The local viewmodel replaces the team-colored static weapons;
  the soldiers remain static. The asset's source and license metadata are
  attributed in `godot/cpp/README.md`. Learner review has not been confirmed.
- **Validation:** The Godot integration tests exercise both local player
  views, animation triggers and skeletal movement, and multiplayer loopback.
  A windowed capture was inspected. Combat and ammunition are not implemented,
  and cross-platform performance and two-computer LAN play remain unverified.

### User-supplied character integration - GitHub Copilot
- **Why:** The user requested their supplied teal/orange soldier and weapon
  GLBs in the existing first-person multiplayer prototype.
- **Generated changes:** Copilot integrated the supplied static models,
  kept collision capsules and multiplayer behavior, adjusted the models'
  orientation and first-person weapon placement, and generated missing mesh
  normals and tangents during Godot import. The source GLBs were not edited.
  Learner review and understanding have not yet been confirmed.
- **Validation:** The Godot process test runner passed its asset import,
  native-session, game-scene, and two-process loopback checks. Windowed
  captures were inspected for weapon placement and both soldier orientations.
  This does not establish cross-platform performance or two-computer LAN behavior.
- **Limitations:** The assets have no skeleton or animation, and there are no
  first-person hands. Fire is still an integration hook, not working combat.

### 2026-11-14 - C++ Godot engine/network integration - GitHub Copilot
- **Scope:** The user selected Godot, confirmed instructor approval for C++
  GDExtension, and requested only their engine/networking work. Teammate
  weapons, final characters, and arena visual design remain separate.
- **Generated changes:** Imported the reviewed ENet transport from PR #17
  and Koushik's Godot arena/assets from PR #18. Added `MatchSession` for
  authoritative two-player inputs/snapshots, IDs, timeouts, disconnect,
  damage/result, and rematch. Added the C++ `FireteamSession` bridge, Godot
  collision callbacks, first-person input/camera and host/join UI, integration
  tests, build configuration, and CI. The arena builder is unchanged.
  Learner review and understanding have not yet been confirmed.
- **Validation:** `cmake --build build-godot --parallel 4` passed;
  `ctest --test-dir build-godot --output-on-failure` and
  `./build-godot/tests/fireteam_tests` passed: 69 cases, 320 assertions.
  `python3 tests/test_godot_processes.py
  /tmp/fireteam-godot-4.7/Godot.app/Contents/MacOS/Godot` passed import,
  native-session tests, real-arena keyboard/mouse/camera tests, and two
  independent process replication/result/rematch/disconnect tests.
  Windowed `game_smoke.gd` also passed on Apple M4/Vulkan; a non-fatal
  MoltenVK pipeline-cache warning remains.
- **Memory validation:** Core-only Debug build with
  `-fsanitize=address,undefined -fno-omit-frame-pointer` passed all 69 cases /
  320 assertions using `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`.
  LeakSanitizer is disabled locally because Apple Clang does not support it;
  Linux CI retains ASan/LSan. Updated remote CI has not run on this work yet.
- **Fixes during validation:** Added required OS binding to the trimmed
  godot-cpp profile; enabled exception support so core validation can be
  logged at the Godot boundary. Fixed fresh spawn/body synchronization,
  explicit input-capture/focus state, and automatic menu restoration after
  disconnect. The test runner rejects Godot script errors even if Godot
  returns exit status zero.
- **Limitations:** Fire/damage integration hooks are tested using test-only
  damage, not a working weapon. No bots, grenades, jumping/crouching, or
  client prediction. Two-computer LAN and cross-platform runs still require
  teammates/CI; only loopback multiplayer is locally verified.

### 2026-11-08 - GameWorld/GameState scaffolding - GitHub Copilot
- **Where:** `GameWorld.hpp/.cpp`, `GameState.hpp/.cpp`, their tests,
  CMake targets, and README architecture notes (issue #3).
- **Why:** Implement the next engine task with beginner explanations.
- **Generated changes:** Copilot implemented ownership, update/removal,
  elapsed-time validation, and legal phase transitions. The learner's
  review, modifications, and understanding have not yet been confirmed.
- **Validation performed by Copilot:** `cmake -S . -B build
  -DCMAKE_BUILD_TYPE=Debug`, `cmake --build build --parallel 4`,
  `ctest --test-dir build --output-on-failure`, and
  `./build/tests/fireteam_tests` passed: 27 cases, 88 assertions.
  `git diff --check` passed. Existing third-party CMake deprecation
  warnings remain.
- **Memory check:** Configured with
  `cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug
  -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer"
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address"`, then built with
  `cmake --build build-asan --target fireteam_tests --parallel 4`.
  `./build-asan/tests/fireteam_tests --test-case='*GameWorld*,*GameState*'`
  passed with no ASan diagnostics (exit 0). Actual summary:

  ```text
  [doctest] test cases:  8 |  8 passed | 0 failed | 19 skipped
  [doctest] assertions: 41 | 41 passed | 0 failed |
  [doctest] Status: SUCCESS!
  ```

  Leak detection is not supported by Apple clang on this platform.
  No remote CI run or gameplay integration was performed for this change.
- **Learning topics to review:** `std::move` transfers ownership, removal
  destroys owned entities, and a state machine rejects illegal transitions.
  These are teaching topics, not a claim of learning already completed.
- **Excerpt:** Request: "lets get started then". Explanation:
  "GameWorld owns and updates entities, while GameState tracks the match phase."
