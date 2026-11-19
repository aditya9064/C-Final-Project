# Running the playable Fireteam: Skirmish 3D client

The single-player 3D client: the C++20 gameplay core compiled as a Godot
**GDExtension**, driving a third-person operator in the team arena, with the HUD.
(For the arena environment itself see [`godot/README.md`](../godot/README.md).)

## Which branch to run
Check out **`feature/gameplay-player-hud`** — it contains the gameplay code **and**
the approved character/weapon assets, so it is the one combined, playable version.
(The character assets also have their own PR; this branch already includes them.)

## Required free software ($0 — all free / open-source)
- **Godot 4.7.2 stable** — <https://godotengine.org/download> (the standard, non-.NET build).
- **CMake 3.17+** and a **C++20 compiler**. On Windows this was built with **MinGW-w64 GCC**
  (the GCC bundled with CLion works). Any MinGW-w64 GCC that ships `libwinpthread-1.dll` is fine.
- The first CMake configure downloads **godot-cpp** (a pinned commit) over the network — no account, no payment.

## 1. Build the C++ GDExtension
The compiled library is **not** committed (it is machine-specific), so build it once.
From the repository root:

```sh
cmake -S godot/cpp -B godot/cpp/build
cmake --build godot/cpp/build -j 4
```

This produces **`godot/cpp/bin/fireteam.dll`**. Then copy your MinGW runtime
`libwinpthread-1.dll` next to it (Godot needs it beside the DLL, or it reports a
missing dependency):

```sh
cp "<your-mingw>/bin/libwinpthread-1.dll" godot/cpp/bin/
```

> **macOS:** `fireteam.gdextension` has macOS entries, but the macOS / `.dylib` build has
> **not** been verified for this project yet — build it on a Mac (same CMake steps,
> producing `fireteam.dylib`) before relying on it. Only the Windows build is verified.

## 2. Open the project
Launch Godot 4.7.2 → **Import** → pick **`godot/project.godot`**. On first open Godot
imports the assets (characters, weapons, arena props); let it finish.

## 3. Run the game
Open **`scenes/game.tscn`** in the editor and press **F6** (Run Current Scene).

> Pressing **F5** runs the project's configured main scene, which is the standalone
> arena (`arena.tscn`) used for rendering QA — **not** the playable game. Use **F6 on
> `game.tscn`** for the player + HUD. (Left `project.godot` unchanged so the arena QA
> workflow still works; set the main scene to `game.tscn` locally if you prefer F5.)

## Controls
| Action | Input |
|---|---|
| Move | `W` `A` `S` `D` |
| Look | Mouse |
| Sprint | `Shift` |
| Jump | `Space` |
| Fire | Left mouse |
| Aim | Right mouse |
| Reload | `R` |

The mouse is captured on start; press **Esc** to release it.

## What works now (single-player, local)
- 3D movement, gravity, jumping, third-person spring-arm camera.
- Character selection + team spawns: **Teal** (male) at `x = -18`, **Orange** (female)
  at `x = +18`, each facing inward.
- Animation controller over the operator clips; SMG attached to the right hand.
- HUD: health, ammo + reload bar, team indicator, match timer + score, crosshair,
  respawn countdown, ELIMINATED overlay, hit-marker / kill-feed seams.
- **Local authoritative respawn:** after the death countdown the host authority rebuilds
  the player at its team spawn (full health/ammo, camera + animation reset).

## What still depends on teammates (not yet wired)
- **Combat (#8 / #9, Khushal):** real hit detection and damage. The client only exposes
  the seam — a damage sink, and the core `Player` **is** a `fireteam::Entity` so
  `Weapon::fire(Entity&)` can target it unchanged.
- **Networking (#10 / #11, Aditya):** multiplayer replication and **respawn authorization**.
  The client exposes a clean `respawn()` action plus a state snapshot; the server owns timing.
  There is **no verified multiplayer** yet — single-player only.

## Validating the build (optional)
Assert-based headless checks live in `godot/scenes` + `godot/scripts`
(`game_smoke`, `play_check`, `hud_check`, `rig_check`, `respawn_check`); the C++ core
has doctests under `tests/`. None are needed just to play.
