#include "fireteam/GameWorld.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace fireteam {

void GameWorld::addEntity(std::unique_ptr<Entity> entity) {
  if (!entity) {
    throw std::invalid_argument("GameWorld cannot own a null entity");
  }
  entities_.push_back(std::move(entity));
}

void GameWorld::update(float deltaSeconds) {
  if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0f) {
    throw std::invalid_argument("deltaSeconds must be finite and non-negative");
  }

  for (const auto& entity : entities_) {
    if (entity->isAlive()) {
      entity->update(deltaSeconds);
    }
  }

  // Defer removal until iteration finishes, so erasing cannot invalidate it.
  std::erase_if(entities_, [](const auto& entity) {
    return !entity->isAlive();
  });
}

}  // namespace fireteam
