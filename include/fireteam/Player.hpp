// Player: a human-controlled Entity (issue #6 -- Koushik's lane).
//
// This is the ENGINE-AGNOSTIC gameplay model: movement intent, planar
// velocity, team, ammo/reload, facing, and the respawn timer all live here in
// plain C++20 so they are unit-tested with doctest and reused unchanged inside
// the Godot client (a thin `PlayerBody : CharacterBody3D` adapter owns one of
// these and drives it each physics frame).
//
// Deliberately NOT in this class (teammates' lanes -- do not implement here):
//   * hit detection / damage application -> Weapon/Rifle, issues #8/#9 (Khushal).
//     Player IS-A Entity, so `Weapon::fire(Entity&)` already targets it.
//   * network replication / sync         -> NetworkManager, #10/#11 (Aditya).
//     Player exposes a Snapshot seam (below); the owner feeds/reads it.
#pragma once

#include "fireteam/Entity.hpp"
#include "fireteam/Vector2.hpp"

namespace fireteam {

// Which side of the arena the player fights for. Teal spawns at -X, orange at
// +X (matches build_arena.gd). Drives character model + spawn point, nothing else.
enum class Team { Teal, Orange };

// A flat, trivially-copyable view of a player's authoritative state. This is
// the SEAM the networking layer (#10/#11) will serialize; it is not the wire
// format itself. Positions are world X/Z metres (Y/gravity is the Godot
// adapter's business, so it is not in the shared model).
struct PlayerSnapshot {
  float x = 0.0f;
  float z = 0.0f;
  float yaw = 0.0f;   // facing, radians
  int health = 0;
  int ammo = 0;
  Team team = Team::Teal;
  bool firing = false;
};

class Player : public Entity {
 public:
  // Tunables (metres/second, rounds, seconds). Public so the Godot adapter and
  // the HUD can show them without magic numbers drifting between layers.
  static constexpr float kWalkSpeed = 4.0f;
  static constexpr float kRunSpeed = 7.0f;
  static constexpr int kMagazineSize = 30;
  static constexpr float kReloadSeconds = 2.0f;
  static constexpr float kRespawnSeconds = 5.0f;
  static constexpr int kMaxHealth = 100;

  Player(Team team, Vector2 spawn);

  // ---- input intent (set by the adapter from keyboard/mouse, or by the
  // network seam for a remote player) ----
  // `planarDir` is a world X/Z direction; it is normalized internally, so its
  // magnitude does not matter and a zero vector means "stand still".
  void setMoveIntent(const Vector2& planarDir);
  void setRunning(bool running);
  void setYaw(float radians) { yaw_ = radians; }

  // ---- simulation ----
  // Advances reload/respawn timers and recomputes `velocity()` from the current
  // intent. Also integrates position so the pure model is meaningful on its own
  // (headless tests, server simulation); the Godot adapter overwrites position
  // with the post-collision result via setPosition() after move_and_slide().
  void update(float deltaSeconds) override;

  const Vector2& velocity() const { return velocity_; }
  float speed() const { return running_ ? kRunSpeed : kWalkSpeed; }
  float yaw() const { return yaw_; }
  Team team() const { return team_; }

  // ---- weapon ammo (firing INPUT only; damage is #8/#9) ----
  int ammo() const { return ammo_; }
  int magazineSize() const { return kMagazineSize; }
  bool isReloading() const { return reloading_; }
  float reloadProgress() const;  // 0..1, 1 when not reloading
  bool canFire() const;
  // Consumes one round if canFire(). Returns true if a shot left the muzzle.
  // Does NOT compute a hit -- the weapon/combat system does that.
  bool fire();
  void reload();

  // ---- respawn ----
  // Normally you do NOT call this: update() detects health reaching 0 and calls
  // it automatically, so the combat system only needs applyDamage(). Exposed
  // (and idempotent) for explicit use. Starts the respawn countdown and freezes
  // movement; health is NOT reset (Entity has no heal) -- the owner recreates /
  // relocates a full-health player when readyToRespawn() becomes true.
  void onKilled();
  bool awaitingRespawn() const { return respawnLeft_ > 0.0f; }
  float respawnRemaining() const { return respawnLeft_ > 0.0f ? respawnLeft_ : 0.0f; }
  // True once the countdown has elapsed while still dead: the owner should now
  // put a fresh full-health player back at a spawn point.
  bool readyToRespawn() const { return !isAlive() && respawnLeft_ <= 0.0f && killed_; }

  // ---- network seam (local test data now; #10/#11 later) ----
  PlayerSnapshot snapshot() const;
  // Apply a remote player's authoritative state (position/facing/ammo/firing).
  // Health is read-only on Entity, so remote damage still flows through
  // applyDamage(); this only moves/aims the proxy.
  void applyRemoteState(const PlayerSnapshot& s);

 private:
  Team team_;
  Vector2 moveDir_{};      // raw intent, pre-normalize
  Vector2 velocity_{};     // computed planar velocity (units/sec)
  float yaw_ = 0.0f;
  bool running_ = false;

  int ammo_ = kMagazineSize;
  bool reloading_ = false;
  float reloadLeft_ = 0.0f;

  bool killed_ = false;
  float respawnLeft_ = 0.0f;
};

}  // namespace fireteam
