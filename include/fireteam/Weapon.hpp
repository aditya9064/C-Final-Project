// Weapon: abstract base class for all weapons (Rifle now, Grenade later).
//
// A Weapon is not an Entity. It doesn't exist in the world by itself;
// a Player owns one (composition), so the player can switch weapons.
//
// A weapon only decides WHEN it can shoot (the cooldown) and WHAT the
// shot looks like (damage, range, direction). It does not decide who
// gets hit. The combat code (#9) takes the Shot and checks it against
// the entities in the world, so the weapon doesn't need to know about
// Player, GameWorld, or any other entity.
#pragma once

#include "fireteam/Vector3.hpp"

namespace fireteam {

// What kind of shot it is, so the caller knows what to do with it:
//   Hitscan -> pass it to resolveShot() right away (Rifle)
//   Grenade -> create a Projectile from it and add it to the world
enum class ShotKind { Hitscan, Grenade };

// Everything the combat code needs to know about one shot.
// A plain struct because it's just data with no rules to protect.
//
// The shot is in 3D (y is up, units are metres). For a rifle the origin
// should be the shooter's eye or gun height, not their feet, so the
// bullet can go over low cover or get blocked by tall cover.
struct Shot {
  ShotKind kind = ShotKind::Hitscan;
  Vector3 origin;     // where the shot starts, in 3D
  Vector3 direction;  // unit-length aim direction, or (0, 0, 0) if no aim
  float range = 0.0f;  // metres
  int damage = 0;     // for a grenade, the damage at the blast center
  float blastRadius = 0.0f;  // only used by grenades
};

class Weapon {
 public:
  // Bad values get fixed instead of crashing: damage becomes at least 1,
  // range and cooldown become at least 0.
  Weapon(int damage, float range, float cooldownSeconds);

  // Virtual destructor so deleting a Rifle through a Weapon pointer
  // (like unique_ptr<Weapon> does) also runs the Rifle destructor.
  virtual ~Weapon() = default;

  // No copying. A Player owns its weapon through a unique_ptr.
  Weapon(const Weapon&) = delete;
  Weapon& operator=(const Weapon&) = delete;

  // Tries to shoot from `origin` toward `aimDirection`.
  // - Ready: starts the cooldown, fills in `shot`, returns true.
  // - Still cooling down: returns false and changes nothing (the cooldown
  //   is not reset or made longer, and `shot` is left alone).
  // The aim direction doesn't need to be unit length; fire() normalizes it.
  // Both are 3D. Use groundToWorld() (Vector3.hpp) to turn an Entity's
  // ground position into a 3D point at the right height.
  bool fire(Vector3 origin, Vector3 aimDirection, Shot& shot);

  // Counts the cooldown down by the time since the last frame.
  // dt == 0 is allowed. Negative, NaN, or infinite dt throws
  // std::invalid_argument (see Time.hpp).
  void update(float deltaSeconds);

  bool canFire() const { return cooldownLeft_ <= 0.0f; }
  int damage() const { return damage_; }
  float range() const { return range_; }
  float cooldownSeconds() const { return cooldownSeconds_; }
  float cooldownLeft() const { return cooldownLeft_; }

 protected:
  // Each kind of weapon builds its own kind of shot. Pure virtual (= 0),
  // so Weapon itself can't be created, only subclasses like Rifle.
  // Only called by fire(), after the cooldown check has passed.
  virtual Shot makeShot(Vector3 origin, Vector3 direction) const = 0;

 private:
  int damage_;
  float range_;
  float cooldownSeconds_;
  float cooldownLeft_ = 0.0f;  // 0 means ready to fire
};

}  // namespace fireteam
