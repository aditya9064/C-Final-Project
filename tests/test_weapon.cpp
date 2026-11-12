#include <doctest/doctest.h>

#include <memory>
#include <type_traits>

#include "fireteam/Rifle.hpp"
#include "fireteam/Weapon.hpp"

using namespace fireteam;

// Entity is abstract, so we need a simple subclass to shoot at.
// The unnamed namespace keeps Dummy private to this file, so it can't
// clash with a test class of the same name in another test file.
namespace {
class Dummy : public Entity {
 public:
  Dummy(Vector2 position, int maxHealth) : Entity(position, maxHealth) {}
  void update(float) override {}
};
}  // namespace

TEST_CASE("Weapon is abstract and Rifle is a Weapon") {
  CHECK(std::is_abstract_v<Weapon>);
  CHECK_FALSE(std::is_abstract_v<Rifle>);
  CHECK(std::is_base_of_v<Weapon, Rifle>);
}

TEST_CASE("Default rifle has the default stats and is ready to fire") {
  Rifle rifle;
  CHECK(rifle.damage() == RIFLE_DAMAGE);
  CHECK(rifle.range() == RIFLE_RANGE);
  CHECK(rifle.cooldownSeconds() == RIFLE_COOLDOWN);
  CHECK(rifle.canFire());
}

// ---------- Normal firing ----------

TEST_CASE("Rifle hits a target in range") {
  Rifle rifle(25, 100.0f, 0.5f);
  Dummy shooter(Vector2(0, 0), 100);
  Dummy target(Vector2(30, 40), 100);  // 50 away

  CHECK(rifle.fire(shooter, target) == FireResult::Hit);
  CHECK(target.health() == 75);
  CHECK_FALSE(rifle.canFire());
}

TEST_CASE("Rifle misses a target out of range but still uses the shot") {
  Rifle rifle(25, 100.0f, 0.5f);
  Dummy shooter(Vector2(0, 0), 100);
  Dummy target(Vector2(101, 0), 100);

  CHECK(rifle.fire(shooter, target) == FireResult::Missed);
  CHECK(target.health() == 100);
  CHECK_FALSE(rifle.canFire());
}

TEST_CASE("Aim direction is ignored, anything in range gets hit") {
  Rifle rifle(10, 100.0f, 0.0f);
  Dummy shooter(Vector2(0, 0), 100);
  Dummy inFront(Vector2(50, 0), 100);
  Dummy behind(Vector2(-50, 0), 100);

  CHECK(rifle.fire(shooter, inFront) == FireResult::Hit);
  CHECK(rifle.fire(shooter, behind) == FireResult::Hit);
}

TEST_CASE("Target exactly at max range is hit") {
  Rifle rifle(25, 50.0f, 0.5f);
  Dummy shooter(Vector2(0, 0), 100);
  Dummy target(Vector2(30, 40), 100);  // exactly 50 away

  CHECK(rifle.fire(shooter, target) == FireResult::Hit);
}

TEST_CASE("Hitting a target enough times kills it, health stops at 0") {
  Rifle rifle(40, 100.0f, 0.0f);  // no cooldown
  Dummy shooter(Vector2(0, 0), 100);
  Dummy target(Vector2(10, 0), 100);

  rifle.fire(shooter, target);
  rifle.fire(shooter, target);
  rifle.fire(shooter, target);
  CHECK(target.health() == 0);
  CHECK_FALSE(target.isAlive());
}

TEST_CASE("Rifle works through a unique_ptr<Weapon>") {
  // This is how a Player will hold its weapon.
  std::unique_ptr<Weapon> weapon = std::make_unique<Rifle>(30, 100.0f, 0.5f);
  Dummy shooter(Vector2(0, 0), 100);
  Dummy target(Vector2(10, 0), 100);

  CHECK(weapon->fire(shooter, target) == FireResult::Hit);
  CHECK(target.health() == 70);
}

// ---------- Cooldown boundary ----------
// Times like 0.25 and 0.5 are exact in floating point, so these
// checks won't fail because of rounding.

TEST_CASE("Firing again before the cooldown is done is rejected") {
  Rifle rifle(25, 100.0f, 0.5f);
  Dummy shooter(Vector2(0, 0), 100);
  Dummy target(Vector2(10, 0), 100);

  REQUIRE(rifle.fire(shooter, target) == FireResult::Hit);

  CHECK(rifle.fire(shooter, target) == FireResult::OnCooldown);
  CHECK(target.health() == 75);

  rifle.update(0.25f);  // only half the cooldown
  CHECK(rifle.fire(shooter, target) == FireResult::OnCooldown);
  CHECK(target.health() == 75);
}

TEST_CASE("Firing exactly when the cooldown ends works") {
  Rifle rifle(25, 100.0f, 0.5f);
  Dummy shooter(Vector2(0, 0), 100);
  Dummy target(Vector2(10, 0), 100);

  REQUIRE(rifle.fire(shooter, target) == FireResult::Hit);
  rifle.update(0.25f);
  rifle.update(0.25f);  // 0.5 total, exactly the cooldown

  CHECK(rifle.cooldownLeft() == 0.0f);
  CHECK(rifle.fire(shooter, target) == FireResult::Hit);
  CHECK(target.health() == 50);
}

TEST_CASE("A rejected shot does not reset or extend the cooldown") {
  Rifle rifle(25, 100.0f, 0.5f);
  Dummy shooter(Vector2(0, 0), 100);
  Dummy target(Vector2(10, 0), 100);

  REQUIRE(rifle.fire(shooter, target) == FireResult::Hit);
  rifle.update(0.25f);
  REQUIRE(rifle.cooldownLeft() == 0.25f);

  // Spam the trigger while on cooldown.
  CHECK(rifle.fire(shooter, target) == FireResult::OnCooldown);
  CHECK(rifle.fire(shooter, target) == FireResult::OnCooldown);
  CHECK(rifle.fire(shooter, target) == FireResult::OnCooldown);

  // Still 0.25 left. If a rejected shot reset the cooldown it would be
  // back at 0.5.
  CHECK(rifle.cooldownLeft() == 0.25f);

  // So only the original 0.5 seconds total is needed.
  rifle.update(0.25f);
  CHECK(rifle.fire(shooter, target) == FireResult::Hit);
  CHECK(target.health() == 50);
}

TEST_CASE("A big time step sets the cooldown to 0, not negative") {
  Rifle rifle(25, 100.0f, 0.5f);
  Dummy shooter(Vector2(0, 0), 100);
  Dummy target(Vector2(10, 0), 100);

  REQUIRE(rifle.fire(shooter, target) == FireResult::Hit);
  rifle.update(10.0f);
  CHECK(rifle.cooldownLeft() == 0.0f);

  // Waiting a long time doesn't save up extra shots.
  CHECK(rifle.fire(shooter, target) == FireResult::Hit);
  CHECK(rifle.fire(shooter, target) == FireResult::OnCooldown);
}

// ---------- Invalid input ----------

TEST_CASE("Shooting yourself is invalid and doesn't start the cooldown") {
  Rifle rifle(25, 100.0f, 0.5f);
  Dummy self(Vector2(0, 0), 100);

  CHECK(rifle.fire(self, self) == FireResult::Invalid);
  CHECK(self.health() == 100);
  CHECK(rifle.canFire());
}

TEST_CASE("A dead shooter can't fire") {
  Rifle rifle(25, 100.0f, 0.5f);
  Dummy shooter(Vector2(0, 0), 10);
  Dummy target(Vector2(10, 0), 100);
  shooter.applyDamage(10);

  CHECK(rifle.fire(shooter, target) == FireResult::Invalid);
  CHECK(target.health() == 100);
  CHECK(rifle.canFire());
}

TEST_CASE("Shooting a dead target is a miss") {
  Rifle rifle(25, 100.0f, 0.5f);
  Dummy shooter(Vector2(0, 0), 100);
  Dummy target(Vector2(10, 0), 10);
  target.applyDamage(10);

  CHECK(rifle.fire(shooter, target) == FireResult::Missed);
  CHECK(target.health() == 0);
}

TEST_CASE("Zero or negative update time is ignored") {
  Rifle rifle(25, 100.0f, 0.5f);
  Dummy shooter(Vector2(0, 0), 100);
  Dummy target(Vector2(10, 0), 100);
  REQUIRE(rifle.fire(shooter, target) == FireResult::Hit);

  rifle.update(0.0f);
  CHECK(rifle.cooldownLeft() == 0.5f);

  rifle.update(-1.0f);  // must not add time to the cooldown
  CHECK(rifle.cooldownLeft() == 0.5f);
}

TEST_CASE("Bad constructor values get fixed") {
  Rifle rifle(-5, -100.0f, -1.0f);
  CHECK(rifle.damage() == 1);
  CHECK(rifle.range() == 0.0f);
  CHECK(rifle.cooldownSeconds() == 0.0f);

  // Range 0 means only a target at the exact same spot gets hit.
  Dummy shooter(Vector2(0, 0), 100);
  Dummy sameSpot(Vector2(0, 0), 100);
  Dummy nearby(Vector2(1, 0), 100);
  CHECK(rifle.fire(shooter, sameSpot) == FireResult::Hit);
  CHECK(rifle.fire(shooter, nearby) == FireResult::Missed);
}
