#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>

#include "fireteam/NetworkProtocol.hpp"

using fireteam::ClientInput;
using fireteam::GameSnapshot;
using fireteam::MatchPhase;
using fireteam::NetworkProtocol;
using fireteam::PlayerSnapshot;
using fireteam::Vector3;

TEST_CASE("Client input encodes and decodes movement and aim") {
  const ClientInput input{
      .moveX = -0.5f,
      .moveZ = 0.5f,
      .yaw = 1.2f,
      .pitch = -0.4f,
      .firePressed = true,
  };

  const auto packet = NetworkProtocol::encodeClientInput(input);
  const auto decoded = NetworkProtocol::decodeClientInput(packet);

  CHECK(packet.size() == NetworkProtocol::kClientInputPacketSize);
  CHECK(decoded.moveX == doctest::Approx(input.moveX));
  CHECK(decoded.moveZ == doctest::Approx(input.moveZ));
  CHECK(decoded.yaw == doctest::Approx(input.yaw));
  CHECK(decoded.pitch == doctest::Approx(input.pitch));
  CHECK(decoded.firePressed);
}

TEST_CASE("Client input rejects invalid values on encode and decode") {
  ClientInput invalid;
  invalid.moveX = 1.01f;
  CHECK_THROWS_AS(NetworkProtocol::encodeClientInput(invalid),
                  std::invalid_argument);

  invalid = {};
  invalid.moveX = 1.0f;
  invalid.moveZ = 1.0f;
  CHECK_THROWS_AS(NetworkProtocol::encodeClientInput(invalid),
                  std::invalid_argument);

  invalid = {};
  invalid.pitch = std::numeric_limits<float>::quiet_NaN();
  CHECK_THROWS_AS(NetworkProtocol::encodeClientInput(invalid),
                  std::invalid_argument);

  auto packet = NetworkProtocol::encodeClientInput({});
  packet[24] = 2;
  CHECK_THROWS_AS(NetworkProtocol::decodeClientInput(packet),
                  std::invalid_argument);

  packet = NetworkProtocol::encodeClientInput({});
  packet[16] = 0;
  packet[17] = 0;
  packet[18] = 0x80;
  packet[19] = 0x40;  // 4.0f, outside the permitted yaw range.
  CHECK_THROWS_AS(NetworkProtocol::decodeClientInput(packet),
                  std::invalid_argument);
}

TEST_CASE("Protocol rejects bad signatures, versions, types, and lengths") {
  auto inputPacket = NetworkProtocol::encodeClientInput({});
  inputPacket[0] = 'X';
  CHECK_THROWS_AS(NetworkProtocol::decodeClientInput(inputPacket),
                  std::invalid_argument);

  inputPacket = NetworkProtocol::encodeClientInput({});
  inputPacket[4] = 2;
  CHECK_THROWS_AS(NetworkProtocol::decodeClientInput(inputPacket),
                  std::invalid_argument);

  const auto snapshotPacket = NetworkProtocol::encodeGameSnapshot({});
  CHECK_THROWS_AS(NetworkProtocol::decodeClientInput(snapshotPacket),
                  std::invalid_argument);
  CHECK_THROWS_AS(NetworkProtocol::decodeClientInput(
                      std::span<const std::uint8_t>{inputPacket.data(), 7}),
                  std::invalid_argument);
}

TEST_CASE("Host game snapshot round-trips both players and match phase") {
  GameSnapshot snapshot{
      .phase = MatchPhase::InGame,
      .players = {{
          PlayerSnapshot{1, Vector3{-4.0f, 1.7f, 2.5f}, 0.2f, -0.1f, 75, 100},
          PlayerSnapshot{2, Vector3{3.0f, 0.0f, -2.0f}, -1.1f, 0.3f, 0, 100},
      }},
  };

  const auto packet = NetworkProtocol::encodeGameSnapshot(snapshot);
  const auto decoded = NetworkProtocol::decodeGameSnapshot(packet);

  CHECK(packet.size() == NetworkProtocol::kGameSnapshotPacketSize);
  CHECK(decoded.phase == MatchPhase::InGame);
  CHECK(decoded.players[0].playerId == 1);
  CHECK(decoded.players[0].position == Vector3{-4.0f, 1.7f, 2.5f});
  CHECK(decoded.players[0].yaw == doctest::Approx(0.2f));
  CHECK(decoded.players[0].pitch == doctest::Approx(-0.1f));
  CHECK(decoded.players[0].health == 75);
  CHECK(decoded.players[1].playerId == 2);
  CHECK(decoded.players[1].health == 0);
  CHECK(decoded.players[1].maxHealth == 100);
}

TEST_CASE("Snapshot supports an empty player slot") {
  GameSnapshot snapshot;
  snapshot.players[0] =
      PlayerSnapshot{1, Vector3{1.0f, 0.0f, 2.0f}, 0.0f, 0.0f, 100, 100};

  const auto decoded =
      NetworkProtocol::decodeGameSnapshot(
          NetworkProtocol::encodeGameSnapshot(snapshot));
  CHECK(decoded.players[0].playerId == 1);
  CHECK(decoded.players[1].playerId == 0);
  CHECK(decoded.players[1].health == 0);
}

TEST_CASE("Game snapshots reject invalid player state") {
  GameSnapshot snapshot;
  snapshot.players[0] =
      PlayerSnapshot{1, Vector3{}, 0.0f, 0.0f, 101, 100};
  CHECK_THROWS_AS(NetworkProtocol::encodeGameSnapshot(snapshot),
                  std::invalid_argument);

  snapshot.players[0] =
      PlayerSnapshot{1, Vector3{}, 0.0f, 0.0f, 100, 100};
  snapshot.players[1] =
      PlayerSnapshot{1, Vector3{}, 0.0f, 0.0f, 100, 100};
  CHECK_THROWS_AS(NetworkProtocol::encodeGameSnapshot(snapshot),
                  std::invalid_argument);

  snapshot.players[1] =
      PlayerSnapshot{2, Vector3{std::numeric_limits<float>::infinity(), 0, 0},
                     0.0f, 0.0f, 100, 100};
  CHECK_THROWS_AS(NetworkProtocol::encodeGameSnapshot(snapshot),
                  std::invalid_argument);
}

TEST_CASE("Game snapshots reject invalid wire phase, count, and health") {
  auto packet = NetworkProtocol::encodeGameSnapshot({});
  packet[8] = 99;
  CHECK_THROWS_AS(NetworkProtocol::decodeGameSnapshot(packet),
                  std::invalid_argument);

  packet = NetworkProtocol::encodeGameSnapshot({});
  packet[9] = 3;
  CHECK_THROWS_AS(NetworkProtocol::decodeGameSnapshot(packet),
                  std::invalid_argument);

  packet = NetworkProtocol::encodeGameSnapshot({});
  packet[12] = 1;  // First player ID is non-zero, but its health is invalid.
  CHECK_THROWS_AS(NetworkProtocol::decodeGameSnapshot(packet),
                  std::invalid_argument);

  packet = NetworkProtocol::encodeGameSnapshot({});
  packet[12] = 1;
  packet[16] = 0;
  packet[17] = 0;
  packet[18] = 0x80;
  packet[19] = 0x7F;  // Infinite X coordinate.
  CHECK_THROWS_AS(NetworkProtocol::decodeGameSnapshot(packet),
                  std::invalid_argument);
}
