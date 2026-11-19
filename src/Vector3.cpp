#include "fireteam/Vector3.hpp"

#include <cmath>

namespace fireteam {

Vector3::Vector3(float x, float y, float z) : x(x), y(y), z(z) {}

Vector3 Vector3::operator+(const Vector3& other) const {
  return Vector3{x + other.x, y + other.y, z + other.z};
}

Vector3 Vector3::operator-(const Vector3& other) const {
  return Vector3{x - other.x, y - other.y, z - other.z};
}

Vector3 Vector3::operator*(float scalar) const {
  return Vector3{x * scalar, y * scalar, z * scalar};
}

Vector3& Vector3::operator+=(const Vector3& other) {
  x += other.x;
  y += other.y;
  z += other.z;
  return *this;
}

Vector3& Vector3::operator-=(const Vector3& other) {
  x -= other.x;
  y -= other.y;
  z -= other.z;
  return *this;
}

Vector3& Vector3::operator*=(float scalar) {
  x *= scalar;
  y *= scalar;
  z *= scalar;
  return *this;
}

bool Vector3::operator==(const Vector3& other) const {
  return x == other.x && y == other.y && z == other.z;
}

bool Vector3::operator!=(const Vector3& other) const {
  return !(*this == other);
}

float Vector3::lengthSquared() const { return x * x + y * y + z * z; }

float Vector3::length() const { return std::sqrt(lengthSquared()); }

Vector3 Vector3::normalized() const {
  float len = length();
  if (len == 0.0f) {
    return Vector3{};  // no direction, don't divide by zero
  }
  return Vector3{x / len, y / len, z / len};
}

Vector3 operator*(float scalar, const Vector3& v) { return v * scalar; }

float dot(const Vector3& a, const Vector3& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

float distance(const Vector3& a, const Vector3& b) { return (a - b).length(); }

Vector3 groundToWorld(const Vector2& ground, float height) {
  // The ground Vector2's y is the world z.
  return Vector3{ground.x, height, ground.y};
}

Vector2 worldToGround(const Vector3& point) {
  return Vector2{point.x, point.z};
}

}  // namespace fireteam
