// Projectile: a thrown grenade flying through the 3D arena.
//
// Units are metres and seconds, and y is up (same as the Godot arena).
//
// Where it is:
//   - position_ (from Entity) is where it is on the ground (x, z).
//     GameWorld and everything else can keep using it like any entity.
//   - height_ is how high it is off the ground.
//   - worldPosition() puts the two together into one 3D point.
//
// How it moves: velocity_ is a Vector3. The x and z parts move it along
// the ground in a straight line, and gravity pulls the y part down every
// frame, so it flies in a real arc (a parabola).
//
// When it hits the ground it bounces: the upward speed flips and gets
// smaller, and the ground speed slows down. Once the bounces get tiny it
// stops and sits on the ground.
//
// When the fuse runs out it freezes where it is (even in mid-air) and is
// "ready to explode". The combat code then calls explode() (see
// explodeReadyProjectiles() in Combat.hpp), which damages everything in
// the blast radius and kills the projectile so GameWorld removes it.
#pragma once

#include <memory>
#include <vector>

#include "fireteam/Combat.hpp"
#include "fireteam/Entity.hpp"
#include "fireteam/Vector3.hpp"
#include "fireteam/Weapon.hpp"

namespace fireteam {

// Physics settings for every projectile.
const float PROJECTILE_GRAVITY = 9.8f;           // metres per second^2
const float PROJECTILE_THROW_UP_SPEED = 7.0f;    // starting upward speed, m/s
const float PROJECTILE_FUSE_SECONDS = 3.0f;
const float PROJECTILE_BOUNCE = 0.5f;            // keeps 50% of upward speed
const float PROJECTILE_GROUND_FRICTION = 0.5f;   // keeps 50% of ground speed
const float PROJECTILE_MIN_BOUNCE_SPEED = 1.5f;  // slower than this: stop

class Projectile : public Entity {
 public:
  // Builds a flying grenade from a grenade Shot.
  // - It starts at shot.origin, so throwing from hand height (about
  //   1.5 m) starts it 1.5 m up. A negative height is fixed to 0.
  // - It always goes up at PROJECTILE_THROW_UP_SPEED (a lob). Only the
  //   ground part of shot.direction is used, to pick which way it goes.
  // - Its ground speed is picked so the first landing is shot.range away.
  explicit Projectile(const Shot& shot);

  // Moves the projectile and counts down the fuse.
  // Throws std::invalid_argument on negative, NaN, or infinite dt.
  void update(float deltaSeconds) override;

  // Damages every living entity the blast reaches (see applyBlast() in
  // Combat.hpp), then kills this projectile. Cover between the blast and
  // someone protects them. Does nothing if it has already exploded.
  // Returns how many entities were damaged.
  int explode(const std::vector<std::unique_ptr<Entity>>& entities,
              const std::vector<CoverBox>& cover = {});

  // Ground position + height as one 3D point.
  Vector3 worldPosition() const { return groundToWorld(position_, height_); }

  bool readyToExplode() const { return readyToExplode_; }
  float height() const { return height_; }
  const Vector3& velocity() const { return velocity_; }
  float fuseLeft() const { return fuseLeft_; }
  int bounceCount() const { return bounceCount_; }
  bool isStopped() const { return stopped_; }
  int blastDamage() const { return blastDamage_; }
  float blastRadius() const { return blastRadius_; }

 private:
  Vector3 velocity_;  // x, z: along the ground. y: up (+) or down (-)
  float height_ = 0.0f;
  float fuseLeft_ = PROJECTILE_FUSE_SECONDS;
  int bounceCount_ = 0;
  bool stopped_ = false;  // done bouncing, sitting on the ground
  bool readyToExplode_ = false;
  int blastDamage_;
  float blastRadius_;
};

}  // namespace fireteam
