#include <doctest/doctest.h>

#include "fireteam/Vector3.hpp"

using fireteam::Vector2;
using fireteam::Vector3;

TEST_CASE("Vector3 starts at zero") {
  Vector3 v;
  CHECK(v.x == 0.0f);
  CHECK(v.y == 0.0f);
  CHECK(v.z == 0.0f);
}

TEST_CASE("Vector3 add, subtract, and multiply") {
  Vector3 a{1.0f, 2.0f, 3.0f};
  Vector3 b{4.0f, -1.0f, 0.5f};

  CHECK(a + b == Vector3{5.0f, 1.0f, 3.5f});
  CHECK(a - b == Vector3{-3.0f, 3.0f, 2.5f});
  CHECK(a * 2.0f == Vector3{2.0f, 4.0f, 6.0f});
  CHECK(2.0f * a == Vector3{2.0f, 4.0f, 6.0f});

  // a and b are not changed
  CHECK(a == Vector3{1.0f, 2.0f, 3.0f});
  CHECK(b == Vector3{4.0f, -1.0f, 0.5f});
}

TEST_CASE("Vector3 +=, -=, *= change the vector") {
  Vector3 pos{0.0f, 0.0f, 0.0f};
  Vector3 velocity{2.0f, 4.0f, -6.0f};

  pos += velocity * 0.5f;
  CHECK(pos == Vector3{1.0f, 2.0f, -3.0f});

  pos -= Vector3{1.0f, 1.0f, 1.0f};
  CHECK(pos == Vector3{0.0f, 1.0f, -4.0f});

  pos *= 3.0f;
  CHECK(pos == Vector3{0.0f, 3.0f, -12.0f});

  CHECK(pos != Vector3{});
}

TEST_CASE("Vector3 length and distance") {
  Vector3 v{2.0f, 3.0f, 6.0f};  // 4 + 9 + 36 = 49
  CHECK(v.lengthSquared() == doctest::Approx(49.0f));
  CHECK(v.length() == doctest::Approx(7.0f));

  CHECK(distance(Vector3{1.0f, 1.0f, 1.0f}, Vector3{3.0f, 4.0f, 7.0f}) ==
        doctest::Approx(7.0f));
  CHECK(distance(v, v) == doctest::Approx(0.0f));
}

TEST_CASE("Vector3 normalized has length 1") {
  Vector3 n = Vector3{0.0f, 0.0f, 5.0f}.normalized();
  CHECK(n.x == doctest::Approx(0.0f));
  CHECK(n.y == doctest::Approx(0.0f));
  CHECK(n.z == doctest::Approx(1.0f));

  Vector3 m = Vector3{3.0f, 4.0f, 12.0f}.normalized();
  CHECK(m.length() == doctest::Approx(1.0f));
}

TEST_CASE("Vector3 normalized zero vector stays zero (no divide by zero)") {
  Vector3 n = Vector3{}.normalized();
  CHECK(n == Vector3{});
}

TEST_CASE("dot product") {
  Vector3 a{1.0f, 2.0f, 3.0f};
  Vector3 b{4.0f, 5.0f, 6.0f};
  CHECK(dot(a, b) == doctest::Approx(32.0f));  // 4 + 10 + 18

  // Straight up and straight forward are at 90 degrees, so dot is 0.
  CHECK(dot(Vector3{0.0f, 1.0f, 0.0f}, Vector3{0.0f, 0.0f, 1.0f}) ==
        doctest::Approx(0.0f));

  // How far (3, 7, 4) goes along the x direction.
  CHECK(dot(Vector3{3.0f, 7.0f, 4.0f}, Vector3{1.0f, 0.0f, 0.0f}) ==
        doctest::Approx(3.0f));
}

TEST_CASE("ground (Vector2) and world (Vector3) conversion") {
  // Ground (x, y) becomes world (x, z), with the height going in y.
  Vector3 eye = groundToWorld(Vector2{5.0f, -2.0f}, 1.63f);
  CHECK(eye.x == doctest::Approx(5.0f));
  CHECK(eye.y == doctest::Approx(1.63f));
  CHECK(eye.z == doctest::Approx(-2.0f));

  // Going back drops the height.
  Vector2 ground = worldToGround(eye);
  CHECK(ground.x == doctest::Approx(5.0f));
  CHECK(ground.y == doctest::Approx(-2.0f));
}
