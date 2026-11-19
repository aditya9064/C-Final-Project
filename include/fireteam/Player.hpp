#pragma once

#include <memory>

#include "fireteam/Entity.hpp"
#include "fireteam/Weapon.hpp"

namespace fireteam {

// Local movement is measured in player-relative axes; aim angles are radians.
struct PlayerInput {
  float moveX = 0.0f;
  float moveZ = 0.0f;
  float yaw = 0.0f;
  float pitch = 0.0f;
  bool firePressed = false;
  bool reloadPressed = false;
};

bool isValidPlayerInput(const PlayerInput& input) noexcept;

class Player : public Entity {
 public:
  static constexpr float kDefaultMoveSpeed = 5.0f;

  explicit Player(Vector3 position, int maxHealth,
                  float moveSpeed = kDefaultMoveSpeed);

  // Applies movement and view angles; returns true when a fire request was
  // present for the combat system to handle.
  bool applyInput(const PlayerInput& input, float deltaSeconds);

  float yaw() const { return yaw_; }
  float pitch() const { return pitch_; }
  float moveSpeed() const { return moveSpeed_; }
  Weapon& weapon() { return *weapon_; }
  const Weapon& weapon() const { return *weapon_; }

  void update(float deltaSeconds) override;

 private:
  float yaw_ = 0.0f;
  float pitch_ = 0.0f;
  float moveSpeed_;
  std::unique_ptr<Weapon> weapon_;
};

}  // namespace fireteam
