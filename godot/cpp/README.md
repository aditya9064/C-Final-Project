# C++ gameplay GDExtension

Godot 4.7 loads `FireteamSession`, a C++ `Node3D` backed by the existing core
and the reviewed ENet transport from PR #17. Koushik's arena from PR #18 is
included without modifying its builder. The source proposal remains historical;
see [the current architecture](../../docs/rendering-architecture.md).

## Build and run

Install CMake, a C++20 compiler, Python 3, and Godot 4.7 standard edition.
From the repository root:

```sh
cmake -S . -B build-godot -DCMAKE_BUILD_TYPE=Debug
cmake --build build-godot --config Debug --parallel 4
ctest --test-dir build-godot --build-config Debug --output-on-failure
<godot> --headless --path godot --editor --import --quit
<godot> --path godot -- --host --port 27770
<godot> --path godot -- --join 127.0.0.1 --port 27770
```

Replace `<godot>` with the executable's path; quote paths containing spaces.
Use the host's LAN IP instead of `127.0.0.1` on another computer. Both instances
must use the same protocol and allow UDP traffic on the selected port.

CMake downloads pinned godot-cpp stable 4.5 bindings (compatible with 4.7)
and outputs `godot/bin/fireteam.{dll,so,dylib}`. Generated bindings, binaries,
and Godot caches are ignored. Build each OS's library locally; do not commit it.
For core-only builds use `-DFIRETEAM_BUILD_GODOT=OFF`.

## Controls and scope

Host/join buttons are also available on launch. WASD moves on the ground plane;
mouse looks, Esc opens the menu, and clicking outside the menu recaptures the
mouse. R plays a visual reload during a match. Host stays in the lobby until
one client joins. MatchOver enables the
host's rematch button. Disconnect clears the client's slot and returns the
host to Lobby.

The HUD adapts Koushik's work from PR #20 to show the local team, replicated
health, match phase, winner, and a crosshair in first person. Ammo, scores,
and timers are not displayed because the multiplayer session does not yet
provide those values.

The user-supplied teal and orange soldier GLBs provide the remote player
visuals; the local player sees the supplied animated arms-and-weapon GLB.
Both teams use the same first-person viewmodel. It plays draw, fire, and
reload animations, but these are visual only: there is no firing or ammo
logic yet. The earlier team-colored static weapon GLBs remain in the project
but are not displayed. Capsules remain as collision shapes. The soldiers have
no skeletons or animations and remain in their static pose.

Godot's import postprocessor generates missing normals and tangents for the
static team models without modifying the source GLBs. Those rebuilt meshes
have no generated LODs or optimized shadow meshes; the animated viewmodel and
overall performance have not been benchmarked.
Movement is host authoritative, with no client prediction yet. Walls/cover
block movement through Godot physics; there is no jump, crouch, or gravity
simulation. The fire signal is an integration hook, not a weapon.

## Teammate interfaces

`FireteamSession` methods available to Godot:

- `host_game(port)` / `join_game(address, port)` / `stop_game()`.
- `set_input(move_x, move_z, yaw, pitch, fire)`; normalized movement, angles
  in radians. Positive yaw rotates toward +X, positive pitch looks up.
- `get_state()` returns `local_player_id`, `port`, `host`, `active`,
  `connected`, `phase` (0 Lobby, 1 InGame, 2 MatchOver), `winner_id`, and
  two player dictionaries (`id`, `position`, `yaw`, `pitch`, `health`,
  `max_health`). Positions are feet in metres; player ID zero is an empty slot.
- `damage_player(player_id, amount)` is host-only. Khushal's weapon handler
  must first raycast from authoritative player aim in the host's physics
  world, then apply damage. Do not accept client-supplied hits or damage.
- `restart_match()` is host-only.
- Signals: `fire_requested(player_id)` on the host and `session_error(message)`
  for explicit connection/validation failures. Errors are also logged.

The child soldier visuals follow the replicated body position and yaw.
Godot camera yaw uses the negative of core yaw.
Core physics/combat callbacks are configured by the extension; the core does
not import Godot headers.

## Integration validation

```sh
<godot> --headless --path godot --script res://tests/session_smoke.gd
python3 tests/test_godot_processes.py "<godot>"
```

The first script loads the native class, connects two sessions, verifies IDs,
movement, real Godot wall collision, replicated transforms, the fire hook,
host-applied damage, winner, rematch, and disconnect.
The Python runner first imports assets and requires explicit PASS markers
without script errors. It runs the native-session script and a real-arena
keyboard/mouse/camera/menu test, including both local perspectives, animation
triggers and bone motion, and model lighting data, then launches two independent Godot processes
and verifies input, snapshots, damage/result, rematch, and disconnect over loopback.
These test-only damage calls prove the combat API, not production hit detection.
CI builds/tests the extension on Linux, macOS, and Windows and runs core
ASan/LSan separately. Remote CI results require publishing the branch.

Koushik's render QA still works via `-- --view overview`, `-- --bench`, or
`-- --inspect` for the free-fly camera. Render screenshots need a GPU; the
headless integration tests do not claim visual or two-computer LAN validation.

## Animated viewmodel attribution

The supplied `fennec_animated_viewmodel.glb` identifies its source as
["[MW: 2019] Fennec (Viper's GMod MW Base)" by VuckyZ](https://sketchfab.com/3d-models/mw-2019-fennec-vipers-gmod-mw-base-d34cd93090b0409382790b3c63818bba),
with [CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/)
in its embedded metadata. The user confirmed permission to use and share this
model and its animations. The original GLB is included unchanged; the game
positions it and plays selected animations. This asset's license and any
underlying rights must be respected when distributing the project.
