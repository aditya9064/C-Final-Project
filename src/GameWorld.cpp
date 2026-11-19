#include "fireteam/GameWorld.hpp"
#include "fireteam/Time.hpp"

#include <algorithm>
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
  requireValidDt(deltaSeconds);

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
