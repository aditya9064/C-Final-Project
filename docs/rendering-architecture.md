# Godot renderer and C++ gameplay

The team selected Godot on November 14, 2026. Instructor acceptance of C++
gameplay through GDExtension was confirmed by the user. This supersedes the
raylib renderer described in the earlier submitted proposal; the proposal
PDF is retained as historical submission evidence.

- Godot 4.7 owns the arena, materials, scene, camera, and physics.
- The pinned godot-cpp 4.5 stable bindings target the backward-compatible
  extension ABI supported by Godot 4.7.
- `FireteamSession` is the registered C++ `Node3D` bridge. It creates capsule
  collision bodies and resolves host movement through `move_and_collide`.
- `MatchSession` owns authoritative player/match state and the ENet transport
  imported from PR #17. A collision callback and fire callback keep the core
  independently testable.
- Godot's built-in multiplayer/RPC transport is deliberately not used:
  keeping the reviewed C++ ENet transport avoids two competing network stacks.
- Client packets contain input only. The host computes motion, resolves
  collision, applies combat damage, and publishes snapshots. This scoped
  prototype supports one host plus one client, not arbitrary player counts.
- The application UI and first-person camera are in `godot/scripts/game.gd`.
  Koushik's arena builder remains unchanged, including inspection/QA modes.
- Khushal must connect 3D weapons to `fire_requested(player_id)` and call
  `damage_player(target_id, damage)` after host-only hit detection. The signal
  alone is not a working weapon.

See [build, test, and integration instructions](../godot/cpp/README.md).
LAN ports must be permitted by both computers' firewalls; port forwarding,
internet matchmaking, prediction, bots, and grenades are outside this prototype.
