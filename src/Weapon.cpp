#include "fireteam/Weapon.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace fireteam {

Weapon::Weapon(int damage, float range, float cooldownSeconds, int magazineSize,
               float reloadSeconds)
    : damage_(damage),
      range_(range),
      cooldownSeconds_(cooldownSeconds),
      magazineSize_(magazineSize),
      reloadSeconds_(reloadSeconds),
      ammo_(magazineSize) {
  if (damage <= 0 || !std::isfinite(range) || range <= 0.0f ||
      !std::isfinite(cooldownSeconds) || cooldownSeconds < 0.0f ||
      magazineSize <= 0 || !std::isfinite(reloadSeconds) ||
      reloadSeconds <= 0.0f) {
    throw std::invalid_argument("Invalid weapon configuration");
  }
}

bool Weapon::fire() {
  if (ammo_ == 0 || cooldownRemaining_ > 0.0f || reloading()) {
    return false;
  }
  --ammo_;
  ++shotsFired_;
  cooldownRemaining_ = cooldownSeconds_;
  return true;
}

bool Weapon::reload() {
  if (ammo_ == magazineSize_ || reloading()) {
    return false;
  }
  reloadRemaining_ = reloadSeconds_;
  return true;
}

void Weapon::update(float deltaSeconds) {
  if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0f) {
    throw std::invalid_argument("Weapon update time must be finite and non-negative");
  }
  cooldownRemaining_ = std::max(0.0f, cooldownRemaining_ - deltaSeconds);
  if (reloading()) {
    reloadRemaining_ = std::max(0.0f, reloadRemaining_ - deltaSeconds);
    if (!reloading()) {
      ammo_ = magazineSize_;
    }
  }
}

}  // namespace fireteam
