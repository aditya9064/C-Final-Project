#include <doctest/doctest.h>

#include <limits>
#include <stdexcept>

#include "fireteam/Rifle.hpp"

TEST_CASE("Rifle enforces shot cooldown, magazine and timed reload") {
  fireteam::Rifle rifle;
  CHECK_FALSE(rifle.reload());
  for (int i = 0; i < 30; ++i) {
    REQUIRE(rifle.fire());
    CHECK_FALSE(rifle.fire());
    rifle.update(0.25f);
  }
  CHECK(rifle.ammo() == 0);
  CHECK_FALSE(rifle.fire());
  REQUIRE(rifle.reload());
  CHECK_FALSE(rifle.reload());
  CHECK_FALSE(rifle.fire());
  rifle.update(1.0f);
  CHECK(rifle.reloading());
  CHECK(rifle.ammo() == 0);
  rifle.update(1.0f);
  CHECK_FALSE(rifle.reloading());
  CHECK(rifle.ammo() == 30);
  CHECK(rifle.fire());
  CHECK(rifle.shotsFired() == 31);
  CHECK_THROWS_AS(rifle.update(-1.0f), std::invalid_argument);
  CHECK_THROWS_AS(rifle.update(std::numeric_limits<float>::quiet_NaN()),
                  std::invalid_argument);
}
