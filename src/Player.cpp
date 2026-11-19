#include "fireteam/Player.hpp"
#include "fireteam/Rifle.hpp"

#include <cmath>
#include <stdexcept>

namespace fireteam {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kHalfPi = kPi / 2.0f;

}  // namespace

bool isValidPlayerInput(const PlayerInput& input) noexcept {
  return std::isfinite(input.moveX) && std::isfinite(input.moveZ) &&
         input.moveX >= -1.0f && input.moveX <= 1.0f &&
         input.moveZ >= -1.0f && input.moveZ <= 1.0f &&
         input.moveX * input.moveX + input.moveZ * input.moveZ <= 1.0001f &&
         std::isfinite(input.yaw) && input.yaw >= -kPi && input.yaw <= kPi &&
         std::isfinite(input.pitch) && input.pitch >= -kHalfPi &&
         input.pitch <= kHalfPi;
}

Player::Player(Vector3 position, int maxHealth, float moveSpeed)
    : Entity(position, maxHealth), moveSpeed_(moveSpeed),
      weapon_(std::make_unique<Rifle>()) {
  if (!std::isfinite(moveSpeed_) || moveSpeed_ < 0.0f) {
    throw std::invalid_argument("moveSpeed must be finite and non-negative");
  }
}

bool Player::applyInput(const PlayerInput& input, float deltaSeconds) {
  if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0f) {
    throw std::invalid_argument("deltaSeconds must be finite and non-negative");
  }
  if (!isValidPlayerInput(input)) {
    throw std::invalid_argument("Player input contains invalid movement or aim");
  }
  if (!isAlive()) {
    return false;
  }

  yaw_ = input.yaw;
  pitch_ = input.pitch;

  const float sinYaw = std::sin(yaw_);
  const float cosYaw = std::cos(yaw_);
  const Vector3 right{cosYaw, 0.0f, sinYaw};
  const Vector3 forward{sinYaw, 0.0f, -cosYaw};
  const Vector3 movement = right * input.moveX + forward * input.moveZ;
  position_ += movement * (moveSpeed_ * deltaSeconds);

  return input.firePressed;
}

void Player::update(float deltaSeconds) {
  if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0f) {
    throw std::invalid_argument("deltaSeconds must be finite and non-negative");
  }
  weapon_->update(deltaSeconds);
}

}  // namespace fireteam
