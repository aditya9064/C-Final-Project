#include "fireteam/Rifle.hpp"

namespace fireteam {

// Damage, range and cadence are adapted from Khushal's rifle in PR #16.
Rifle::Rifle() : Weapon(25, 300.0f, 0.25f, 30, 2.0f) {}

}  // namespace fireteam
