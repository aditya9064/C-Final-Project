// Weapon: abstract base class for all weapons (Rifle now, Grenade later).
//
// A Weapon is not an Entity. It doesn't exist in the world by itself;
// a Player owns one (composition), so the player can switch weapons.
//
// The cooldown is the wait time between shots. fire() checks it every
// time, so no weapon can shoot faster than it is supposed to.
#pragma once

#include "fireteam/Entity.hpp"

namespace fireteam {

// The result of trying to fire. enum class keeps the names scoped,
// so we write FireResult::Hit instead of just Hit.
enum class FireResult {
  Hit,         // shot fired and the target took damage
  Missed,      // shot fired but the target took no damage
  OnCooldown,  // fired too soon after the last shot, nothing happened
  Invalid      // bad request (like shooting yourself), nothing happened
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

  // Tries to shoot at target. This checks the rules every weapon shares
  // (no shooting yourself, shooter must be alive, cooldown must be done)
  // and then calls checkHit(), which each weapon writes its own way.
  FireResult fire(const Entity& shooter, Entity& target);

  // Counts the cooldown down by the time since the last frame.
  // Zero or negative time is ignored.
  void update(float deltaSeconds);

  bool canFire() const { return cooldownLeft_ <= 0.0f; }
  int damage() const { return damage_; }
  float range() const { return range_; }
  float cooldownSeconds() const { return cooldownSeconds_; }
  float cooldownLeft() const { return cooldownLeft_; }

 protected:
  // Each weapon decides here if the shot hits, and applies the damage.
  // Returns true on a hit. Pure virtual (= 0), so Weapon itself can't be
  // created, only subclasses like Rifle.
  virtual bool checkHit(const Entity& shooter, Entity& target) = 0;

 private:
  int damage_;
  float range_;
  float cooldownSeconds_;
  float cooldownLeft_ = 0.0f;  // 0 means ready to fire
};

}  // namespace fireteam
