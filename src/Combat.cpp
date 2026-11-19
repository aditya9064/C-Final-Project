#include "fireteam/Combat.hpp"

#include <cmath>

#include "fireteam/Projectile.hpp"

namespace fireteam {

// ---------- Cover ----------

CoverBox makeCover(Vector2 groundCenter, float sizeX, float sizeZ,
                   float height) {
  CoverBox box;
  box.minCorner = Vector3(groundCenter.x - sizeX / 2.0f, 0.0f,
                          groundCenter.y - sizeZ / 2.0f);
  box.maxCorner = Vector3(groundCenter.x + sizeX / 2.0f, height,
                          groundCenter.y + sizeZ / 2.0f);
  return box;
}

// ---------- Ray tests ----------

// Helper for rayHitsBox(). Looks at ONE axis (x, y, or z) at a time.
// The box's inside on this axis is from `low` to `high`. This works out
// when the ray is between them, and shrinks the window [tEnter, tExit]
// to fit. Returns false if the ray is never inside the box.
static bool clipToSlab(float start, float dir, float low, float high,
                       float& tEnter, float& tExit) {
  if (dir == 0.0f) {
    // Not moving on this axis, so it has to already be between them.
    return start >= low && start <= high;
  }

  float t1 = (low - start) / dir;
  float t2 = (high - start) / dir;
  if (t1 > t2) {
    // Moving in the negative direction, so it reaches `high` first.
    float temp = t1;
    t1 = t2;
    t2 = temp;
  }

  // The ray is inside the box only while it's inside on ALL three axes,
  // so keep the latest way in and the earliest way out.
  if (t1 > tEnter) {
    tEnter = t1;
  }
  if (t2 < tExit) {
    tExit = t2;
  }
  return tEnter <= tExit;
}

bool rayHitsBox(Vector3 origin, Vector3 direction, float maxDist,
                const CoverBox& box, float& hitDist) {
  float tEnter = 0.0f;    // nothing behind the start of the shot
  float tExit = maxDist;  // nothing past the end of the shot

  if (!clipToSlab(origin.x, direction.x, box.minCorner.x, box.maxCorner.x,
                  tEnter, tExit)) {
    return false;
  }
  if (!clipToSlab(origin.y, direction.y, box.minCorner.y, box.maxCorner.y,
                  tEnter, tExit)) {
    return false;
  }
  if (!clipToSlab(origin.z, direction.z, box.minCorner.z, box.maxCorner.z,
                  tEnter, tExit)) {
    return false;
  }

  hitDist = tEnter;
  return true;
}

bool rayHitsBody(Vector3 origin, Vector3 direction, float maxDist,
                 Vector2 feet, float bodyHeight, float& hitDist) {
  // Part 1: look from above. From above, the body is a circle and the
  // shot is a line, so find where the line goes into and out of the
  // circle. This is the usual ray/circle quadratic a*t^2 + b*t + c = 0.
  float dx = direction.x;
  float dz = direction.z;
  float ox = origin.x - feet.x;  // shot start, relative to the body
  float oz = origin.z - feet.y;  // (the ground Vector2's y is world z)

  float a = dx * dx + dz * dz;
  if (a == 0.0f) {
    return false;  // straight up or down, never crosses the circle
  }
  float b = 2.0f * (ox * dx + oz * dz);
  float c = ox * ox + oz * oz - BODY_RADIUS * BODY_RADIUS;

  float discriminant = b * b - 4.0f * a * c;
  if (discriminant < 0.0f) {
    return false;  // the line passes beside the body
  }
  float root = std::sqrt(discriminant);
  float tIn = (-b - root) / (2.0f * a);   // goes into the circle
  float tOut = (-b + root) / (2.0f * a);  // comes out of the circle

  if (tOut < 0.0f) {
    return false;  // the body is behind the shot
  }
  if (tIn < 0.0f) {
    tIn = 0.0f;  // the shot starts inside the circle (point blank)
  }

  // Part 2: check the height. How high is the bullet when it reaches
  // the body? Between the feet and the top of the head means a hit.
  float heightIn = origin.y + direction.y * tIn;
  float heightOut = origin.y + direction.y * tOut;

  float t = 0.0f;
  if (heightIn >= 0.0f && heightIn <= bodyHeight) {
    t = tIn;  // hits the side of the body
  } else if (heightIn > bodyHeight && heightOut <= bodyHeight) {
    // Came in above the head but is going down, so it hits the top of
    // the head. Find where the bullet is exactly at head height.
    t = (bodyHeight - origin.y) / direction.y;
  } else {
    return false;  // over the head (or under the floor) the whole way
  }

  if (t > maxDist) {
    return false;  // out of range
  }
  hitDist = t;
  return true;
}

bool lineOfSightClear(Vector3 from, Vector3 to,
                      const std::vector<CoverBox>& cover) {
  Vector3 direction = (to - from).normalized();
  float length = distance(from, to);

  for (const CoverBox& box : cover) {
    float hitDist = 0.0f;
    if (rayHitsBox(from, direction, length, box, hitDist)) {
      return false;  // something is in the way
    }
  }
  return true;
}

// ---------- Hitscan ----------

Entity* resolveShot(const Shot& shot, const Entity& shooter,
                    const std::vector<std::unique_ptr<Entity>>& entities,
                    const std::vector<CoverBox>& cover) {
  if (shot.kind != ShotKind::Hitscan) {
    return nullptr;  // grenades become Projectiles instead
  }
  if (!shooter.isAlive()) {
    return nullptr;
  }

  // Look through every entity and remember the closest one the shot
  // goes through.
  Entity* closest = nullptr;
  float closestDist = 0.0f;

  for (const auto& entity : entities) {
    if (entity == nullptr) {
      continue;  // shouldn't happen, but don't crash if it does
    }
    // Same address means it's the shooter.
    if (entity.get() == &shooter) {
      continue;
    }
    if (!entity->isAlive()) {
      continue;
    }

    float dist = 0.0f;
    if (!rayHitsBody(shot.origin, shot.direction, shot.range,
                     entity->position(), BODY_HEIGHT, dist)) {
      continue;  // the shot misses this one
    }

    // Strict < means if two targets are the same distance away, the one
    // earlier in the list wins.
    if (closest == nullptr || dist < closestDist) {
      closest = entity.get();
      closestDist = dist;
    }
  }

  if (closest == nullptr) {
    return nullptr;
  }

  // Is any cover closer than the target? Then the bullet hits the cover.
  for (const CoverBox& box : cover) {
    float coverDist = 0.0f;
    if (rayHitsBox(shot.origin, shot.direction, closestDist, box,
                   coverDist)) {
      return nullptr;
    }
  }

  closest->applyDamage(shot.damage);
  return closest;
}

// ---------- Explosions (grenades) ----------

int blastDamageAt(float dist, float radius, int maxDamage) {
  if (dist > radius || maxDamage <= 0) {
    return 0;
  }
  if (radius <= 0.0f) {
    return maxDamage;  // zero radius: only the exact center, full damage
  }

  // 1.0 at the center, 0.0 at the edge.
  float closeness = 1.0f - dist / radius;
  int damage = static_cast<int>(std::round(maxDamage * closeness));

  // Inside the radius always does at least 1 damage.
  if (damage < 1) {
    damage = 1;
  }
  return damage;
}

int applyBlast(Vector3 center, float radius, int maxDamage,
               const std::vector<std::unique_ptr<Entity>>& entities,
               const std::vector<CoverBox>& cover) {
  int damagedCount = 0;

  for (const auto& entity : entities) {
    if (entity == nullptr || !entity->isAlive()) {
      continue;
    }
    // dynamic_cast returns nullptr if the entity isn't a Projectile.
    if (dynamic_cast<Projectile*>(entity.get()) != nullptr) {
      continue;
    }

    // Closest part of the body to the blast: same spot on the ground,
    // at the blast's height, but kept between the feet and the head.
    float bodyY = center.y;
    if (bodyY < 0.0f) {
      bodyY = 0.0f;
    }
    if (bodyY > BODY_HEIGHT) {
      bodyY = BODY_HEIGHT;
    }
    Vector3 closestPoint = groundToWorld(entity->position(), bodyY);
    float dist = distance(center, closestPoint);

    // Cover check: can the blast "see" their feet or their head?
    Vector3 feet = groundToWorld(entity->position(), 0.0f);
    Vector3 head = groundToWorld(entity->position(), BODY_HEIGHT);
    if (!lineOfSightClear(center, feet, cover) &&
        !lineOfSightClear(center, head, cover)) {
      continue;  // fully behind cover
    }

    int damage = blastDamageAt(dist, radius, maxDamage);
    if (damage > 0) {
      entity->applyDamage(damage);
      damagedCount++;
    }
  }
  return damagedCount;
}

int explodeReadyProjectiles(
    const std::vector<std::unique_ptr<Entity>>& entities,
    const std::vector<CoverBox>& cover) {
  int explodedCount = 0;

  for (const auto& entity : entities) {
    if (entity == nullptr) {
      continue;
    }
    Projectile* projectile = dynamic_cast<Projectile*>(entity.get());
    if (projectile != nullptr && projectile->isAlive() &&
        projectile->readyToExplode()) {
      projectile->explode(entities, cover);
      explodedCount++;
    }
  }
  return explodedCount;
}

}  // namespace fireteam
