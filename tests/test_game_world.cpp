#include <doctest/doctest.h>

#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

#include "fireteam/GameWorld.hpp"

namespace {

class WorldTestEntity : public fireteam::Entity {
 public:
  explicit WorldTestEntity(int& destructions)
      : Entity({}, 100), destructions_(destructions) {}

  ~WorldTestEntity() override { ++destructions_; }

  void update(float dt) override {
    ++updates;
    position_.x += 10.0f * dt;
    if (dieOnUpdate) {
      applyDamage(100);
    }
  }

  int updates = 0;
  bool dieOnUpdate = false;

 private:
  int& destructions_;
};

}  // namespace

TEST_CASE("Empty GameWorld accepts a valid update") {
  fireteam::GameWorld world;
  CHECK_NOTHROW(world.update(0.5f));
  CHECK(world.entities().empty());
}

TEST_CASE("GameWorld owns entities and dispatches updates") {
  int destructions = 0;
  {
    fireteam::GameWorld world;
    auto entity = std::make_unique<WorldTestEntity>(destructions);
    auto* view = entity.get();
    world.addEntity(std::move(entity));
    CHECK(entity == nullptr);
    REQUIRE(world.entities().size() == 1);
    world.update(0.5f);
    CHECK(view->updates == 1);
    CHECK(view->position().x == doctest::Approx(5.0f));
    CHECK(destructions == 0);
  }
  CHECK(destructions == 1);
}

TEST_CASE("GameWorld removes already dead entities without updating them") {
  int destructions = 0;
  fireteam::GameWorld world;
  auto entity = std::make_unique<WorldTestEntity>(destructions);
  entity->applyDamage(100);
  world.addEntity(std::move(entity));
  world.update(0.0f);
  CHECK(world.entities().empty());
  CHECK(destructions == 1);
}

TEST_CASE("GameWorld removes entities dying during update and retains survivors") {
  int destructions = 0;
  fireteam::GameWorld world;
  auto dying = std::make_unique<WorldTestEntity>(destructions);
  dying->dieOnUpdate = true;
  auto survivor = std::make_unique<WorldTestEntity>(destructions);
  auto* survivorView = survivor.get();
  world.addEntity(std::move(dying));
  world.addEntity(std::move(survivor));
  world.update(0.5f);
  REQUIRE(world.entities().size() == 1);
  CHECK(world.entities().front().get() == survivorView);
  CHECK(survivorView->updates == 1);
  CHECK(destructions == 1);
  world.update(0.5f);
  CHECK(survivorView->updates == 2);
}

TEST_CASE("GameWorld rejects null ownership") {
  fireteam::GameWorld world;
  CHECK_THROWS_AS(world.addEntity(nullptr), std::invalid_argument);
  CHECK(world.entities().empty());
}

TEST_CASE("GameWorld rejects invalid time before updating entities") {
  int destructions = 0;
  fireteam::GameWorld world;
  auto entity = std::make_unique<WorldTestEntity>(destructions);
  auto* view = entity.get();
  world.addEntity(std::move(entity));
  for (float dt : {-1.0f, std::numeric_limits<float>::infinity(),
                   std::numeric_limits<float>::quiet_NaN()}) {
    CHECK_THROWS_AS(world.update(dt), std::invalid_argument);
    CHECK(view->updates == 0);
  }
  CHECK_NOTHROW(world.update(0.0f));
  CHECK(view->updates == 1);
}
