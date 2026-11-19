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
mouse. Click fires; R reloads a partially used magazine. Host stays in the lobby until
one client joins. MatchOver enables the
host's rematch button. Disconnect clears the client's slot and returns the
host to Lobby.

The HUD adapts Koushik's work from PR #20 to show the local team, replicated
health, magazine ammunition, reload state, match phase, winner, and a
crosshair in first person. Scores and timers are not yet displayed.

Koushik's rigged versions of the user-supplied teal and orange soldiers
provide the remote player visuals; the local player sees the supplied
animated arms-and-weapon GLB. The soldiers play idle and jogging animations
based on replicated position changes, with a short hold between snapshots.
Remote visuals interpolate between authoritative positions and yaw, with their
position kept within 0.15 m of the collision body; large jumps snap immediately.
Mouse look updates the local camera as input arrives. This reduces visible
jitter, but does not reduce network round-trip latency or predict movement.
Shots still use authoritative collision bodies, so the smoothed visual and
hitbox may differ slightly.
These are presentation estimates, not replicated animation events: directional
movement and remote firing, reload, and death animations are not implemented.
Both teams use the same first-person viewmodel. It plays draw, fire, and
reload animations when the authoritative state confirms them. The earlier team-colored static weapon GLBs remain in the project
but are not displayed. Capsules remain as collision shapes.

Godot's import postprocessor generates missing normals and tangents for the
older static team models without modifying the source GLBs. The rigged soldier
models use Godot's standard importer to retain their skinning. Overall
performance has not been benchmarked. Two small chest details in each rigged
soldier were reassigned from thumb bones to `spine_03` to prevent them from
following the hands. Other visible rigging artifacts remain during locomotion;
they require further work on the source rig.
Movement is host authoritative, with no client prediction yet. Walls/cover
block movement through Godot physics; there is no jump, crouch, or gravity
simulation.

The rifle uses 25 damage, 300 m range, and a 0.25 s cooldown, adapted from
Khushal's PR #16. Each player has a 30-round magazine and a two-second reload
with unlimited spare ammunition. A host-side ray starts at the player's eye,
uses their requested yaw and pitch, and checks arena geometry and player
collision before applying damage. Misses and blocked shots consume ammunition.
The host controls reloads, cooldowns, damage, and the match result.

## Teammate interfaces

`FireteamSession` methods available to Godot:

- `host_game(port)` / `join_game(address, port)` / `stop_game()`.
- `set_input(move_x, move_z, yaw, pitch, fire, reload = false)`; normalized
  movement, angles in radians. Positive yaw rotates toward +X, positive pitch
  looks up. Fire and reload are one-shot requests.
- `get_state()` returns `local_player_id`, `port`, `host`, `active`,
  `connected`, `phase` (0 Lobby, 1 InGame, 2 MatchOver), `winner_id`, and
  two player dictionaries (`id`, `position`, `yaw`, `pitch`, `health`,
  `max_health`, `ammo`, `max_ammo`, `reloading`, `shots_fired`). Positions are
  feet in metres; player ID zero is an empty slot. The wire protocol is version
  2; both peers must run this version.
- `damage_player(player_id, amount)` is host-only. The combat handler calls it
  only after a host-side raycast; clients never supply hits or damage.
- `restart_match()` is host-only.
- Signals: `shot_fired(player_id, yaw, pitch, damage, range)` and the legacy
  `fire_requested(player_id)` on the host for accepted shots, and
  `session_error(message)` for explicit connection/validation failures.

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
movement, real Godot wall collision, replicated transforms, host/client
shooting, misses, cover, damage, winner, rematch, and disconnect.
The Python runner first imports assets and requires explicit PASS markers
without script errors. It runs the native-session script and a real-arena
keyboard/mouse/camera/menu test, including both local perspectives, animation
triggers and bone motion, and model lighting data, then launches two independent Godot processes
and verifies client input, actual raycast damage/result, rematch, and
disconnect over loopback.
CI builds/tests the extension on Linux, macOS, and Windows and runs core
ASan/LSan separately. Remote CI results require publishing the branch.

Koushik's render QA still works via `-- --view overview`, `-- --bench`, or
`-- --inspect` for the free-fly camera. Render screenshots need a GPU; the
headless integration tests do not claim visual or two-computer LAN validation.

## Asset attribution

The rigged `teal_soldier.glb` and `orange_soldier.glb` were integrated from
Koushik's commit `76e80fd` in PR #20. His player rig identifies the skeleton
and animation clips as the same ones used with his original models. His
character credits identify the
[Quaternius Universal Base Characters](https://quaternius.com/packs/universalbasecharacters.html)
and [Universal Animation Library](https://quaternius.com/packs/universalanimationlibrary.html)
as CC0 sources for that rig and its animations.

### Animated viewmodel

The supplied `fennec_animated_viewmodel.glb` identifies its source as
["[MW: 2019] Fennec (Viper's GMod MW Base)" by VuckyZ](https://sketchfab.com/3d-models/mw-2019-fennec-vipers-gmod-mw-base-d34cd93090b0409382790b3c63818bba),
with [CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/)
in its embedded metadata. The user confirmed permission to use and share this
model and its animations. The original GLB is included unchanged; the game
positions it and plays selected animations. This asset's license and any
underlying rights must be respected when distributing the project.
