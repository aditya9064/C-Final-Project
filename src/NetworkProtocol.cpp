#include "fireteam/NetworkProtocol.hpp"

#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace fireteam {
namespace {

constexpr std::array<std::uint8_t, 4> kMagic{'F', 'T', 'S', 'K'};
constexpr std::uint8_t kVersion = 1;
constexpr std::uint8_t kClientInputType = 1;
constexpr std::uint8_t kGameSnapshotType = 2;
constexpr float kMaxWorldCoordinate = 10000.0f;

static_assert(sizeof(float) == sizeof(std::uint32_t));
static_assert(std::numeric_limits<float>::is_iec559);

void writeU32(std::span<std::uint8_t> bytes, std::size_t offset,
              std::uint32_t value) {
  for (std::size_t i = 0; i < sizeof(value); ++i) {
    bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8));
  }
}

std::uint32_t readU32(std::span<const std::uint8_t> bytes,
                      std::size_t offset) {
  std::uint32_t value = 0;
  for (std::size_t i = 0; i < sizeof(value); ++i) {
    value |= static_cast<std::uint32_t>(bytes[offset + i]) << (i * 8);
  }
  return value;
}

void writeI32(std::span<std::uint8_t> bytes, std::size_t offset,
              std::int32_t value) {
  writeU32(bytes, offset, std::bit_cast<std::uint32_t>(value));
}

std::int32_t readI32(std::span<const std::uint8_t> bytes,
                     std::size_t offset) {
  return std::bit_cast<std::int32_t>(readU32(bytes, offset));
}

void writeFloat(std::span<std::uint8_t> bytes, std::size_t offset,
                float value) {
  writeU32(bytes, offset, std::bit_cast<std::uint32_t>(value));
}

float readFloat(std::span<const std::uint8_t> bytes, std::size_t offset) {
  return std::bit_cast<float>(readU32(bytes, offset));
}

void writeHeader(std::span<std::uint8_t> packet, std::uint8_t type) {
  for (std::size_t i = 0; i < kMagic.size(); ++i) {
    packet[i] = kMagic[i];
  }
  packet[4] = kVersion;
  packet[5] = type;
  packet[6] = 0;
  packet[7] = 0;
}

void validateHeader(std::span<const std::uint8_t> packet,
                    std::size_t expectedSize, std::uint8_t expectedType) {
  if (packet.size() != expectedSize) {
    throw std::invalid_argument("Network packet has an invalid size");
  }
  for (std::size_t i = 0; i < kMagic.size(); ++i) {
    if (packet[i] != kMagic[i]) {
      throw std::invalid_argument("Network packet has an invalid signature");
    }
  }
  if (packet[4] != kVersion || packet[5] != expectedType || packet[6] != 0 ||
      packet[7] != 0) {
    throw std::invalid_argument("Network packet has an unsupported header");
  }
}

bool validPosition(const Vector3& position) {
  return std::isfinite(position.x) && std::isfinite(position.y) &&
         std::isfinite(position.z) &&
         std::abs(position.x) <= kMaxWorldCoordinate &&
         std::abs(position.y) <= kMaxWorldCoordinate &&
         std::abs(position.z) <= kMaxWorldCoordinate;
}

void validateInput(const ClientInput& input) {
  if (!isValidPlayerInput(input)) {
    throw std::invalid_argument("Client input contains invalid movement or aim");
  }
}

void validatePlayer(const PlayerSnapshot& player) {
  if (player.playerId == 0) {
    if (player.position != Vector3{} || player.yaw != 0.0f ||
        player.pitch != 0.0f || player.health != 0 || player.maxHealth != 0) {
      throw std::invalid_argument("Empty player slot must have zero state");
    }
    return;
  }

  if (!validPosition(player.position) ||
      !isValidPlayerInput(
          PlayerInput{.yaw = player.yaw, .pitch = player.pitch}) ||
      player.maxHealth <= 0 || player.health < 0 ||
      player.health > player.maxHealth) {
    throw std::invalid_argument("Player snapshot contains invalid state");
  }
}

void validateSnapshot(const GameSnapshot& snapshot) {
  switch (snapshot.phase) {
    case MatchPhase::Lobby:
    case MatchPhase::InGame:
    case MatchPhase::MatchOver:
      break;
    default:
      throw std::invalid_argument("Game snapshot has an invalid match phase");
  }

  for (const auto& player : snapshot.players) {
    validatePlayer(player);
  }
  if (snapshot.players[0].playerId != 0 &&
      snapshot.players[0].playerId == snapshot.players[1].playerId) {
    throw std::invalid_argument("Player snapshot IDs must be unique");
  }
}

}  // namespace

std::array<std::uint8_t, NetworkProtocol::kClientInputPacketSize>
NetworkProtocol::encodeClientInput(const ClientInput& input) {
  validateInput(input);

  std::array<std::uint8_t, kClientInputPacketSize> packet{};
  writeHeader(packet, kClientInputType);
  writeFloat(packet, 8, input.moveX);
  writeFloat(packet, 12, input.moveZ);
  writeFloat(packet, 16, input.yaw);
  writeFloat(packet, 20, input.pitch);
  packet[24] = input.firePressed ? 1 : 0;
  return packet;
}

ClientInput NetworkProtocol::decodeClientInput(
    std::span<const std::uint8_t> packet) {
  validateHeader(packet, kClientInputPacketSize, kClientInputType);
  if (packet[24] > 1 || packet[25] != 0 || packet[26] != 0 ||
      packet[27] != 0) {
    throw std::invalid_argument("Client input has invalid button data");
  }

  ClientInput input{
      .moveX = readFloat(packet, 8),
      .moveZ = readFloat(packet, 12),
      .yaw = readFloat(packet, 16),
      .pitch = readFloat(packet, 20),
      .firePressed = packet[24] != 0,
  };
  validateInput(input);
  return input;
}

std::array<std::uint8_t, NetworkProtocol::kGameSnapshotPacketSize>
NetworkProtocol::encodeGameSnapshot(const GameSnapshot& snapshot) {
  validateSnapshot(snapshot);

  std::array<std::uint8_t, kGameSnapshotPacketSize> packet{};
  writeHeader(packet, kGameSnapshotType);
  packet[8] = static_cast<std::uint8_t>(snapshot.phase);
  packet[9] = static_cast<std::uint8_t>(snapshot.players.size());

  std::size_t offset = 12;
  for (const auto& player : snapshot.players) {
    writeU32(packet, offset, player.playerId);
    writeFloat(packet, offset + 4, player.position.x);
    writeFloat(packet, offset + 8, player.position.y);
    writeFloat(packet, offset + 12, player.position.z);
    writeFloat(packet, offset + 16, player.yaw);
    writeFloat(packet, offset + 20, player.pitch);
    writeI32(packet, offset + 24, player.health);
    writeI32(packet, offset + 28, player.maxHealth);
    offset += 32;
  }
  return packet;
}

GameSnapshot NetworkProtocol::decodeGameSnapshot(
    std::span<const std::uint8_t> packet) {
  validateHeader(packet, kGameSnapshotPacketSize, kGameSnapshotType);
  if (packet[9] != 2 || packet[10] != 0 || packet[11] != 0) {
    throw std::invalid_argument("Game snapshot has invalid player count");
  }

  GameSnapshot snapshot;
  switch (packet[8]) {
    case static_cast<std::uint8_t>(MatchPhase::Lobby):
      snapshot.phase = MatchPhase::Lobby;
      break;
    case static_cast<std::uint8_t>(MatchPhase::InGame):
      snapshot.phase = MatchPhase::InGame;
      break;
    case static_cast<std::uint8_t>(MatchPhase::MatchOver):
      snapshot.phase = MatchPhase::MatchOver;
      break;
    default:
      throw std::invalid_argument("Game snapshot has an invalid match phase");
  }

  std::size_t offset = 12;
  for (auto& player : snapshot.players) {
    player.playerId = readU32(packet, offset);
    player.position = Vector3{
        readFloat(packet, offset + 4),
        readFloat(packet, offset + 8),
        readFloat(packet, offset + 12),
    };
    player.yaw = readFloat(packet, offset + 16);
    player.pitch = readFloat(packet, offset + 20);
    player.health = readI32(packet, offset + 24);
    player.maxHealth = readI32(packet, offset + 28);
    offset += 32;
  }

  validateSnapshot(snapshot);
  return snapshot;
}

}  // namespace fireteam
