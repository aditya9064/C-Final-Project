#include <doctest/doctest.h>

#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>

#include "fireteam/Rifle.hpp"
#include "fireteam/Weapon.hpp"

using namespace fireteam;

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

TEST_CASE("Firing a ready rifle makes a shot with the rifle's stats") {
  Rifle rifle(25, 100.0f, 0.5f);
  Shot shot;

  CHECK(rifle.fire(Vector3(10, 0, 20), Vector3(1, 0, 0), shot));
  CHECK(shot.origin == Vector3(10, 0, 20));
  CHECK(shot.direction == Vector3(1, 0, 0));
  CHECK(shot.range == 100.0f);
  CHECK(shot.damage == 25);
  CHECK_FALSE(rifle.canFire());  // cooldown started
}

TEST_CASE("The aim direction in the shot is made unit length") {
  Rifle rifle(25, 100.0f, 0.0f);
  Shot shot;

  REQUIRE(rifle.fire(Vector3(0, 0, 0), Vector3(3, 0, 4), shot));  // length 5
  CHECK(shot.direction.x == doctest::Approx(0.6f));
  CHECK(shot.direction.y == doctest::Approx(0.0f));
  CHECK(shot.direction.z == doctest::Approx(0.8f));
}

TEST_CASE("A shot keeps its height and can aim up or down") {
  // Standing eye height is 1.63 m. Aiming slightly up at something.
  Rifle rifle(25, 100.0f, 0.0f);
  Shot shot;

  REQUIRE(rifle.fire(Vector3(2, 1.63f, 5), Vector3(0, 3, 4), shot));
  CHECK(shot.origin == Vector3(2, 1.63f, 5));
  CHECK(shot.direction.y == doctest::Approx(0.6f));  // going up
  CHECK(shot.direction.z == doctest::Approx(0.8f));
  CHECK(shot.direction.length() == doctest::Approx(1.0f));
}

TEST_CASE("Default rifle range is in metres and reaches across the arena") {
  // The arena is 42 x 34 m, so the corner-to-corner distance is about 54 m.
  CHECK(RIFLE_RANGE >= 54.0f);
  CHECK(RIFLE_RANGE <= 200.0f);  // not the old 300 "pixel" range
}

TEST_CASE("A zero aim direction stays zero instead of dividing by zero") {
  Rifle rifle(25, 100.0f, 0.0f);
  Shot shot;

  CHECK(rifle.fire(Vector3(0, 0, 0), Vector3(0, 0, 0), shot));
  CHECK(shot.direction == Vector3(0, 0, 0));
}

TEST_CASE("Rifle works through a unique_ptr<Weapon>") {
  // This is how a Player will hold its weapon.
  std::unique_ptr<Weapon> weapon = std::make_unique<Rifle>(30, 100.0f, 0.5f);
  Shot shot;

  CHECK(weapon->fire(Vector3(0, 0, 0), Vector3(0, 0, 1), shot));
  CHECK(shot.damage == 30);
}

// ---------- Cooldown boundary ----------
// Times like 0.25 and 0.5 are exact in floating point, so these
// checks won't fail because of rounding.

TEST_CASE("Firing again before the cooldown is done is rejected") {
  Rifle rifle(25, 100.0f, 0.5f);
  Shot shot;

  REQUIRE(rifle.fire(Vector3(0, 0, 0), Vector3(1, 0, 0), shot));
  CHECK_FALSE(rifle.fire(Vector3(0, 0, 0), Vector3(1, 0, 0), shot));

  rifle.update(0.25f);  // only half the cooldown
  CHECK_FALSE(rifle.fire(Vector3(0, 0, 0), Vector3(1, 0, 0), shot));
}

TEST_CASE("Firing exactly when the cooldown ends works") {
  Rifle rifle(25, 100.0f, 0.5f);
  Shot shot;

  REQUIRE(rifle.fire(Vector3(0, 0, 0), Vector3(1, 0, 0), shot));
  rifle.update(0.25f);
  rifle.update(0.25f);  // 0.5 total, exactly the cooldown

  CHECK(rifle.cooldownLeft() == 0.0f);
  CHECK(rifle.fire(Vector3(0, 0, 0), Vector3(1, 0, 0), shot));
}

TEST_CASE("A rejected shot does not reset or extend the cooldown") {
  Rifle rifle(25, 100.0f, 0.5f);
  Shot shot;

  REQUIRE(rifle.fire(Vector3(0, 0, 0), Vector3(1, 0, 0), shot));
  rifle.update(0.25f);
  REQUIRE(rifle.cooldownLeft() == 0.25f);

  // Spam the trigger while on cooldown.
  CHECK_FALSE(rifle.fire(Vector3(0, 0, 0), Vector3(1, 0, 0), shot));
  CHECK_FALSE(rifle.fire(Vector3(0, 0, 0), Vector3(1, 0, 0), shot));
  CHECK_FALSE(rifle.fire(Vector3(0, 0, 0), Vector3(1, 0, 0), shot));

  // Still 0.25 left. If a rejected shot reset the cooldown it would be
  // back at 0.5.
  CHECK(rifle.cooldownLeft() == 0.25f);

  // So only the original 0.5 seconds total is needed.
  rifle.update(0.25f);
  CHECK(rifle.fire(Vector3(0, 0, 0), Vector3(1, 0, 0), shot));
}

TEST_CASE("A rejected shot leaves the Shot variable alone") {
  Rifle rifle(25, 100.0f, 0.5f);
  Shot shot;

  REQUIRE(rifle.fire(Vector3(1, 0, 1), Vector3(1, 0, 0), shot));
  CHECK_FALSE(rifle.fire(Vector3(99, 0, 99), Vector3(0, 0, 1), shot));

  // Still the first shot, not the rejected one.
  CHECK(shot.origin == Vector3(1, 0, 1));
  CHECK(shot.direction == Vector3(1, 0, 0));
}

TEST_CASE("A big time step sets the cooldown to 0, not negative") {
  Rifle rifle(25, 100.0f, 0.5f);
  Shot shot;

  REQUIRE(rifle.fire(Vector3(0, 0, 0), Vector3(1, 0, 0), shot));
  rifle.update(10.0f);
  CHECK(rifle.cooldownLeft() == 0.0f);

  // Waiting a long time doesn't save up extra shots.
  CHECK(rifle.fire(Vector3(0, 0, 0), Vector3(1, 0, 0), shot));
  CHECK_FALSE(rifle.fire(Vector3(0, 0, 0), Vector3(1, 0, 0), shot));
}

// ---------- Invalid input ----------

TEST_CASE("Zero update time is allowed and changes nothing") {
  Rifle rifle(25, 100.0f, 0.5f);
  Shot shot;
  REQUIRE(rifle.fire(Vector3(0, 0, 0), Vector3(1, 0, 0), shot));

  CHECK_NOTHROW(rifle.update(0.0f));
  CHECK(rifle.cooldownLeft() == 0.5f);
}

TEST_CASE("Negative, NaN, or infinite update time throws") {
  Rifle rifle(25, 100.0f, 0.5f);
  Shot shot;
  REQUIRE(rifle.fire(Vector3(0, 0, 0), Vector3(1, 0, 0), shot));

  float nan = std::numeric_limits<float>::quiet_NaN();
  float inf = std::numeric_limits<float>::infinity();

  CHECK_THROWS_AS(rifle.update(-1.0f), std::invalid_argument);
  CHECK_THROWS_AS(rifle.update(nan), std::invalid_argument);
  CHECK_THROWS_AS(rifle.update(inf), std::invalid_argument);
  CHECK_THROWS_AS(rifle.update(-inf), std::invalid_argument);

  // None of the bad values got into the cooldown.
  CHECK(rifle.cooldownLeft() == 0.5f);
}

TEST_CASE("Weapon isn't stuck after someone passes it a NaN dt") {
  // If NaN ever got stored in the cooldown, canFire() would be false
  // forever. Check that normal updates still work afterwards.
  Rifle rifle(25, 100.0f, 0.5f);
  Shot shot;
  REQUIRE(rifle.fire(Vector3(0, 0, 0), Vector3(1, 0, 0), shot));

  CHECK_THROWS(rifle.update(std::numeric_limits<float>::quiet_NaN()));
  rifle.update(0.5f);
  CHECK(rifle.canFire());
  CHECK(rifle.fire(Vector3(0, 0, 0), Vector3(1, 0, 0), shot));
}

TEST_CASE("Bad constructor values get fixed") {
  Rifle rifle(-5, -100.0f, -1.0f);
  CHECK(rifle.damage() == 1);
  CHECK(rifle.range() == 0.0f);
  CHECK(rifle.cooldownSeconds() == 0.0f);

  // The fixed values are what end up in the shot.
  Shot shot;
  REQUIRE(rifle.fire(Vector3(0, 0, 0), Vector3(1, 0, 0), shot));
  CHECK(shot.damage == 1);
  CHECK(shot.range == 0.0f);
}
