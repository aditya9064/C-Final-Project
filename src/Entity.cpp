#include "fireteam/Entity.hpp"

#include <algorithm>

namespace fireteam {

// A zero/negative max health would make an entity dead on arrival, which is
// never what a caller means; clamp to at least 1 so the invariant
// "a freshly constructed entity is alive" always holds.
Entity::Entity(Vector2 position, int maxHealth)
    : position_(position),
      health_(std::max(1, maxHealth)),
      maxHealth_(std::max(1, maxHealth)) {}

void Entity::applyDamage(int amount) {
  if (amount <= 0) {
    return;  // Invalid input: ignore rather than heal or assert.
  }
  // std::max keeps health from going below 0 even if `amount` exceeds it
  // (the boundary case). Subtracting first then clamping is simpler and
  // less error-prone than checking `amount >= health_` separately.
  health_ = std::max(0, health_ - amount);
}

}  // namespace fireteam
