#include "fireteam/Bot.hpp"

#include <utility>

#include "fireteam/Combat.hpp"
#include "fireteam/Time.hpp"
#include "fireteam/Projectile.hpp"
#include "fireteam/Rifle.hpp"

namespace fireteam {

Bot::Bot(Vector2 position, std::vector<Vector2> waypoints, int maxHealth,
         float sightRange, float speed)
    : Entity(position, maxHealth),
      weapon_(std::make_unique<Rifle>(RIFLE_DAMAGE, RIFLE_RANGE,
                                      BOT_FIRE_COOLDOWN)),
      waypoints_(std::move(waypoints)),
      sightRange_(sightRange),
      speed_(speed) {
  if (sightRange_ < 0.0f) {
    sightRange_ = 0.0f;
  }
  if (speed_ < 0.0f) {
    speed_ = 0.0f;
  }
}

void Bot::update(float deltaSeconds) {
  requireValidDt(deltaSeconds);

  weapon_->update(deltaSeconds);

  if (state_ != BotState::Patrol || waypoints_.empty()) {
    return;  // engaging, or nowhere to walk to
  }

  Vector2 goal = waypoints_[currentWaypoint_];
  Vector2 toGoal = goal - position_;
  float step = speed_ * deltaSeconds;

  if (toGoal.length() <= step) {
    // Close enough to reach it this frame: snap onto it and aim for the
    // next waypoint. After the last one, loop back to the first.
    position_ = goal;
    currentWaypoint_++;
    if (currentWaypoint_ >= static_cast<int>(waypoints_.size())) {
      currentWaypoint_ = 0;
    }
  } else {
    position_ += toGoal.normalized() * step;
  }
}

void Bot::think(const std::vector<std::unique_ptr<Entity>>& entities,
                const std::vector<CoverBox>& cover) {
  if (!isAlive()) {
    return;
  }

  Entity* target = findTarget(entities, cover);
  if (target == nullptr) {
    state_ = BotState::Patrol;
    return;
  }

  state_ = BotState::Engage;

  // Shoot from our eyes at the target's eyes (the same line findTarget()
  // checked), if the cooldown is done.
  Vector3 myEye = groundToWorld(position_, STAND_EYE_HEIGHT);
  Vector3 targetEye = groundToWorld(target->position(), STAND_EYE_HEIGHT);
  Shot shot;
  if (weapon_->fire(myEye, targetEye - myEye, shot)) {
    resolveShot(shot, *this, entities, cover);
  }
}

Entity* Bot::findTarget(const std::vector<std::unique_ptr<Entity>>& entities,
                        const std::vector<CoverBox>& cover) {
  Entity* closest = nullptr;
  float closestDist = 0.0f;
  Vector3 myEye = groundToWorld(position_, STAND_EYE_HEIGHT);

  for (const auto& entity : entities) {
    if (entity == nullptr || entity.get() == this || !entity->isAlive()) {
      continue;
    }
    // dynamic_cast returns nullptr if the entity isn't that type.
    if (dynamic_cast<Bot*>(entity.get()) != nullptr) {
      continue;  // don't shoot other bots
    }
    if (dynamic_cast<Projectile*>(entity.get()) != nullptr) {
      continue;  // don't shoot grenades
    }

    float dist = distance(position_, entity->position());
    if (dist > sightRange_) {
      continue;  // too far away
    }
    Vector3 theirEye = groundToWorld(entity->position(), STAND_EYE_HEIGHT);
    if (!lineOfSightClear(myEye, theirEye, cover)) {
      continue;  // hiding behind cover
    }
    if (closest == nullptr || dist < closestDist) {
      closest = entity.get();
      closestDist = dist;
    }
  }
  return closest;
}

void runBotAI(const std::vector<std::unique_ptr<Entity>>& entities,
              const std::vector<CoverBox>& cover) {
  for (const auto& entity : entities) {
    if (entity == nullptr) {
      continue;
    }
    Bot* bot = dynamic_cast<Bot*>(entity.get());
    if (bot != nullptr && bot->isAlive()) {
      bot->think(entities, cover);
    }
  }
}

}  // namespace fireteam
