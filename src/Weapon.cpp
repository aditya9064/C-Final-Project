#include "fireteam/Weapon.hpp"

#include <algorithm>

namespace fireteam {

Weapon::Weapon(int damage, float range, float cooldownSeconds)
    : damage_(std::max(1, damage)),
      range_(std::max(0.0f, range)),
      cooldownSeconds_(std::max(0.0f, cooldownSeconds)) {}

FireResult Weapon::fire(const Entity& shooter, Entity& target) {
  // Same address means it's the same object, so the player is
  // trying to shoot themselves.
  if (&shooter == &target) {
    return FireResult::Invalid;
  }
  if (!shooter.isAlive()) {
    return FireResult::Invalid;
  }
  // Still waiting on the last shot. Return without touching the
  // cooldown so it doesn't get reset or made longer.
  if (!canFire()) {
    return FireResult::OnCooldown;
  }

  // The shot happens now, so start the cooldown, even if it misses.
  cooldownLeft_ = cooldownSeconds_;

  // A dead target can't take damage, so count it as a miss.
  if (!target.isAlive()) {
    return FireResult::Missed;
  }

  if (checkHit(shooter, target)) {
    return FireResult::Hit;
  }
  return FireResult::Missed;
}

void Weapon::update(float deltaSeconds) {
  if (deltaSeconds <= 0.0f) {
    return;
  }
  cooldownLeft_ = cooldownLeft_ - deltaSeconds;
  if (cooldownLeft_ < 0.0f) {
    cooldownLeft_ = 0.0f;
  }
}

}  // namespace fireteam
