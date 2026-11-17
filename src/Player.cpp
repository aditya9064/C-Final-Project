#include "fireteam/Player.hpp"

#include <algorithm>
#include <cmath>

namespace fireteam {

Player::Player(Team team, Vector2 spawn)
    : Entity(spawn, kMaxHealth), team_(team) {}

void Player::setMoveIntent(const Vector2& planarDir) { moveDir_ = planarDir; }

void Player::setRunning(bool running) { running_ = running; }

void Player::update(float deltaSeconds) {
  // Mirror GameWorld's contract: ignore non-positive / non-finite steps rather
  // than integrate garbage.
  if (!(deltaSeconds > 0.0f) || !std::isfinite(deltaSeconds)) {
    velocity_ = Vector2{0.0f, 0.0f};
    return;
  }

  // Death is detected HERE, not pushed in: the combat system (Khushal, #8/#9)
  // only ever calls Entity::applyDamage(), so Player reacts to its own health
  // reaching 0 on the next tick -- exactly the pattern Entity.hpp prescribes.
  if (!isAlive() && !killed_) {
    onKilled();
  }

  // Timers advance even while dead (respawn) or standing (reload).
  if (reloading_) {
    reloadLeft_ -= deltaSeconds;
    if (reloadLeft_ <= 0.0f) {
      reloading_ = false;
      reloadLeft_ = 0.0f;
      ammo_ = kMagazineSize;
    }
  }
  if (respawnLeft_ > 0.0f) {
    respawnLeft_ = std::max(0.0f, respawnLeft_ - deltaSeconds);
  }

  // A dead or respawning player does not move.
  if (!isAlive() || awaitingRespawn()) {
    velocity_ = Vector2{0.0f, 0.0f};
    return;
  }

  if (moveDir_.lengthSquared() > 0.0f) {
    velocity_ = moveDir_.normalized() * speed();
  } else {
    velocity_ = Vector2{0.0f, 0.0f};
  }
  // Standalone integration; the Godot adapter overwrites with the post-collision
  // position after move_and_slide().
  position_ += velocity_ * deltaSeconds;
}

float Player::reloadProgress() const {
  if (!reloading_) return 1.0f;
  return std::clamp(1.0f - reloadLeft_ / kReloadSeconds, 0.0f, 1.0f);
}

bool Player::canFire() const {
  return isAlive() && !awaitingRespawn() && !reloading_ && ammo_ > 0;
}

bool Player::fire() {
  if (!canFire()) return false;
  --ammo_;
  return true;
}

void Player::reload() {
  if (reloading_ || !isAlive() || awaitingRespawn()) return;
  if (ammo_ >= kMagazineSize) return;
  reloading_ = true;
  reloadLeft_ = kReloadSeconds;
}

void Player::onKilled() {
  if (killed_) return;  // idempotent; normally called automatically from update()
  killed_ = true;
  respawnLeft_ = kRespawnSeconds;
  reloading_ = false;
  reloadLeft_ = 0.0f;
  moveDir_ = Vector2{0.0f, 0.0f};
  velocity_ = Vector2{0.0f, 0.0f};
}

PlayerSnapshot Player::snapshot() const {
  PlayerSnapshot s;
  s.x = position().x;
  s.z = position().y;  // model's Vector2.y is the world Z ground axis
  s.yaw = yaw_;
  s.health = health();
  s.ammo = ammo_;
  s.team = team_;
  s.firing = false;  // set per-shot by the caller; not part of persistent state
  return s;
}

void Player::applyRemoteState(const PlayerSnapshot& s) {
  setPosition(Vector2{s.x, s.z});
  yaw_ = s.yaw;
  ammo_ = s.ammo;
  team_ = s.team;
  // Health flows through applyDamage() on the authoritative side, not here.
}

}  // namespace fireteam
