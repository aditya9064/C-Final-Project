#include <doctest/doctest.h>

#include <limits>
#include <stdexcept>

#include "fireteam/Time.hpp"

using fireteam::requireValidDt;

TEST_CASE("requireValidDt accepts zero and positive dt") {
  CHECK_NOTHROW(requireValidDt(0.0f));
  CHECK_NOTHROW(requireValidDt(0.016f));  // about one frame at 60 FPS
  CHECK_NOTHROW(requireValidDt(5.0f));
}

TEST_CASE("requireValidDt rejects negative dt") {
  CHECK_THROWS_AS(requireValidDt(-0.016f), std::invalid_argument);
  CHECK_THROWS_AS(requireValidDt(-1.0f), std::invalid_argument);
}

TEST_CASE("requireValidDt rejects NaN and infinity") {
  float nan = std::numeric_limits<float>::quiet_NaN();
  float inf = std::numeric_limits<float>::infinity();

  CHECK_THROWS_AS(requireValidDt(nan), std::invalid_argument);
  CHECK_THROWS_AS(requireValidDt(inf), std::invalid_argument);
  CHECK_THROWS_AS(requireValidDt(-inf), std::invalid_argument);
}
