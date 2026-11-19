# GDExtension proof-of-concept spike — results (2026-11-16)

**Question:** can Fireteam's existing C++20 code (`GameWorld`/`Entity`/`GameState`, teammates'
`Weapon`/`Rifle` and `NetworkManager`) run as a **godot-cpp GDExtension** inside the approved Godot
4.7 client on this Windows machine, at $0, without rewriting the C++? **Answer: YES — every claim
verified end to end.**

Ran **fully isolated in a scratch workspace**; touched no repo branch and no committed build. It only
*reads* the real sources (`src/*.cpp`, `include/fireteam/*.hpp`) and the two open PRs' sources
(`origin/feature/weapon-rifle`, `origin/feature/network-manager`), all compiled **unchanged**.

## Result (Godot 4.7.2 headless, PATH cut to Windows system dirs)
```
SPIKE_CALC:   moved_x_times_100(2,3) = 600                                   (GDScript -> C++)
SPIKE_CORE:   entities=2 | e0.pos=(6.0, 8.0) | e0.health=70 | phase=1        (GameWorld/Entity/GameState)
SPIKE_WEAPON: Rifle.fire -> FireResult=0 (Hit)                              (teammate PR #16)
SPIKE_EXCEPT: caught: GameWorld cannot own a null entity                    (exception boundary)
SPIKE_NET:    net_connect_test = 1 (client+server connected on localhost)   (teammate PR #17, real ENet)
SPIKE_TICK:   engine drove GameWorldNode 5 physics ticks; entity x=0.4166   (engine -> C++)
SPIKE_RESULT  PASS
```
What each line proves:
1. **GDScript → C++**: Godot called a bound C++ method and got the C++-computed `600`.
2. **Existing core (develop) reused unchanged**: a `GameWorld` ran 2 entities; entity 0 integrated
   velocity (3,4)·2s → **(6,8)**; `applyDamage(30)` 100→**70**; `GameState`→InGame (**phase=1**).
3. **Teammate `Weapon`/`Rifle` (PR #16) reused unchanged**: `Rifle::fire` at an in-range target →
   `FireResult::Hit`.
4. **Exception boundary**: the core throws (`GameWorld` rejects a null entity); the bound method
   **catches** it so nothing propagates into godot-cpp (built `-fno-exceptions`).
5. **Teammate `NetworkManager` (PR #17) reused unchanged**: real **ENet** server + client completed
   a **localhost handshake** inside the extension.
6. **Engine → C++**: a C++ `GameWorldNode : Node3D` was ticked by Godot's `_physics_process` (5
   ticks), driving the engine-agnostic `GameWorld` — this is the exact recommended integration pattern.

## Environment / versions (all free, $0)
| Thing | Value |
|---|---|
| Godot | 4.7.2.stable.official.ed1daf0bf (console build) |
| godot-cpp | `master` pinned at **`abb196939767b7bbee0b3a380faefdcbdb118a54`** (bundles `extension_api-4-7.json`; **no 4.6/4.7 branch exists** — master is the match) |
| Compiler | MinGW **GCC 15.2.0** (bundled with CLion 2026.2.2) |
| Build | CLion-bundled CMake + **"MinGW Makefiles"** / `mingw32-make` (bundled cygwin `ninja` is broken — 0xc0000135) |
| ENet | already vendored on disk at `build-arena/_deps/enet-src/` — **no download needed** |
| Python | 3.13 (godot-cpp binding generator) |

## Build issues hit + fixes (carry into the real integration)
- **Cygwin `ninja` fails** (`0xc0000135`) → use **"MinGW Makefiles"** + `mingw32-make`.
- **C++20 required**: `GameWorld.cpp` uses `std::erase_if`. godot-cpp headers compile fine at C++20.
- **ABI match**: the prebuilt godot-cpp lib is `template_debug`; link consumers with the same defines
  it was built with — `DEBUG_ENABLED GDEXTENSION THREADS_ENABLED WINDOWS_ENABLED` + `-fvisibility=hidden`.
  (A `template_release` godot-cpp is the eventual shipping build.)
- **Runtime dep confirmed**: the DLL needs **`libwinpthread-1.dll`**. With PATH cut to system dirs and
  the sibling DLL present → loads + PASS; **remove it → Godot: "Missing dependencies:
  libwinpthread-1.dll. Error 126", extension won't load.** Fix: ship it beside `fireteam.dll`, put it
  on PATH, or static-link it (full `-static` currently breaks on GCC 15 — `__ms_vsnprintf` /
  `__intrinsic_setjmpex` — so shipping the one DLL is the pragmatic answer).
- **`windows.h` vs godot-cpp macros**: `NetworkManager.hpp` → `<enet.h>` → `<windows.h>`, whose macros
  collide with Godot headers. Fix: keep ENet/NetworkManager in a **separate translation unit**
  (`net_probe.cpp`) that never includes godot-cpp; the bridge calls it through a plain forward decl.
- **ENet link**: compile `enet-src/*.c` (win32.c, not unix.c) + link **`ws2_32`, `winmm`**.

## Reuse verdict (spike items 1–5)
`fireteam_core` includes **only std headers** — no SFML, no ENet — and `Weapon`/`Rifle` only depend on
`Entity`. All compiled into the extension **with zero changes**. The 3D work is **additive**: `Entity`
gains a `Node3D`/`CharacterBody3D` adapter, `GameWorld` is owned by a `Node3D` whose
`_physics_process` calls `update` (proven above). `Vector2` stays as the X/Z ground math; 3D
collision/hitscan is new Godot-physics code, not a rewrite. `render()` was already dropped — fits Godot.

## ENet / networking (spike item 6) — PROVEN
The **C++ `NetworkManager` (raw ENet)** connected a server + client on localhost **inside the
extension**. **The one rule: pick ONE networking stack** — keep the C++ `NetworkManager` (matches
`docs/rendering-architecture.md`, keeps "substantial C++") **or** use Godot's `ENetMultiplayerPeer`
(as `godot/cpp/README.md` suggests), never both. **Recommended: keep the C++ `NetworkManager`.** Next
step (Aditya's lane): build the state-replication layer on top (the transport is proven).

## Limitations / open items
- **Cross-platform**: CI builds on **ubuntu + macos + windows** (`.github/workflows/ci.yml`), but this
  POC + its `.gdextension` are **Windows-only**. A real integration needs linux/macos library entries
  and godot-cpp built on each (expected to work — godot-cpp is cross-platform — but **unverified
  here**). *Which OSes do teammates build on?* → confirm before committing CI.
- godot-cpp has **no 4.7 branch**; `master` matches today but moves — **pin the commit** (done above).
- DLL carries a `libwinpthread-1.dll` runtime dep (ship alongside) — see above.
- Hot-reload (`reloadable=true`) leaves a `~fireteam.dll` temp copy — cosmetic.
- CI can't render (no GPU) but **can** build the extension + run this headless self-test as a gate.

## Recommendation
**Adopt C++ GDExtension as the primary architecture.** Godot owns rendering/scenes/animation/
camera/3D-physics; **gameplay rules, state, combat and netcode stay in C++20** as a godot-cpp
GDExtension, still doctest-unit-testable. The spike shows this is **buildable and correct** on the
existing $0 toolchain, reusing the current C++ and both teammates' PRs unchanged. This satisfies the
"keep substantial C++" goal while using Godot for the 3D the team already built and approved.
