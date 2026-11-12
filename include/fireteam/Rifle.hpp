// Rifle: the basic hitscan weapon.
//
// Hitscan means there's no bullet flying through the world. The shot is
// checked right away, on the same frame it's fired.
//
// For now a hit only depends on distance: if the target is within range
// of the shooter, it gets hit. Which way the shooter is aiming is ignored.
#pragma once

#include "fireteam/Weapon.hpp"

namespace fireteam {

// Default rifle stats.
const int RIFLE_DAMAGE = 25;
const float RIFLE_RANGE = 300.0f;    // world units
const float RIFLE_COOLDOWN = 0.25f;  // seconds between shots

class Rifle : public Weapon {
 public:
  // Rifle() makes a standard rifle. Tests can pass their own numbers.
  explicit Rifle(int damage = RIFLE_DAMAGE, float range = RIFLE_RANGE,
                 float cooldownSeconds = RIFLE_COOLDOWN);

 protected:
  bool checkHit(const Entity& shooter, Entity& target) override;
};

}  // namespace fireteam
