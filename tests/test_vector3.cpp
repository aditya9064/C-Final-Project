#include <doctest/doctest.h>

#include "fireteam/Vector3.hpp"

using fireteam::Vector3;

TEST_CASE("Vector3 default-constructs to the zero vector") {
  const Vector3 vector;
  CHECK(vector.x == 0.0f);
  CHECK(vector.y == 0.0f);
  CHECK(vector.z == 0.0f);
}

TEST_CASE("Vector3 addition and subtraction preserve their operands") {
  const Vector3 a{1.0f, 2.0f, 3.0f};
  const Vector3 b{3.0f, -4.0f, 2.0f};

  CHECK(a + b == Vector3{4.0f, -2.0f, 5.0f});
  CHECK(a - b == Vector3{-2.0f, 6.0f, 1.0f});
  CHECK(a == Vector3{1.0f, 2.0f, 3.0f});
  CHECK(b == Vector3{3.0f, -4.0f, 2.0f});
}

TEST_CASE("Vector3 compound assignment modifies and returns the vector") {
  Vector3 position{0.0f, 1.0f, 2.0f};
  const Vector3 velocity{10.0f, 5.0f, -2.0f};

  position += velocity * 0.5f;
  CHECK(position == Vector3{5.0f, 3.5f, 1.0f});

  Vector3& result = (position -= Vector3{1.0f, 1.0f, 1.0f});
  CHECK(&result == &position);
  CHECK(position == Vector3{4.0f, 2.5f, 0.0f});

  position *= 2.0f;
  CHECK(position == Vector3{8.0f, 5.0f, 0.0f});
}

TEST_CASE("Vector3 scalar multiplication works from both sides") {
  const Vector3 vector{2.0f, -3.0f, 4.0f};
  CHECK(vector * 2.0f == Vector3{4.0f, -6.0f, 8.0f});
  CHECK(2.0f * vector == Vector3{4.0f, -6.0f, 8.0f});
}

TEST_CASE("Vector3 dot and cross products") {
  const Vector3 xAxis{1.0f, 0.0f, 0.0f};
  const Vector3 yAxis{0.0f, 1.0f, 0.0f};

  CHECK(xAxis.dot(yAxis) == doctest::Approx(0.0f));
  CHECK(Vector3{1.0f, 2.0f, 3.0f}.dot(Vector3{4.0f, 5.0f, 6.0f}) ==
        doctest::Approx(32.0f));
  CHECK(xAxis.cross(yAxis) == Vector3{0.0f, 0.0f, 1.0f});
  CHECK(yAxis.cross(xAxis) == Vector3{0.0f, 0.0f, -1.0f});
}

TEST_CASE("Vector3 length and normalization use all three axes") {
  const Vector3 vector{2.0f, 3.0f, 6.0f};
  CHECK(vector.lengthSquared() == doctest::Approx(49.0f));
  CHECK(vector.length() == doctest::Approx(7.0f));

  const Vector3 unit = vector.normalized();
  CHECK(unit.x == doctest::Approx(2.0f / 7.0f));
  CHECK(unit.y == doctest::Approx(3.0f / 7.0f));
  CHECK(unit.z == doctest::Approx(6.0f / 7.0f));
  CHECK(unit.length() == doctest::Approx(1.0f));
}

TEST_CASE("normalizing the zero Vector3 is safe") {
  CHECK(Vector3{}.normalized() == Vector3{});
}

TEST_CASE("distance between Vector3 points includes vertical distance") {
  CHECK(fireteam::distance(Vector3{0.0f, 0.0f, 0.0f},
                           Vector3{2.0f, 3.0f, 6.0f}) ==
        doctest::Approx(7.0f));
  CHECK(fireteam::distance(Vector3{2.0f, 3.0f, 6.0f},
                           Vector3{0.0f, 0.0f, 0.0f}) ==
        doctest::Approx(7.0f));
}
