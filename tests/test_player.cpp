#include <doctest/doctest.h>

#include <limits>
#include <stdexcept>

#include "fireteam/Player.hpp"

using fireteam::Player;
using fireteam::PlayerInput;
using fireteam::Vector3;

TEST_CASE("Player moves in player-relative directions at a frame-independent speed") {
  Player player{Vector3{}, 100};

  player.applyInput(PlayerInput{.moveZ = 1.0f}, 0.5f);
  CHECK(player.position() == Vector3{0.0f, 0.0f, -2.5f});

  player.applyInput(PlayerInput{.moveX = 1.0f}, 0.5f);
  CHECK(player.position() == Vector3{2.5f, 0.0f, -2.5f});
}

TEST_CASE("Player movement follows its yaw and keeps diagonal speed normalized") {
  Player player{Vector3{}, 100};

  player.applyInput(PlayerInput{.moveZ = 1.0f, .yaw = 1.57079632679f},
                    1.0f);
  CHECK(player.position().x == doctest::Approx(5.0f));
  CHECK(player.position().z == doctest::Approx(0.0f));

  player.applyInput(
      PlayerInput{.moveX = 0.70710678f, .moveZ = 0.70710678f}, 1.0f);
  CHECK(player.position().x == doctest::Approx(8.535534f));
  CHECK(player.position().z == doctest::Approx(-3.535534f));
}

TEST_CASE("Player updates aim and surfaces a fire request for combat") {
  Player player{Vector3{}, 100};
  const PlayerInput input{
      .yaw = -0.75f,
      .pitch = 0.25f,
      .firePressed = true,
  };

  CHECK(player.applyInput(input, 0.0f));
  CHECK(player.yaw() == doctest::Approx(-0.75f));
  CHECK(player.pitch() == doctest::Approx(0.25f));
  CHECK(player.position() == Vector3{});
  CHECK_FALSE(player.applyInput(PlayerInput{}, 0.0f));
}

TEST_CASE("Player rejects invalid movement, aim, speed, and frame time") {
  CHECK_THROWS_AS(Player(Vector3{}, 100, -1.0f), std::invalid_argument);
  CHECK_THROWS_AS(
      Player(Vector3{}, 100, std::numeric_limits<float>::infinity()),
      std::invalid_argument);

  Player player{Vector3{}, 100};
  CHECK_THROWS_AS(player.applyInput(PlayerInput{.moveX = 1.0f, .moveZ = 1.0f},
                                   0.1f),
                  std::invalid_argument);
  CHECK_THROWS_AS(
      player.applyInput(
          PlayerInput{.yaw = std::numeric_limits<float>::quiet_NaN()}, 0.1f),
      std::invalid_argument);
  CHECK_THROWS_AS(player.applyInput(PlayerInput{}, -0.1f),
                  std::invalid_argument);
  CHECK_THROWS_AS(
      player.applyInput(PlayerInput{},
                        std::numeric_limits<float>::infinity()),
      std::invalid_argument);
}

TEST_CASE("Dead players do not move aim or request fire") {
  Player player{Vector3{1.0f, 0.0f, 2.0f}, 100};
  player.applyInput(PlayerInput{.yaw = 0.5f}, 0.0f);
  player.applyDamage(100);

  CHECK_FALSE(player.applyInput(
      PlayerInput{.moveX = 1.0f, .yaw = 1.0f, .firePressed = true}, 1.0f));
  CHECK(player.position() == Vector3{1.0f, 0.0f, 2.0f});
  CHECK(player.yaw() == doctest::Approx(0.5f));
  CHECK(player.health() == 0);
}
