#pragma once

#include <memory>
#include <vector>

#include "fireteam/Entity.hpp"

namespace fireteam {

class GameWorld {
 public:
  void addEntity(std::unique_ptr<Entity> entity);
  void update(float deltaSeconds);

  // The collection cannot be changed by callers; GameWorld retains ownership.
  const std::vector<std::unique_ptr<Entity>>& entities() const {
    return entities_;
  }

 private:
  std::vector<std::unique_ptr<Entity>> entities_;
};

}  // namespace fireteam
