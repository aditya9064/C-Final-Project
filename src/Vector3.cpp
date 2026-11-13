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

float Vector3::dot(const Vector3& other) const {
  return x * other.x + y * other.y + z * other.z;
}

Vector3 Vector3::cross(const Vector3& other) const {
  return Vector3{
      y * other.z - z * other.y,
      z * other.x - x * other.z,
      x * other.y - y * other.x,
  };
}

float Vector3::lengthSquared() const { return dot(*this); }

float Vector3::length() const { return std::sqrt(lengthSquared()); }

Vector3 Vector3::normalized() const {
  const float magnitude = length();
  if (magnitude == 0.0f) {
    return Vector3{};
  }
  return Vector3{x / magnitude, y / magnitude, z / magnitude};
}

Vector3 operator*(float scalar, const Vector3& vector) {
  return vector * scalar;
}

float distance(const Vector3& a, const Vector3& b) {
  return (a - b).length();
}

}  // namespace fireteam
