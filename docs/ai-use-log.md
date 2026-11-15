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

<!-- Add further entries above this line as the project progresses. -->

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

### 2026-11 (several sessions, finalised 2026-11-13) — Koushik Cherukuri — Claude Code (Anthropic Claude)
- **Where:** the Godot arena environment: `godot/scripts/build_arena.gd`,
  `godot/scenes/arena.tscn`, `godot/project.godot`, the Blender generators in
  `godot/pipeline/*.py`, the QA scripts/reports in `godot/qa/`, and the
  documentation for this PR (`docs/rendering-architecture.md`, `godot/README.md`,
  `godot/pipeline/README.md`, `godot/assets/MANIFEST.md`, README updates, this entry).
  No C++ source, test or CMake file was written or changed with AI in this work.
- **Why:** Building a 3D arena to the agreed spec (octagon footprint, 4.50 m
  walls, 1.28 m / 2.10 m cover, canopy) and matching eight concept-art references
  is a large amount of geometry, material and asset-pipeline code outside the
  course's C++ scope. I directed the work (spec, references, what to change
  after each render, what to preserve) and used Claude Code as the implementer
  and for the repo clean-up / documentation pass.
- **What I kept/changed:** Most of `build_arena.gd` and the pipeline scripts
  were generated by the assistant from my spec and then iterated against real
  Godot screenshots that I reviewed; I approved the final arena visually before
  this PR was prepared. Code I rejected along the way (e.g. an SDFGI lighting
  setup that darkened the floor, a procedural canopy GLB replaced by the
  in-script fabric roof) is not in the branch. Per CONTRIBUTING.md I am
  responsible for being able to explain and modify this code without AI help;
  the section comments in `build_arena.gd` were written with that in mind and
  reviewers should expect me to walk through any part of it.
- **How I checked it:** Every change was verified by running Godot, not by
  reading code: 17 fixed-camera screenshots per pass, the five cover raycast
  tests printed on each launch (5/5 PASS), `godot --headless --import` from a
  wiped cache (0 errors), 300-frame benchmarks per preset, and a repeatability
  test that regenerated all Blender assets and compared hashes (22/22
  identical). For this PR the C++ tree (unchanged `develop` code) was configured, built and
  `fireteam_tests` run (27 cases / 88 assertions pass). Evidence:
  `godot/qa/arena_final_candidate/`, `docs/evidence/arena-2026-11-13-godot-qa.md`.
- **What I learned:** (1) Godot's `Node3D.rotation.y` and a hand-written
  rotation matrix use opposite sign conventions — the script needs two helpers
  (`_rot` for world-space offsets, `_rotg` for parts attached to a rotated
  node). (2) GDScript infers `Variant` when iterating an untyped array, so
  `var count := max(...)` fails to parse; explicit types (`var count: int`) are
  needed. (3) Blender's bundled glTF exporter only loads reliably in headless
  runs with `--factory-startup`. (4) Godot's GLB importer extracts embedded
  textures to loose PNGs beside the file; five tree GLBs each embedded the same
  8 MB bark atlas, so the kit was re-exported as `.gltf` with one shared
  `textures/` folder (54 MB → 17 MB, pixels identical, verified by re-render). (5) Foliage needs alpha-scissor, not
  alpha-blend, or overlapping leaf cards sort wrongly and look like ghosts.
- **Excerpt:** Prompt (abridged): "Environment only. Do NOT do characters,
  animation, movement, weapons, AI, networking… References are the visual
  source of truth… Produce the QA package… stop and wait for owner approval."
  Response ended with the report heading "FINAL STATUS: ARENA READY FOR OWNER
  REVIEW" and the list of known remaining issues now in `known_issues.md`.
