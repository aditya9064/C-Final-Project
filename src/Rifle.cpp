#include "fireteam/Rifle.hpp"

namespace fireteam {

Rifle::Rifle(int damage, float range, float cooldownSeconds)
    : Weapon(damage, range, cooldownSeconds) {}

bool Rifle::checkHit(const Entity& shooter, Entity& target) {
  float dist = distance(shooter.position(), target.position());

  // A target exactly at max range still counts as a hit.
  if (dist > range()) {
    return false;
  }
  target.applyDamage(damage());
  return true;
}

}  // namespace fireteam
