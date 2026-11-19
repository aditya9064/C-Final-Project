// Vector3: a 3D float vector for combat math (shots, cover, grenades).
//
// The arena is 3D (see docs/rendering-architecture.md in PR #18), so a
// shot needs a height: a bullet can go over low cover but not tall cover.
//
// Axes match Godot:
//   x = left/right on the ground
//   y = UP (height off the ground, in metres)
//   z = forward/back on the ground
//
// Entity still stores its position as a Vector2 on the ground. That
// Vector2's (x, y) is the ground (x, z) here. groundToWorld() and
// worldToGround() below convert between the two.
#pragma once

#include "fireteam/Vector2.hpp"

namespace fireteam {

struct Vector3 {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;

  Vector3() = default;
  Vector3(float x, float y, float z);

  // These return a new vector and don't change this one.
  Vector3 operator+(const Vector3& other) const;
  Vector3 operator-(const Vector3& other) const;
  Vector3 operator*(float scalar) const;

  // These change this vector.
  Vector3& operator+=(const Vector3& other);
  Vector3& operator-=(const Vector3& other);
  Vector3& operator*=(float scalar);

  bool operator==(const Vector3& other) const;
  bool operator!=(const Vector3& other) const;

  // sqrt(x^2 + y^2 + z^2)
  float length() const;

  // Same as length() but without the sqrt. Good for comparing distances.
  float lengthSquared() const;

  // Copy with length 1, same direction. A zero vector returns (0, 0, 0).
  Vector3 normalized() const;
};

// So that 2.0f * v works too, not only v * 2.0f.
Vector3 operator*(float scalar, const Vector3& v);

// Dot product: a.x*b.x + a.y*b.y + a.z*b.z.
// If b is a unit direction, dot(a, b) is how far a goes along b.
// The hitscan code uses this to find the closest point on a shot's line.
float dot(const Vector3& a, const Vector3& b);

float distance(const Vector3& a, const Vector3& b);

// Ground position (from Entity) + height -> 3D point.
// Example: groundToWorld({5, 2}, 1.63f) is (5, 1.63, 2).
Vector3 groundToWorld(const Vector2& ground, float height);

// 3D point -> ground position (drops the height).
Vector2 worldToGround(const Vector3& point);

}  // namespace fireteam
