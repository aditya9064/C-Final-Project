// Vector3: a 3D float vector for world positions, velocities, and directions.
#pragma once

namespace fireteam {

struct Vector3 {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;

  Vector3() = default;
  Vector3(float x, float y, float z);

  Vector3 operator+(const Vector3& other) const;
  Vector3 operator-(const Vector3& other) const;
  Vector3 operator*(float scalar) const;

  Vector3& operator+=(const Vector3& other);
  Vector3& operator-=(const Vector3& other);
  Vector3& operator*=(float scalar);

  bool operator==(const Vector3& other) const;
  bool operator!=(const Vector3& other) const;

  float dot(const Vector3& other) const;
  Vector3 cross(const Vector3& other) const;
  float length() const;
  float lengthSquared() const;
  Vector3 normalized() const;
};

Vector3 operator*(float scalar, const Vector3& vector);
float distance(const Vector3& a, const Vector3& b);

}  // namespace fireteam
