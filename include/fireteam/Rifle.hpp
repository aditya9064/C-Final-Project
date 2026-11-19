// Rifle: the basic hitscan weapon.
//
// Hitscan means there's no bullet flying through the world. The combat
// code checks the Shot right away, on the same frame it's fired.
// (A Grenade, later, will spawn a Projectile entity instead.)
//
// The Shot is a 3D line: it starts at origin and goes along direction
// for up to range metres. The combat code decides what it hits.
#pragma once

#include "fireteam/Weapon.hpp"

namespace fireteam {

// Default rifle stats. Distances are in metres to match the 3D arena
// (42 x 34 m, so 100 m reaches across the whole map).
const int RIFLE_DAMAGE = 25;
const float RIFLE_RANGE = 100.0f;    // metres
const float RIFLE_COOLDOWN = 0.25f;  // seconds between shots

class Rifle : public Weapon {
 public:
  // Rifle() makes a standard rifle. Tests can pass their own numbers.
  explicit Rifle(int damage = RIFLE_DAMAGE, float range = RIFLE_RANGE,
                 float cooldownSeconds = RIFLE_COOLDOWN);

 protected:
  Shot makeShot(Vector3 origin, Vector3 direction) const override;
};

}  // namespace fireteam
