// Grenade: a thrown weapon that explodes after a short fuse.
//
// Unlike the Rifle, a grenade shot isn't resolved right away. fire()
// returns a Shot with kind == ShotKind::Grenade, and the caller creates a
// Projectile from it (see Projectile.hpp). The Projectile flies, bounces,
// and then explodes, damaging everything inside its blast radius.
//
// Cooldown, damage, and range work the same as every Weapon:
//   range  = about how far away (in metres) the grenade first lands
//   damage = damage at the center of the blast
#pragma once

#include "fireteam/Weapon.hpp"

namespace fireteam {

// Default grenade stats (distances in metres).
const int GRENADE_DAMAGE = 100;          // at the center of the blast
const float GRENADE_RANGE = 15.0f;       // metres to the first landing
const float GRENADE_COOLDOWN = 1.0f;     // seconds between throws
const float GRENADE_BLAST_RADIUS = 5.0f;  // metres

class Grenade : public Weapon {
 public:
  explicit Grenade(int damage = GRENADE_DAMAGE, float range = GRENADE_RANGE,
                   float cooldownSeconds = GRENADE_COOLDOWN,
                   float blastRadius = GRENADE_BLAST_RADIUS);

  float blastRadius() const { return blastRadius_; }

 protected:
  Shot makeShot(Vector3 origin, Vector3 direction) const override;

 private:
  float blastRadius_;
};

}  // namespace fireteam
