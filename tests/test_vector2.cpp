#include <doctest/doctest.h>

#include "fireteam/Vector2.hpp"

using fireteam::Vector2;

// doctest's Approx handles floating-point rounding: 0.1f + 0.2f is not
// exactly 0.3f in binary, so exact == on computed floats is a classic bug.

TEST_CASE("Vector2 default-constructs to the zero vector") {
  Vector2 v;
  CHECK(v.x == 0.0f);
  CHECK(v.y == 0.0f);
}

TEST_CASE("Vector2 addition and subtraction (normal use)") {
  Vector2 a{1.0f, 2.0f};
  Vector2 b{3.0f, -4.0f};

  Vector2 sum = a + b;
  CHECK(sum == Vector2{4.0f, -2.0f});

  Vector2 diff = a - b;
  CHECK(diff == Vector2{-2.0f, 6.0f});

  // Operands must be untouched: operator+ returns a new vector.
  CHECK(a == Vector2{1.0f, 2.0f});
  CHECK(b == Vector2{3.0f, -4.0f});
}

TEST_CASE("Vector2 compound assignment modifies in place and chains") {
  Vector2 pos{0.0f, 0.0f};
  Vector2 velocity{10.0f, 5.0f};
  const float dt = 0.5f;

  // The idiom every movement update will use.
  pos += velocity * dt;
  CHECK(pos == Vector2{5.0f, 2.5f});

  // operator+= returns a reference to `pos`, so the result IS pos.
  Vector2& result = (pos += Vector2{1.0f, 1.0f});
  CHECK(&result == &pos);
  CHECK(pos == Vector2{6.0f, 3.5f});

  pos -= Vector2{6.0f, 3.5f};
  CHECK(pos == Vector2{});

  Vector2 s{1.0f, -1.0f};
  s *= 3.0f;
  CHECK(s == Vector2{3.0f, -3.0f});
}

TEST_CASE("Vector2 scalar multiplication works from both sides") {
  Vector2 v{2.0f, -3.0f};
  CHECK(v * 2.0f == Vector2{4.0f, -6.0f});
  CHECK(2.0f * v == Vector2{4.0f, -6.0f});  // Exercises the free function.
}

TEST_CASE("Vector2 length uses a 3-4-5 triangle") {
  Vector2 v{3.0f, 4.0f};
  CHECK(v.length() == doctest::Approx(5.0f));
  CHECK(v.lengthSquared() == doctest::Approx(25.0f));
}

TEST_CASE("Vector2 normalized has unit length and keeps direction") {
  Vector2 v{3.0f, 4.0f};
  Vector2 n = v.normalized();
  CHECK(n.length() == doctest::Approx(1.0f));
  CHECK(n.x == doctest::Approx(0.6f));
  CHECK(n.y == doctest::Approx(0.8f));
}

TEST_CASE("Vector2 normalizing the zero vector is safe (boundary case)") {
  // Dividing by a length of 0 would produce NaN; we return (0,0) instead.
  Vector2 zero;
  Vector2 n = zero.normalized();
  CHECK(n == Vector2{});
}

TEST_CASE("distance between two points") {
  CHECK(fireteam::distance(Vector2{0.0f, 0.0f}, Vector2{3.0f, 4.0f}) ==
        doctest::Approx(5.0f));
  // Symmetric.
  CHECK(fireteam::distance(Vector2{3.0f, 4.0f}, Vector2{0.0f, 0.0f}) ==
        doctest::Approx(5.0f));
  // Distance to self is zero.
  CHECK(fireteam::distance(Vector2{7.0f, 7.0f}, Vector2{7.0f, 7.0f}) ==
        doctest::Approx(0.0f));
}
