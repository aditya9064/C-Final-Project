#include "fireteam/Weapon.hpp"

#include <algorithm>

#include "fireteam/Time.hpp"

namespace fireteam {

Weapon::Weapon(int damage, float range, float cooldownSeconds)
    : damage_(std::max(1, damage)),
      range_(std::max(0.0f, range)),
      cooldownSeconds_(std::max(0.0f, cooldownSeconds)) {}

bool Weapon::fire(Vector3 origin, Vector3 aimDirection, Shot& shot) {
  // Still waiting on the last shot. Return without touching the
  // cooldown so it doesn't get reset or made longer.
  if (!canFire()) {
    return false;
  }

  // The shot happens now, so start the cooldown.
  cooldownLeft_ = cooldownSeconds_;

  // normalized() turns the aim into a unit vector. A (0, 0, 0) aim stays
  // (0, 0, 0) instead of dividing by zero.
  shot = makeShot(origin, aimDirection.normalized());
  return true;
}

void Weapon::update(float deltaSeconds) {
  // Throws on negative, NaN, or infinite dt, before the cooldown is
  // touched, so a bad dt can never get stored in cooldownLeft_.
  requireValidDt(deltaSeconds);

  cooldownLeft_ = cooldownLeft_ - deltaSeconds;
  if (cooldownLeft_ < 0.0f) {
    cooldownLeft_ = 0.0f;
  }
}

}  // namespace fireteam
