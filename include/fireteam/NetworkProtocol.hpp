#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "fireteam/GameState.hpp"
#include "fireteam/Player.hpp"

namespace fireteam {

using ClientInput = PlayerInput;

struct PlayerSnapshot {
  std::uint32_t playerId = 0;
  Vector3 position;
  float yaw = 0.0f;
  float pitch = 0.0f;
  std::int32_t health = 0;
  std::int32_t maxHealth = 0;
  std::int32_t ammo = 0;
  std::int32_t maxAmmo = 0;
  bool reloading = false;
  std::uint32_t shotsFired = 0;
};

struct GameSnapshot {
  MatchPhase phase = MatchPhase::Lobby;
  // Player ID zero marks an unoccupied slot.
  std::array<PlayerSnapshot, 2> players{};
};

class NetworkProtocol {
 public:
  // Version 2 uses little-endian integers and IEEE-754 32-bit floats.
  static constexpr std::size_t kClientInputPacketSize = 28;
  static constexpr std::size_t kGameSnapshotPacketSize = 108;

  static std::array<std::uint8_t, kClientInputPacketSize> encodeClientInput(
      const ClientInput& input);
  static ClientInput decodeClientInput(std::span<const std::uint8_t> packet);

  static std::array<std::uint8_t, kGameSnapshotPacketSize> encodeGameSnapshot(
      const GameSnapshot& snapshot);
  static GameSnapshot decodeGameSnapshot(std::span<const std::uint8_t> packet);
};

}  // namespace fireteam
