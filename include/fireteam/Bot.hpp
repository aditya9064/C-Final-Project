// Bot: a computer-controlled soldier that fills empty lobby slots.
//
// It's a simple state machine with two states:
//   Patrol: walk around a loop of waypoints.
//   Engage: a target is in sight, so stand still and shoot it.
// It switches to Engage as soon as a target is in sight, and back to
// Patrol when no target is in sight anymore (moved away, went behind
// cover, or died).
//
// "In sight" means within sight range AND no cover in the way, checked
// in 3D from the bot's eyes to the target's eyes (both 1.63 m up). So a
// player standing behind low cover (1.28 m) can be seen and shot in the
// head, but a player behind tall cover (2.10 m) is hidden.
// The bot shoots along that same line, eyes to eyes.
//
// The work is split in two because Entity::update() only gets dt:
//   update(dt)       -> moves along the patrol route, counts down the
//                       weapon cooldown (normal Entity stuff)
//   think(entities, cover) -> looks around, picks the state, and shoots
// Each frame the game calls update() on every entity (GameWorld does
// this), then runBotAI(world.entities(), arenaCover) to let every bot
// think.
//
// Like Player, a Bot OWNS its weapon (composition): a Rifle, but one that
// fires slower (BOT_FIRE_COOLDOWN instead of RIFLE_COOLDOWN).
#pragma once

#include <memory>
#include <vector>

#include "fireteam/Combat.hpp"
#include "fireteam/Entity.hpp"
#include "fireteam/Weapon.hpp"

namespace fireteam {

// Default bot settings (distances in metres, like the 3D arena).
// The sight range is shorter than the rifle's range (RIFLE_RANGE), so a
// bot never starts shooting at someone its rifle can't reach.
const int BOT_HEALTH = 100;
const float BOT_SIGHT_RANGE = 40.0f;  // metres
const float BOT_SPEED = 4.0f;         // metres per second (a jog)
// Bots shoot slower than a player's rifle (RIFLE_COOLDOWN is 0.25 s), so
// a player has time to fight back. Same damage and range as a rifle.
const float BOT_FIRE_COOLDOWN = 0.6f;  // seconds between bot shots

enum class BotState { Patrol, Engage };

class Bot : public Entity {
 public:
  // `waypoints` is the patrol loop. With no waypoints the bot stands
  // still while patrolling. A negative sight range or speed is fixed to 0.
  Bot(Vector2 position, std::vector<Vector2> waypoints,
      int maxHealth = BOT_HEALTH, float sightRange = BOT_SIGHT_RANGE,
      float speed = BOT_SPEED);

  // Patrol: walk toward the current waypoint. Engage: stand still.
  // Always counts down the weapon cooldown.
  // Throws std::invalid_argument on negative, NaN, or infinite dt.
  void update(float deltaSeconds) override;

  // Looks for the closest target it can see: within sight range (exactly
  // at sight range counts) and not hidden behind any of the `cover`.
  // If there is one: switch to Engage and shoot at it when the weapon is
  // ready. If not: switch to Patrol. A dead bot does nothing.
  //
  // Targets are living entities that aren't this bot, another Bot, or a
  // Projectile. (No teams yet, so bots fight everyone else.)
  // `cover` can be left out (an open field).
  void think(const std::vector<std::unique_ptr<Entity>>& entities,
             const std::vector<CoverBox>& cover = {});

  BotState state() const { return state_; }
  float sightRange() const { return sightRange_; }
  float speed() const { return speed_; }
  const Weapon& weapon() const { return *weapon_; }
  int currentWaypoint() const { return currentWaypoint_; }

 private:
  // Returns the closest target in sight, or nullptr if there isn't one.
  Entity* findTarget(const std::vector<std::unique_ptr<Entity>>& entities,
                     const std::vector<CoverBox>& cover);

  std::unique_ptr<Weapon> weapon_;
  std::vector<Vector2> waypoints_;
  int currentWaypoint_ = 0;
  float sightRange_;
  float speed_;
  BotState state_ = BotState::Patrol;
};

// Calls think() on every living Bot in the list. Call once per frame,
// after updating the entities.
void runBotAI(const std::vector<std::unique_ptr<Entity>>& entities,
              const std::vector<CoverBox>& cover = {});

}  // namespace fireteam
