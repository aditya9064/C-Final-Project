#include <doctest/doctest.h>

#include <memory>
#include <type_traits>
#include <vector>

#include "fireteam/Entity.hpp"

using fireteam::Entity;
using fireteam::Vector2;

namespace {

// Entity is abstract, so tests need a minimal concrete subclass. This is
// also how Player/Bot will look: override update(), inherit everything else.
class TestEntity : public Entity {
 public:
  TestEntity(Vector2 position, int maxHealth) : Entity(position, maxHealth) {}

  // `override` makes the compiler verify this really overrides a virtual in
  // the base. A typo in the name/signature would otherwise silently create
  // a brand-new unrelated method.
  void update(float deltaSeconds) override {
    position_ += velocity * deltaSeconds;
    ++updateCount;
  }

  Vector2 velocity;
  int updateCount = 0;
};

}  // namespace

TEST_CASE("Entity is abstract and cannot be instantiated directly") {
  // Compile-time check: proves the pure virtual `update()` does its job.
  static_assert(std::is_abstract_v<Entity>);
  static_assert(!std::is_abstract_v<TestEntity>);
}

TEST_CASE("Entity starts alive at full health with the given position") {
  TestEntity e{Vector2{10.0f, 20.0f}, 100};
  CHECK(e.health() == 100);
  CHECK(e.maxHealth() == 100);
  CHECK(e.isAlive());
  CHECK(e.position() == Vector2{10.0f, 20.0f});
}

TEST_CASE("applyDamage reduces health (normal use)") {
  TestEntity e{Vector2{}, 100};
  e.applyDamage(30);
  CHECK(e.health() == 70);
  CHECK(e.isAlive());

  e.applyDamage(30);
  CHECK(e.health() == 40);
}

TEST_CASE("Damage exceeding remaining health clamps to exactly 0 (boundary)") {
  TestEntity e{Vector2{}, 50};
  e.applyDamage(999);
  CHECK(e.health() == 0);
  CHECK_FALSE(e.isAlive());

  // Further damage on a dead entity stays at 0, never negative.
  e.applyDamage(10);
  CHECK(e.health() == 0);
}

TEST_CASE("Damage exactly equal to remaining health kills (boundary)") {
  TestEntity e{Vector2{}, 50};
  e.applyDamage(50);
  CHECK(e.health() == 0);
  CHECK_FALSE(e.isAlive());
}

TEST_CASE("Zero or negative damage is ignored (invalid input)") {
  TestEntity e{Vector2{}, 100};
  e.applyDamage(0);
  CHECK(e.health() == 100);

  // Negative damage must NOT act as healing.
  e.applyDamage(-25);
  CHECK(e.health() == 100);
}

TEST_CASE("Non-positive max health is clamped so the entity is alive (invalid input)") {
  TestEntity zero{Vector2{}, 0};
  CHECK(zero.isAlive());
  CHECK(zero.health() == 1);

  TestEntity negative{Vector2{}, -40};
  CHECK(negative.isAlive());
  CHECK(negative.health() == 1);
}

TEST_CASE("update() is dispatched polymorphically through Entity pointers") {
  // This is the exact ownership pattern GameWorld will use: a vector of
  // unique_ptr<Entity> that destroys each entity automatically when the
  // vector goes out of scope (via Entity's virtual destructor).
  std::vector<std::unique_ptr<Entity>> entities;

  auto mover = std::make_unique<TestEntity>(Vector2{0.0f, 0.0f}, 100);
  mover->velocity = Vector2{2.0f, 0.0f};
  TestEntity* raw = mover.get();  // Non-owning view for assertions.
  entities.push_back(std::move(mover));

  for (auto& e : entities) {
    e->update(0.5f);  // Called through Entity*, runs TestEntity::update.
  }

  CHECK(raw->updateCount == 1);
  CHECK(raw->position() == Vector2{1.0f, 0.0f});
}
