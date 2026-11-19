#include "fireteam/Projectile.hpp"

#include <cmath>

#include "fireteam/Time.hpp"

namespace fireteam {

// A projectile has 1 health: it's "alive" while flying and dies when it
// explodes, which tells GameWorld to remove it.
Projectile::Projectile(const Shot& shot)
    : Entity(worldToGround(shot.origin), 1),
      blastDamage_(shot.damage),
      blastRadius_(shot.blastRadius) {
  height_ = shot.origin.y;
  if (height_ < 0.0f) {
    height_ = 0.0f;  // can't start under the floor
  }

  // How long until it first lands? Solve for the time t when the height
  // is back to 0:
  //   height + up * t - (gravity / 2) * t^2 = 0
  // With the quadratic formula, the positive answer is:
  //   t = (up + sqrt(up^2 + 2 * gravity * height)) / gravity
  // (From the ground, height = 0, this is just 2 * up / gravity.)
  float up = PROJECTILE_THROW_UP_SPEED;
  float timeInAir =
      (up + std::sqrt(up * up + 2.0f * PROJECTILE_GRAVITY * height_)) /
      PROJECTILE_GRAVITY;

  // Ground speed so it covers shot.range in that time.
  float groundSpeed = shot.range / timeInAir;

  // Only the ground part of the aim picks the direction. A zero aim
  // stays (0, 0), so it goes straight up and comes back down.
  Vector2 groundDirection = worldToGround(shot.direction).normalized();
  velocity_ = groundToWorld(groundDirection * groundSpeed, up);
}

void Projectile::update(float deltaSeconds) {
  requireValidDt(deltaSeconds);

  if (readyToExplode_) {
    return;  // waiting for the combat code to call explode()
  }

  // Count down the fuse no matter what else is happening.
  fuseLeft_ = fuseLeft_ - deltaSeconds;
  if (fuseLeft_ <= 0.0f) {
    fuseLeft_ = 0.0f;
    readyToExplode_ = true;
  }

  // Sitting still on the ground: no more physics. Without this, gravity
  // would push it below the ground every frame and it would keep bouncing.
  if (stopped_) {
    return;
  }

  // Ground movement: straight line (the x and z parts of the velocity).
  position_ += worldToGround(velocity_) * deltaSeconds;

  // Height: gravity changes the speed, then the speed changes the height.
  // Doing it in this order every frame makes the height follow a parabola.
  velocity_.y = velocity_.y - PROJECTILE_GRAVITY * deltaSeconds;
  height_ = height_ + velocity_.y * deltaSeconds;

  // Hit the ground while falling: bounce.
  if (height_ <= 0.0f && velocity_.y < 0.0f) {
    height_ = 0.0f;
    velocity_.y = -velocity_.y * PROJECTILE_BOUNCE;
    velocity_.x *= PROJECTILE_GROUND_FRICTION;
    velocity_.z *= PROJECTILE_GROUND_FRICTION;
    bounceCount_++;

    // Bounces too small to matter: stop and sit on the ground.
    if (velocity_.y < PROJECTILE_MIN_BOUNCE_SPEED) {
      velocity_ = Vector3(0, 0, 0);
      stopped_ = true;
    }
  }
}

int Projectile::explode(const std::vector<std::unique_ptr<Entity>>& entities,
                        const std::vector<CoverBox>& cover) {
  if (!isAlive()) {
    return 0;  // already exploded
  }
  int damaged = applyBlast(worldPosition(), blastRadius_, blastDamage_,
                           entities, cover);

  // Kill the projectile so it can't explode again and GameWorld removes it.
  applyDamage(health());
  return damaged;
}

}  // namespace fireteam
