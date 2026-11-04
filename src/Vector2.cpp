#include "fireteam/Vector2.hpp"

#include <cmath>

namespace fireteam {

// Member initializer list (`: x(x), y(y)`) initializes the members directly
// instead of default-constructing them and then assigning. For floats the
// difference is negligible, but it's the idiomatic form and matters for
// members that are expensive or impossible to default-construct.
Vector2::Vector2(float x, float y) : x(x), y(y) {}

// `Vector2{...}` builds a temporary and returns it by value. The compiler
// elides the copy (return value optimization), so this is not slow.
Vector2 Vector2::operator+(const Vector2& other) const {
  return Vector2{x + other.x, y + other.y};
}

Vector2 Vector2::operator-(const Vector2& other) const {
  return Vector2{x - other.x, y - other.y};
}

Vector2 Vector2::operator*(float scalar) const {
  return Vector2{x * scalar, y * scalar};
}

// `*this` is the object the operator was called on. Returning `*this` by
// reference (the `Vector2&` return type) hands back the same object, not a
// copy, which is what lets `a += b` be used as an expression.
Vector2& Vector2::operator+=(const Vector2& other) {
  x += other.x;
  y += other.y;
  return *this;
}

Vector2& Vector2::operator-=(const Vector2& other) {
  x -= other.x;
  y -= other.y;
  return *this;
}

Vector2& Vector2::operator*=(float scalar) {
  x *= scalar;
  y *= scalar;
  return *this;
}

// Exact float comparison is fine here because we only use it for identity
// checks in tests and for the "is this the zero vector" case. For tolerance
// based comparisons callers should compare `distance(a, b)` to an epsilon.
bool Vector2::operator==(const Vector2& other) const {
  return x == other.x && y == other.y;
}

bool Vector2::operator!=(const Vector2& other) const {
  return !(*this == other);
}

float Vector2::lengthSquared() const { return x * x + y * y; }

float Vector2::length() const { return std::sqrt(lengthSquared()); }

Vector2 Vector2::normalized() const {
  const float len = length();
  if (len == 0.0f) {
    return Vector2{};  // No direction to normalize; avoid dividing by zero.
  }
  return Vector2{x / len, y / len};
}

// Non-member operator: delegates to the member version so the math lives in
// exactly one place.
Vector2 operator*(float scalar, const Vector2& v) { return v * scalar; }

float distance(const Vector2& a, const Vector2& b) { return (a - b).length(); }

}  // namespace fireteam
