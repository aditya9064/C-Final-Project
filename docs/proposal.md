# Proposal: Fireteam: Skirmish

**Course:** CSC2210 Procedural and Object-Oriented C++
**Team:** Aditya Miriyala, Khushal Lingamaneni, Koushik Cherukuri
**Revision:** Professor-approved pivot to a scoped 3D first-person game

## 1. Overview

**Fireteam: Skirmish** is a small 3D first-person shooter for two players on a local network. Players join one compact arena, move and aim from a first-person camera, and use a hitscan rifle to compete in a short match. The project is intended to demonstrate procedural and object-oriented C++ through a working game loop, entity composition and inheritance, collision and combat logic, and ENet-based multiplayer without depending on a dedicated online service.

## 2. Core features (must work)

1. **A playable 3D arena** with first-person movement and mouse-look controls.
2. **Two-player LAN match** using ENet, with a host and a joining client.
3. **Combat loop**: one hitscan rifle, 3D hit detection, health, elimination, and a visible match result.
4. **Consistent match state**: the host is authoritative for damage and the win condition; clients send player input/fire requests and receive replicated state.
5. **Object-oriented design**: polymorphic `Entity`/`Player`, a `Weapon` composed into `Player`, an RAII `NetworkManager`, a `GameWorld`, and a `GameState` state machine (Lobby → InGame → MatchOver).

**Verification:** build and run two game instances on the same LAN; confirm both can move in the arena, see each other update, hit an opponent with the rifle, observe the opponent's health/elimination state, and reach a match result.

### Stretch goals (not required for core grade)

- Simple bots, only after the two-player game is stable
- Grenade with gravity, bounce, and blast radius
- More players, another arena, improved HUD, animation, and sound

## 3. Design sketch

- `Vector3` — 3D positions, directions, and movement math used by gameplay
- `Entity` (abstract base: position, health, virtual `update()`) → `Player` (derived); rendering is handled separately
- `Weapon` composed *into* `Player` (base class + `Rifle`; stretch `Grenade`)
- `NetworkManager` — owns the ENet host/peer lifetime via RAII and transports application messages
- `GameWorld` — owns entities in `std::vector<std::unique_ptr<Entity>>`, runs updates, and handles simple 3D collision/hit queries
- `GameState` — Lobby / InGame / MatchOver state machine
- `Renderer` — raylib-based 3D camera, scene, and HUD; reads game state rather than owning gameplay rules

**Data flow:** keyboard/mouse input produces local movement and fire requests → the host validates and applies match changes → ENet replicates player/match state → each client's renderer draws the arena and players from a first-person camera.

**Design choices:** `Player`-has-a-`Weapon` uses **composition**, so weapons can be changed without changing the player type. `Player`-is-an-`Entity` uses **inheritance**, allowing a polymorphic entity container. Rendering stays separate from entities so gameplay rules and hit detection can be tested without opening a graphics window.

## 4. Tools

- **Build:** CMake (team is split Windows/Mac, so CMake keeps the build portable); compiled with g++/clang/MSVC depending on platform
- **Standard:** C++20
- **Libraries:** raylib 5.5 (3D rendering/window/input) and ENet 1.3.18 (networking), fetched through CMake `FetchContent`
- **Hardware:** standard laptops on the same LAN/Wi-Fi for multiplayer testing; no special hardware
- **Scope control:** first complete and test a local 3D arena and combat loop, then connect two instances over ENet. Bots, grenades, and 5v5 are explicitly optional.
- **Risk mitigation:** keep the arena, movement model, and collision rules simple; test gameplay logic separately from raylib rendering and add LAN synchronization only after local play works.

## 5. Work plan

- **Aditya** — core engine/game loop, `NetworkManager` (ENet wrapper), and state synchronization
- **Koushik** — raylib 3D rendering, camera, player controls, and HUD
- **Khushal** — weapon/combat systems and 3D hit detection; optional grenade/bot work only after core completion

Everyone writes tests for their own subsystem and reviews teammates' pull requests, so all three can explain the full program.

| Week | Milestone |
|---|---|
| 9 | Proposal and repository foundation |
| 10 (Check-in 1) | Local 3D arena, first-person movement/look, and initial unit tests |
| 11 | Two instances exchange player state; rifle hit detection and damage integrated |
| 12 (Check-in 2) | Two-player core match playable; memory diagnostics run; tests updated |
| 13 | Stabilize core; attempt stretch goals only if required features are reliable |
| 14 (Readiness check) | Fresh-clone build test by one teammate using only the README; demo rehearsal |
| 15 | Final polish, report, slides, submission |

**Merge strategy:** short task branches start from `develop`; reviewed, passing pull requests merge into `develop`, with tested checkpoints promoted to `main` by pull request. No direct commits to `main`.

## 6. Test plan and sources

- **Normal:** two connected instances move and aim; a valid rifle hit reduces the target's health and elimination triggers the match result.
- **Boundary:** a lobby with fewer than two players cannot start; movement and aim remain within the arena bounds.
- **Combat:** rays that miss do not apply damage; valid hits apply damage once and respect the weapon cooldown.
- **Invalid input:** malformed or out-of-range network messages are rejected without crashing or corrupting match state.
- **Failure condition:** a client disconnects mid-match and the host ends or continues the match cleanly.

**Sources:** The team selected the 3D first-person direction and scoped core features; the professor approved the proposal change. GitHub Copilot helped organize the revised plan. External tutorials and references used during implementation will be recorded in the AI-use log.
