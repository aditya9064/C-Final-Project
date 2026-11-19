# C++ gameplay layer (GDExtension)

> **Status: building.** The GDExtension spike passed
> ([`docs/evidence/gdextension-spike-2026-11-16.md`](../../docs/evidence/gdextension-spike-2026-11-16.md))
> and the team adopted **C++ GDExtension** as the primary architecture. This folder now holds the real
> extension (issues **#6 Player**, **#7 HUD** — Koushik's lanes), not a plan. Combat (#8/#9) and
> networking (#10/#11) remain teammates' lanes; this layer only exposes **seams** for them.

The approach (and why Godot + C++, not SFML) is in
[`docs/rendering-architecture.md`](../../docs/rendering-architecture.md): Godot owns rendering, scenes,
3D collision/raycast, animation, camera, audio, UI; **gameplay logic stays in C++20**, compiled as a
`godot-cpp` GDExtension, still unit-tested with doctest.

## Layering (keep substantial graded C++)

| Layer | Lives in | Role |
|---|---|---|
| `fireteam::Player : Entity` | `src/Player.cpp` (**fireteam_core**, doctested) | engine-agnostic model: planar movement, ammo/reload, respawn, team, snapshot seam |
| `PlayerBody : CharacterBody3D` | `godot/cpp/src/player_body.*` | thin adapter: input → model → `move_and_slide` → write resolved X/Z back |
| `GameWorldNode : Node3D` | `godot/cpp/src/game_world_node.*` | match flow via `fireteam::GameState` + timer/scores |

The model is `Entity`-derived, so Khushal's `Weapon::fire(Entity&)` (#16) targets a `Player` unchanged
when combat lands. Movement/ammo/respawn are tested in `tests/test_player.cpp` (pure C++, no engine).

### Seams (not implemented here — teammates' lanes)
- **Combat (#8/#9):** `PlayerBody::request_fire()` consumes a round via the model and returns whether a
  shot left the muzzle. It does **not** compute hits/damage; the weapon system does, through
  `Entity::applyDamage`.
- **Networking (#10/#11):** `fireteam::Player::snapshot()` / `applyRemoteState()` are the authoritative
  state seam, fed by local test data for now. The transport is the C++ `NetworkManager` (one stack —
  **not** Godot's `ENetMultiplayerPeer`), consistent with the spike and `docs/rendering-architecture.md`.

## Build (CMake, not scons)

godot-cpp is pinned via `FetchContent` at the spike-proven commit
`abb196939767b7bbee0b3a380faefdcbdb118a54` (it bundles `extension_api-4-7.json`; there is no 4.7
branch, so the commit is pinned). Use the CLion-bundled MinGW GCC + `"MinGW Makefiles"` (the bundled
cygwin `ninja` is broken). C++20 is required (`std::erase_if`).

```
# default: fetch + build godot-cpp, then the extension
cmake -S godot/cpp -B build-ext -G "MinGW Makefiles" \
      -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build build-ext -j            # -> godot/cpp/bin/fireteam.dll

# fast local iteration against an already-built godot-cpp:
#   add -DGODOTCPP_PREBUILT=ON -DGODOTCPP_SRC=<checkout> \
#       -DGODOTCPP_LIB=<libgodot-cpp...a> -DGODOTCPP_GEN=<build dir>
```

Ship **`libwinpthread-1.dll`** beside `fireteam.dll` in `godot/cpp/bin/` — the spike confirmed Godot
fails with "Missing dependencies" (Error 126) without it. Built binaries are git-ignored; teammates
build locally or pull the CI artifact, then run an import pass (`godot --headless --import --path godot`)
so Godot writes `.godot/extension_list.cfg` and loads the extension.

## Platforms
Windows (MinGW GCC 15.2.0) is **built and verified**. `fireteam.gdextension` also declares macOS
entries; those are built in CI / by the Mac teammate and are **unverified on the Windows dev machine**
(godot-cpp is cross-platform, so they are expected to work once built). Linux/macOS CI is a separate,
non-blocking workflow.

## Verify
```
cmake --build build-ext -j                                   # extension builds
godot --headless --import --path godot                       # registers the extension
godot --headless --path godot res://scenes/game_smoke.tscn   # -> GAMEPLAY_SMOKE PASS
ctest --test-dir <core build>                                # 37 doctests incl. Player
```
