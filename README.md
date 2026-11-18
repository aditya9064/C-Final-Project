# Fireteam: Skirmish

![CI](https://github.com/aditya9064/c-final-project/actions/workflows/ci.yml/badge.svg)
![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)
![Platforms](https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux-lightgrey)

A scoped 3D first-person shooter for two players on a local network — built
as our CSC2210 (Procedural and Object-Oriented C++) final project. Explore a
compact arena, aim and move from a first-person camera, and compete with a
hitscan rifle. Bots, grenades, and larger matches are optional stretch goals.

> **Course:** CSC2210 Procedural and Object-Oriented C++ — Dr. Chibuzo Ukegbu
> **Team:** Aditya Miriyala, Khushal Lingamaneni, Koushik Cherukuri

## Table of contents

- [About](#about)
- [Features](#features)
- [Demo](#demo)
- [Architecture](#architecture)
- [Tech stack](#tech-stack)
- [Getting started](#getting-started)
- [Project structure](#project-structure)
- [Testing](#testing)
- [Development workflow](#development-workflow)
- [Roadmap](#roadmap)
- [Documentation](#documentation)
- [License](#license)

## About

**Fireteam: Skirmish** is a small 3D first-person LAN shooter. Its core goal
is a playable two-player match in one compact arena: first-person movement
and mouse-look, one hitscan rifle, 3D hit detection, health, elimination,
and a match result. The host owns authoritative match state; ENet carries
the messages between instances. A 5v5 mode, bots, grenades, and extra maps
are stretch goals, not prerequisites for a complete project.

Full project rationale, scope, and design justification are in the
[project proposal](docs/proposal.md) ([PDF](docs/proposal.pdf)).

## Features

### Core (required to work)
- Real-time networked gameplay for two human players over LAN via ENet
- 3D first-person movement and mouse-look in one compact arena
- One hitscan rifle, 3D hit detection, health and elimination
- Host-authoritative match result, replicated to both players
- Clean OOP structure: polymorphic `Entity` hierarchy, RAII-wrapped
  networking, explicit `GameState` state machine

### Stretch goals
- AI-controlled bots filling empty lobby slots (scaling toward 5v5)
- Grenade/explosive weapon with real projectile physics (gravity, bounce,
  blast radius)
- Full 5v5 scale, HUD polish, sound, multiple maps

## Demo

> 🚧 **Coming soon.** A gameplay demo (recording + screenshots) will be
> added here once core features are implemented and playable end-to-end —
> we're not going to fake footage of a game that doesn't exist yet. Check
> back around Check-in 2 / the readiness check milestone.

## Architecture

### Entity & subsystem design

```mermaid
classDiagram
    class Entity {
        <<abstract>>
        +Vector3 position
        +int health
        +update()
    }
    class Player {
        +Weapon* weapon
        +bool isLocal
        +readInput()
        +fire()
    }
    class Bot {
        +AIState state
        +decide()
    }
    class Projectile {
        +Vector3 velocity
        +applyPhysics()
    }
    class Weapon {
        <<abstract>>
        +fire()
    }
    class Rifle
    class Grenade

    Entity <|-- Player
    Entity <|-- Bot
    Entity <|-- Projectile
    Weapon <|-- Rifle
    Weapon <|-- Grenade
    Player "1" *-- "1" Weapon : has-a (composition)

    class NetworkManager {
        +connect()
        +send()
        +receive()
    }
    class GameWorld {
        +entities()
        +addEntity()
        +update()
        +checkHits()
    }
    class Renderer {
        +Camera3D camera
        +draw(GameWorld)
    }

    GameWorld "1" *-- "*" Entity : owns
    GameWorld --> NetworkManager : uses
    Renderer --> GameWorld : reads
```

We use **composition** for `Player`-has-a-`Weapon` (weapons are swappable,
not baked into the player's type) but **inheritance** for the `Entity`
hierarchy, since polymorphic dispatch over one container of entities is the
natural fit for a single game-update loop. See
[docs/proposal.md](docs/proposal.md#3-design-sketch) for the full rationale.

Implemented foundation: `GameWorld` owns entities with `unique_ptr`, updates
living entities, then removes dead ones. Adding a null entity or updating
with negative/non-finite elapsed time throws `std::invalid_argument`.
References to removed entities must not be used after an update.
The renderer reads entity data rather than calling `Entity::render()`, and
the gameplay core does not depend on Godot.

`GameState` starts in `MatchPhase::Lobby` and permits only the cycle
Lobby -> InGame -> MatchOver -> Lobby. Illegal transitions throw
`std::logic_error` without changing the phase. `MatchSession` handles two-player
joining, host-authoritative movement/health, elimination, rematches, and
disconnects. The Godot `FireteamSession` extension delegates movement collision
to Godot physics while keeping match rules in C++. `Vector3` supplies the 3D math
used by entity positions and movement; `Vector2` remains available for
genuinely 2D uses such as HUD layout.

`Player` derives from `Entity` and applies validated, frame-rate-independent
movement in player-relative X/Z directions, plus yaw and pitch. It reports a
fire request to the combat layer. `godot/scripts/game.gd` connects input and a
first-person camera; the C++ extension resolves player motion against the arena.
Weapons and hit detection remain Khushal's integration work.

`NetworkProtocol` defines versioned, fixed-size, little-endian messages for
client movement, aim, and fire input, plus the host's two-player match
snapshot. Encoders and decoders validate ranges, player health, IDs, packet
headers, and malformed data before accepting it. The ENet transport wrapper
from [PR #17](https://github.com/aditya9064/C-Final-Project/pull/17) is integrated
with `MatchSession`. Host is player 1, the sole client is player 2. Client
messages carry controls, never trusted transforms or health. See
[the Godot C++ integration guide](godot/cpp/README.md).

### Match state machine

```mermaid
stateDiagram-v2
    [*] --> Lobby
    Lobby --> InGame: enough players connected
    InGame --> MatchOver: one side eliminated
    MatchOver --> Lobby: rematch
    MatchOver --> [*]: quit
```

### Host-authoritative network data flow (a shot being fired)

```mermaid
sequenceDiagram
    participant P1 as Player 1 (host)
    participant N1 as NetworkManager (host)
    participant N2 as NetworkManager (joining client)
    participant P2 as Player 2 (joining client)

    P2->>N2: Input: fire request
    N2->>N1: Send request (ENet)
    N1->>N1: Validate shot and update match state
    N1-->>N2: Replicate health and match state
    N1->>P1: Update host world
    N2->>P2: Update client world
```

### Client/server component overview

```mermaid
flowchart LR
    subgraph Host [Host]
        InputA[Input] --> PlayerA[Player]
        PlayerA --> WorldA[GameWorld]
        WorldA --> RendererA[Renderer / Godot]
        WorldA <--> NetA[NetworkManager / authority]
    end
    subgraph Client [Joining client]
        InputB[Input] --> PlayerB[Player]
        PlayerB --> WorldB[GameWorld]
        WorldB --> RendererB[Renderer / Godot]
        WorldB <--> NetB[NetworkManager]
    end
    NetA <-->|ENet over UDP| NetB
```

## Tech stack

| Concern | Choice | Why |
|---|---|---|
| Language | C++20 | Course requirement |
| Build | CMake 3.16+ with `FetchContent` | One build system works across Windows/macOS/Linux with no manual dependency installs |
| Rendering/window/input | [Godot](https://godotengine.org/) 4.7 with C++ GDExtension | Reuses Koushik's 3D arena; gameplay and ENet remain C++ |
| Networking | [ENet](http://enet.bespin.org/) 1.3.18 | Purpose-built for real-time game networking (sequencing/reliability over UDP), much smaller surface than general-purpose async I/O libraries |
| Testing | [doctest](https://github.com/doctest/doctest) 2.4.11 | Header-only, fast to compile, fast to run in CI |

## Getting started

### Prerequisites
- CMake ≥ 3.16
- A C++20 compiler (recent MSVC, GCC, or Clang)
- Python 3 (Godot binding generator) and Godot 4.7 standard edition
- Internet connection on first configure (CMake `FetchContent` downloads
  godot-cpp, ENet, and doctest source)

### Build

```bash
git clone https://github.com/aditya9064/c-final-project.git
cd c-final-project
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

### Run

```bash
# Replace <godot> with your Godot 4.7 executable.
<godot> --headless --path godot --editor --import --quit
<godot> --path godot -- --host --port 27770
# Run a second instance (or use the host's LAN IP on a second computer).
<godot> --path godot -- --join 127.0.0.1 --port 27770
```

The default scene has host/join controls, first-person WASD/mouse input,
host-side arena collision, replicated players, health, match state, and a
host rematch control. The local soldier is hidden in first person and the
remote player uses a supplied teal or orange soldier model. An animated
first-person arms-and-weapon model follows the camera; click plays its fire
animation and R plays a visual reload. The HUD, adapted from Koushik's PR #20,
shows replicated health, team, phase, winner, and a crosshair. These visuals
do not yet implement shooting
or ammunition. The soldier models are static. Asset attribution and license
details are in [the Godot integration guide](godot/cpp/README.md). The older
raylib smoke test is preserved behind
`-DFIRETEAM_BUILD_LEGACY_SMOKE=ON`; core-only tests can use
`-DFIRETEAM_BUILD_GODOT=OFF`.

## Project structure

```
c-final-project/
├── .github/            CI workflow, issue/PR templates
├── assets/             Game assets (models, textures, fonts, sounds) as added
├── docs/                proposal, AI-use log, evidence (tests/memory diagnostics)
├── include/fireteam/    Public headers
├── godot/               Arena, gameplay UI, C++ extension, integration tests
├── src/                 Implementation (.cpp)
├── tests/                doctest-based test suite
└── CMakeLists.txt        Build configuration (FetchContent for all dependencies)
```

## Testing

```bash
cd build
ctest --output-on-failure
```

Our test plan (normal, boundary/empty, invalid-input, and failure-condition
cases) is documented in
[docs/proposal.md](docs/proposal.md#6-test-plan-and-sources) and will grow
alongside each subsystem. Memory diagnostic tool output and the required
test-evidence table will be added under `docs/` as they're produced.

## Development workflow

- `main` — always buildable/demoable, protected, merges only via reviewed PR
- `develop` — integration branch for finished feature work
- Short-lived `feature/*` task branches start from `develop` and merge back
  after review and passing checks

```mermaid
gitGraph
    commit id: "init"
    branch develop
    checkout develop
    commit id: "scaffold"
    branch feature/task
    checkout feature/task
    commit id: "tested task"
    checkout develop
    merge feature/task
    checkout main
    merge develop tag: "Check-in 1"
```

Full conventions (commit style, review expectations, code style) are in
[CONTRIBUTING.md](CONTRIBUTING.md).

| Area | Owner |
|---|---|
| Core engine, game loop, `NetworkManager` | Aditya Miriyala |
| Godot arena, 3D rendering, characters, visual design | Koushik Cherukuri |
| Weapons, 3D hit detection, optional bots/grenade | Khushal Lingamaneni |

(Everyone writes tests for their own subsystem and reviews teammates' PRs,
per the course's teamwork requirement — ownership above is primary
responsibility, not exclusive access.)

## Roadmap

| Week | Milestone |
|---|---|
| 9 | Proposal and repository foundation |
| 10 (Check-in 1) | Local 3D arena, first-person movement/look, initial unit tests |
| 11 | Two instances exchange player state; hit detection and damage integrated |
| 12 (Check-in 2) | Two-player core match playable; memory diagnostics run |
| 13 | Stabilize core; optional goals only if required features are reliable |
| 14 (Readiness check) | Fresh-clone build test using only this README; demo rehearsal |
| 15 | Final polish, report, slides, submission |

## Documentation

- [Project proposal (PDF)](docs/proposal.pdf) / [source](docs/proposal.md)
- [AI use log](docs/ai-use-log.md)
- [Contributing / branching conventions](CONTRIBUTING.md)

## License

Project code is licensed under [MIT](LICENSE). The animated viewmodel has
separate [attribution and license terms](godot/cpp/README.md#animated-viewmodel-attribution).
