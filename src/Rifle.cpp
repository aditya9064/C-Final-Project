#include "fireteam/Rifle.hpp"

namespace fireteam {

Rifle::Rifle(int damage, float range, float cooldownSeconds)
    : Weapon(damage, range, cooldownSeconds) {}

Shot Rifle::makeShot(Vector3 origin, Vector3 direction) const {
  Shot shot;
  shot.origin = origin;
  shot.direction = direction;
  shot.range = range();
  shot.damage = damage();
  return shot;
}

}  // namespace fireteam
