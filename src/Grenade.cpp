#include "fireteam/Grenade.hpp"

namespace fireteam {

Grenade::Grenade(int damage, float range, float cooldownSeconds,
                 float blastRadius)
    : Weapon(damage, range, cooldownSeconds) {
  // A negative radius makes no sense, so fix it to 0 like the other stats.
  if (blastRadius < 0.0f) {
    blastRadius = 0.0f;
  }
  blastRadius_ = blastRadius;
}

Shot Grenade::makeShot(Vector3 origin, Vector3 direction) const {
  Shot shot;
  shot.kind = ShotKind::Grenade;
  shot.origin = origin;
  shot.direction = direction;
  shot.range = range();
  shot.damage = damage();
  shot.blastRadius = blastRadius_;
  return shot;
}

}  // namespace fireteam
