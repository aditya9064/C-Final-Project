# C++ gameplay layer (GDExtension) — integration plan

> **Status: NOT YET BUILT.** This is the documented plan + class mapping, not
> compiled code. No `.cpp` gameplay has been written or built for Godot. The
> arena, materials, cameras, cover collision and cover raycast tests are
> implemented in GDScript (`../scripts/build_arena.gd`) so the renderer could be
> validated by actually running it. Below is how the C++20 gameplay from the
> original design maps onto Godot via `godot-cpp`. The team-level decision
> (SFML vs Godot, what must change, CI implications) is written up in
> [`../../docs/rendering-architecture.md`](../../docs/rendering-architecture.md);
> this file is the lower-level class mapping that would follow from it.

## Architecture decision (Part 12.1)

The original project specified **SFML + ENet + CMake**. The visual prototype is
**Godot 4.7 Forward+**. These two renderers must not be blended. Recommended,
pending your professor's constraints:

- **If Godot is permitted as the renderer:** Godot owns rendering, scenes, 3D
  collision/raycast, lighting, animation, camera, audio. Gameplay logic stays in
  **C++20** compiled as a **GDExtension** (`godot-cpp`). Networking uses Godot's
  built-in **ENetMultiplayerPeer** (host = authoritative server) rather than a
  second hand-rolled ENet stack.
- **If the course strictly requires SFML rendering:** that is a *course-
  compliance blocker*. Keep the existing SFML/ENet project (`../../src`,
  `../../tests`, `CMakeLists.txt`) as the graded deliverable and treat this
  Godot build as a **separate visual prototype**. Do not delete the SFML code.

The existing SFML game (`../../src`, smoke-test `fireteam_skirmish`) and tests
are **untouched** by this Godot work.

## Class mapping (preserve the original concepts — Part 12.1)

| Original C++ concept | GDExtension class (godot-cpp) | Notes |
|---|---|---|
| `Entity` (abstract) | `class Entity : public CharacterBody3D` | shared movement/health base |
| `Player` | `class Player : public Entity` | input-driven; WASD on X/Z plane |
| `Bot` | `class Bot : public Entity` | NavigationAgent3D + LOS raycasts |
| `Weapon` / `Rifle` | `class Weapon`, `class Rifle : public Weapon` | hitscan via `PhysicsDirectSpaceState3D::intersect_ray` |
| `GameWorld` | `class GameWorld : public Node3D` | owns entities, spawns, match flow |
| `NetworkManager` | wrapper over `ENetMultiplayerPeer` + RPCs | host authoritative |
| `GameState` | `enum { Lobby, InGame, MatchOver }` | state machine on GameWorld |

Existing `Vector2` ground math can be reused for X/Z; **collision and hitscan
must run in 3D** (`Vector3`, Godot physics) — the raycast cover tests in
`build_arena.gd::_run_cover_tests()` already demonstrate the exact 3D hitscan
call the `Rifle` will use.

## Build skeleton (next step, not done)

```
godot/cpp/
  godot-cpp/            # submodule: git submodule add https://github.com/godotengine/godot-cpp
  src/
    register_types.cpp  # GDREGISTER_CLASS for each class above
    entity.{h,cpp} player.{h,cpp} bot.{h,cpp}
    weapon.{h,cpp} rifle.{h,cpp}
    game_world.{h,cpp} network_manager.{h,cpp}
  SConstruct            # scons build -> ../bin/libfireteam.<platform>.<target>
  ../fireteam.gdextension  # tells Godot where the compiled library is
```

Build: `cd godot/cpp && scons platform=windows target=template_debug`
(requires Python + SCons + the godot-cpp submodule at the 4.7 branch).

## Priority order for the gameplay pass

1. `Player` movement (WASD on X/Z, accel/decel, crouch, collision) — reuses the
   `CONFIG` scale constants already in `build_arena.gd`.
2. `Rifle` 3D hitscan + the cover rules the raycast tests already encode.
3. `GameWorld` + `GameState` (Lobby/InGame/MatchOver), team-win condition.
4. `NetworkManager` over ENetMultiplayerPeer, host authoritative, transform/
   health/fire sync, join/disconnect/match-end.
5. `Bot` with NavigationAgent3D on a navmesh baked from the arena collision.
