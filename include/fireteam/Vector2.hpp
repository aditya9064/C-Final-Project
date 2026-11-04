// Vector2: a 2D float vector used for positions, velocities, and directions.
//
// This is a `struct` (public members) rather than a `class` because it is
// plain data with no invariant to protect -- any pair of floats is a valid
// vector. Operator overloading is justified here because game math reads
// far more clearly as `pos += velocity * dt` than as nested function calls.
#pragma once

namespace fireteam {

struct Vector2 {
  float x = 0.0f;
  float y = 0.0f;

  // Default-constructs to (0, 0) thanks to the member initializers above.
  Vector2() = default;
  Vector2(float x, float y);

  // Arithmetic operators that return a NEW vector and leave `*this` alone.
  // They are `const` because they do not modify the object they're called on.
  Vector2 operator+(const Vector2& other) const;
  Vector2 operator-(const Vector2& other) const;
  Vector2 operator*(float scalar) const;

  // Compound-assignment operators MODIFY `*this`, then return a reference to
  // it so expressions can chain (e.g. `a += b` evaluates to `a`).
  Vector2& operator+=(const Vector2& other);
  Vector2& operator-=(const Vector2& other);
  Vector2& operator*=(float scalar);

  bool operator==(const Vector2& other) const;
  bool operator!=(const Vector2& other) const;

  // Euclidean length: sqrt(x^2 + y^2).
  float length() const;

  // Length without the sqrt. Cheaper for comparing distances (e.g. range
  // checks) because sqrt is monotonic, so comparing squares gives the same
  // answer.
  float lengthSquared() const;

  // Returns a unit-length copy pointing the same direction. A zero-length
  // vector has no direction, so it returns (0, 0) instead of dividing by 0.
  Vector2 normalized() const;
};

// Free function so `2.0f * v` works as well as `v * 2.0f`. A member operator
// can only handle the case where the Vector2 is on the LEFT.
Vector2 operator*(float scalar, const Vector2& v);

// Convenience for distance between two points.
float distance(const Vector2& a, const Vector2& b);

}  // namespace fireteam
