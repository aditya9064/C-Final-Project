#include <doctest/doctest.h>

#include <array>
#include <chrono>
#include <limits>
#include <stdexcept>
#include <thread>

#include "fireteam/MatchSession.hpp"

using fireteam::GameSnapshot;
using fireteam::MatchPhase;
using fireteam::MatchSession;
using fireteam::NetworkEventType;
using fireteam::NetworkManager;
using fireteam::NetworkProtocol;
using fireteam::PlayerInput;
using fireteam::SessionOptions;
using fireteam::Vector3;

namespace {

template <typename Predicate>
bool pumpUntil(MatchSession& host, MatchSession& client, Predicate predicate) {
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (std::chrono::steady_clock::now() < deadline) {
    host.step(0.0f);
    client.step(0.0f);
    if (predicate()) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return false;
}

bool connect(MatchSession& host, MatchSession& client) {
  host.host(0);
  client.join("127.0.0.1", host.port());
  return pumpUntil(host, client, [&] {
    return host.connected() && client.connected() &&
           client.snapshot().phase == MatchPhase::InGame;
  });
}

bool connectRaw(MatchSession& host, NetworkManager& client) {
  bool clientConnected = false;
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (std::chrono::steady_clock::now() < deadline) {
    host.step(0.0f);
    if (auto event = client.pollEvent(1)) {
      clientConnected |= event->type == NetworkEventType::Connected;
    }
    if (host.connected() && clientConnected) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_CASE("Match sessions connect with fixed identities and authoritative spawns") {
  MatchSession host;
  MatchSession client;
  CHECK_FALSE(host.active());
  CHECK(host.localPlayerId() == 0);
  REQUIRE(connect(host, client));
  CHECK(host.isHost());
  CHECK_FALSE(client.isHost());
  CHECK(host.localPlayerId() == 1);
  CHECK(client.localPlayerId() == 2);
  CHECK(host.port() != 0);
  CHECK(host.snapshot().phase == MatchPhase::InGame);
  CHECK(client.snapshot().players[0].playerId == 1);
  CHECK(client.snapshot().players[1].playerId == 2);
  CHECK(client.snapshot().players[0].position == Vector3{-18.0f, 0.0f, 0.0f});
  CHECK(client.snapshot().players[1].position == Vector3{18.0f, 0.0f, 0.0f});
  CHECK(client.snapshot().players[1].health == 100);
  CHECK(host.winnerId() == 0);
}

TEST_CASE("Client inputs move only on host time and replicate in snapshots") {
  MatchSession host;
  MatchSession client;
  REQUIRE(connect(host, client));

  client.submitInput(PlayerInput{.moveX = 1.0f, .yaw = 0.0f, .pitch = 0.2f});
  REQUIRE(pumpUntil(host, client, [&] {
    return host.snapshot().players[1].pitch == 0.2f;
  }));
  CHECK(host.snapshot().players[1].position.x == doctest::Approx(18.0f));
  client.step(1000.0f);
  CHECK(client.snapshot().players[1].position.x == doctest::Approx(18.0f));

  host.step(1000.0f);
  CHECK(host.snapshot().players[1].position.x == doctest::Approx(18.5f));
  REQUIRE(pumpUntil(host, client, [&] {
    return client.snapshot().players[1].position.x == 18.5f;
  }));

  host.submitInput(PlayerInput{.moveZ = 1.0f});
  host.step(0.1f);
  CHECK(host.snapshot().players[0].position.z == doctest::Approx(-0.5f));
}

TEST_CASE("Stale input is released without moving or repeating fire forever") {
  MatchSession host;
  MatchSession client;
  REQUIRE(connect(host, client));
  int fires = 0;
  host.setFireHandler([&](std::uint32_t playerId, float, float, int, float) {
    CHECK(playerId == 1);
    ++fires;
  });
  host.submitInput(PlayerInput{.moveX = 1.0f, .firePressed = true});
  host.step(0.1f);
  host.step(0.1f);
  host.step(0.1f);
  CHECK(fires == 1);
  const auto stopped = host.snapshot().players[0].position;
  for (int i = 0; i < 10; ++i) {
    host.step(0.1f);
  }
  CHECK(host.snapshot().players[0].position == stopped);
}

TEST_CASE("Host enforces client shot cooldown and replicates reload state") {
  MatchSession host;
  MatchSession client;
  REQUIRE(connect(host, client));
  int shots = 0;
  float shotYaw = 0.0f;
  host.setFireHandler([&](std::uint32_t id, float yaw, float, int damage, float range) {
    CHECK(id == 2);
    CHECK(damage == 25);
    CHECK(range == 300.0f);
    shotYaw = yaw;
    ++shots;
  });
  client.submitInput(PlayerInput{.yaw = 1.0f, .firePressed = true});
  client.submitInput(PlayerInput{.yaw = 0.0f});
  REQUIRE(pumpUntil(host, client, [&] { return shots == 1; }));
  CHECK(shotYaw == doctest::Approx(1.0f));
  CHECK(host.snapshot().players[1].ammo == 29);
  client.submitInput(PlayerInput{.firePressed = true});
  for (int i = 0; i < 10; ++i) {
    host.step(0.0f);
    client.step(0.0f);
  }
  CHECK(shots == 1);
  client.submitInput(PlayerInput{.reloadPressed = true});
  REQUIRE(pumpUntil(host, client, [&] { return client.snapshot().players[1].reloading; }));
  CHECK(client.snapshot().players[1].ammo == 29);
  client.submitInput(PlayerInput{.firePressed = true});
  host.step(0.1f);
  for (int i = 0; i < 20; ++i) {
    host.step(0.1f);
    client.step(0.0f);
  }
  CHECK_FALSE(host.snapshot().players[1].reloading);
  CHECK(host.snapshot().players[1].ammo == 30);
  CHECK(shots == 1);
}

TEST_CASE("Collision and fire callbacks use authoritative state and allow elimination") {
  MatchSession host;
  MatchSession client;
  REQUIRE(connect(host, client));
  int collisions = 0;
  host.setCollisionResolver([&](std::uint32_t id, Vector3 desired) {
    ++collisions;
    if (id == 1) {
      CHECK(desired.x == doctest::Approx(-17.5f));
      desired.x = -18.0f;
    }
    return desired;
  });
  host.setFireHandler([&](std::uint32_t id, float, float, int, float) {
    CHECK(id == 1);
    host.applyDamage(2, 100);
  });
  host.submitInput(PlayerInput{.moveX = 1.0f, .firePressed = true});
  host.step(0.1f);
  CHECK(collisions == 1);
  CHECK(host.snapshot().players[0].position.x == doctest::Approx(-18.0f));
  CHECK(host.snapshot().phase == MatchPhase::MatchOver);
  CHECK(host.winnerId() == 1);
  REQUIRE(pumpUntil(host, client, [&] { return client.winnerId() == 1; }));
  CHECK(client.snapshot().players[1].health == 0);
}

TEST_CASE("Damage winner freezing and rematch are host-authoritative") {
  MatchSession host;
  MatchSession client;
  REQUIRE(connect(host, client));
  CHECK_THROWS_AS(client.applyDamage(1, 50), std::logic_error);
  CHECK_THROWS_AS(client.rematch(), std::logic_error);
  CHECK_THROWS_AS(host.applyDamage(0, 10), std::invalid_argument);
  CHECK_THROWS_AS(host.applyDamage(3, 10), std::invalid_argument);
  CHECK_THROWS_AS(host.applyDamage(2, 0), std::invalid_argument);
  CHECK_THROWS_AS(host.rematch(), std::logic_error);
  host.applyDamage(1, 25);
  CHECK(host.snapshot().players[0].health == 75);
  host.applyDamage(1, 1000);
  CHECK(host.winnerId() == 2);
  REQUIRE(pumpUntil(host, client, [&] { return client.winnerId() == 2; }));
  const auto frozen = host.snapshot().players[0].position;
  host.submitInput(PlayerInput{.moveX = 1.0f, .firePressed = true});
  host.step(0.1f);
  CHECK(host.snapshot().players[0].position == frozen);
  CHECK_THROWS_AS(host.applyDamage(2, 10), std::logic_error);

  host.rematch();
  CHECK(host.snapshot().phase == MatchPhase::InGame);
  CHECK(host.snapshot().players[0].health == 100);
  CHECK(host.snapshot().players[1].health == 100);
  CHECK(host.snapshot().players[0].yaw == 0.0f);
  CHECK(host.winnerId() == 0);
  REQUIRE(pumpUntil(host, client, [&] {
    return client.snapshot().phase == MatchPhase::InGame;
  }));
  CHECK(client.winnerId() == 0);
  CHECK(client.snapshot().players[0].health == 100);
}

TEST_CASE("Disconnect returns host to Lobby and allows a fresh client") {
  MatchSession host;
  MatchSession client;
  REQUIRE(connect(host, client));
  host.applyDamage(1, 20);
  client.stop();
  REQUIRE(pumpUntil(host, client, [&] { return !host.connected(); }));
  CHECK(host.active());
  CHECK(host.localPlayerId() == 1);
  CHECK(host.snapshot().phase == MatchPhase::Lobby);
  CHECK(host.snapshot().players[0].health == 100);
  CHECK(host.snapshot().players[1].playerId == 0);
  CHECK(client.localPlayerId() == 0);
  CHECK(client.snapshot().phase == MatchPhase::Lobby);
  CHECK_FALSE(client.active());

  client.join("127.0.0.1", host.port());
  REQUIRE(pumpUntil(host, client, [&] {
    return client.snapshot().phase == MatchPhase::InGame;
  }));
  CHECK(host.snapshot().players[1].health == 100);
  host.stop();
  REQUIRE(pumpUntil(host, client, [&] { return !client.active(); }));
  CHECK(client.localPlayerId() == 0);
  CHECK(client.winnerId() == 0);
}

TEST_CASE("Session validates arguments and ignores input outside an active match") {
  MatchSession session;
  CHECK_THROWS_AS(session.submitInput({}), std::logic_error);
  CHECK_THROWS_AS(session.step(-1.0f), std::invalid_argument);
  CHECK_THROWS_AS(session.step(std::numeric_limits<float>::quiet_NaN()),
                  std::invalid_argument);
  CHECK_THROWS_AS(session.join("", 1234), std::invalid_argument);
  CHECK_THROWS_AS(session.join("127.0.0.1", 0), std::invalid_argument);
  CHECK_THROWS_AS(MatchSession(SessionOptions{.maxStepSeconds = 1.0f}),
                  std::invalid_argument);
  CHECK_THROWS_AS(MatchSession(SessionOptions{.peerTimeoutSeconds = 0.0f}),
                  std::invalid_argument);
  session.host(0);
  CHECK_NOTHROW(session.submitInput(PlayerInput{.moveX = 1.0f}));
  session.step(0.1f);
  CHECK(session.snapshot().players[0].position.x == doctest::Approx(-18.0f));
  CHECK_THROWS_AS(session.submitInput(PlayerInput{.moveX = 2.0f}),
                  std::invalid_argument);
  CHECK_THROWS_AS(session.applyDamage(1, 10), std::logic_error);
}

TEST_CASE("Invalid collision results are surfaced and do not corrupt position") {
  MatchSession host;
  MatchSession client;
  REQUIRE(connect(host, client));
  host.setCollisionResolver([](std::uint32_t, Vector3) {
    return Vector3{std::numeric_limits<float>::infinity(), 0.0f, 0.0f};
  });
  host.submitInput(PlayerInput{.moveX = 1.0f});
  CHECK_THROWS_AS(host.step(0.1f), std::invalid_argument);
  CHECK(host.snapshot().players[0].position.x == doctest::Approx(-18.0f));
}

TEST_CASE("Client connection timeout clears session with an explicit error") {
  auto unused = NetworkManager::createServer(0);
  const auto port = unused.port();
  MatchSession client{SessionOptions{.connectionTimeoutSeconds = 0.02f}};
  client.join("127.0.0.1", port);
  CHECK_NOTHROW(client.submitInput({}));
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  CHECK_THROWS_AS(client.step(0.0f), std::runtime_error);
  CHECK_FALSE(client.active());
  CHECK_FALSE(client.connected());
  CHECK(client.localPlayerId() == 0);
}

TEST_CASE("Nonresponsive connected peer times out and frees the host slot") {
  MatchSession host{SessionOptions{.peerTimeoutSeconds = 0.05f}};
  host.host(0);
  auto rawClient = NetworkManager::connectTo("127.0.0.1", host.port());
  REQUIRE(connectRaw(host, rawClient));
  std::this_thread::sleep_for(std::chrono::milliseconds(60));
  CHECK_THROWS_AS(host.step(0.0f), std::runtime_error);
  CHECK(host.active());
  CHECK_FALSE(host.connected());
  CHECK(host.snapshot().phase == MatchPhase::Lobby);
  auto replacement = NetworkManager::connectTo("127.0.0.1", host.port());
  REQUIRE(connectRaw(host, replacement));
}

TEST_CASE("A client cannot impersonate authoritative position or health packets") {
  MatchSession host;
  host.host(0);
  auto client = NetworkManager::connectTo("127.0.0.1", host.port());
  REQUIRE(connectRaw(host, client));
  auto forged = host.snapshot();
  forged.players[0].health = 0;
  client.sendToServer(NetworkProtocol::encodeGameSnapshot(forged));
  client.flush();
  bool rejected = false;
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (!rejected && std::chrono::steady_clock::now() < deadline) {
    try {
      host.step(0.0f);
    } catch (const std::runtime_error&) {
      rejected = true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(rejected);
  CHECK_FALSE(host.connected());
  CHECK(host.snapshot().phase == MatchPhase::Lobby);
  CHECK(host.snapshot().players[0].health == 100);
}

TEST_CASE("Remote fire is consumed once even when a release heartbeat follows it") {
  MatchSession host;
  MatchSession client;
  REQUIRE(connect(host, client));
  int fires = 0;
  host.setFireHandler([&](std::uint32_t id, float, float, int, float) {
    CHECK(id == 2);
    ++fires;
  });
  client.submitInput(PlayerInput{.firePressed = true});
  client.step(0.0f);
  REQUIRE(pumpUntil(host, client, [&] { return fires == 1; }));
  for (int i = 0; i < 5; ++i) {
    host.step(0.01f);
    client.step(0.01f);
  }
  CHECK(fires == 1);
}

TEST_CASE("Rematch does not replay the client's movement from the previous match") {
  MatchSession host;
  MatchSession client;
  REQUIRE(connect(host, client));
  client.submitInput(PlayerInput{.moveX = 1.0f, .pitch = 0.1f});
  REQUIRE(pumpUntil(host, client, [&] {
    return host.snapshot().players[1].pitch == 0.1f;
  }));
  host.applyDamage(1, 100);
  REQUIRE(pumpUntil(host, client, [&] { return client.winnerId() == 2; }));
  host.rematch();
  REQUIRE(pumpUntil(host, client, [&] {
    return client.snapshot().phase == MatchPhase::InGame;
  }));
  for (int i = 0; i < 3; ++i) {
    client.step(0.01f);
    host.step(0.01f);
  }
  CHECK(host.snapshot().players[1].position.x == doctest::Approx(18.0f));
  CHECK(host.snapshot().players[1].pitch == 0.0f);
}

TEST_CASE("Input flooding cannot multiply elapsed host simulation") {
  MatchSession host;
  host.host(0);
  auto rawClient = NetworkManager::connectTo("127.0.0.1", host.port());
  REQUIRE(connectRaw(host, rawClient));
  const auto input = NetworkProtocol::encodeClientInput(
      PlayerInput{.moveX = 1.0f, .pitch = 0.3f});
  for (int i = 0; i < 200; ++i) {
    rawClient.sendToServer(input);
  }
  rawClient.flush();
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (host.snapshot().players[1].pitch != 0.3f &&
         std::chrono::steady_clock::now() < deadline) {
    host.step(0.0f);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  REQUIRE(host.snapshot().players[1].pitch == 0.3f);
  CHECK(host.snapshot().players[1].position.x == doctest::Approx(18.0f));
  host.step(999999.0f);
  CHECK(host.snapshot().players[1].position.x == doctest::Approx(18.5f));
}

TEST_CASE("A nonresponsive host also times out a client that keeps stepping") {
  MatchSession client{SessionOptions{.peerTimeoutSeconds = 0.05f}};
  auto rawHost = NetworkManager::createServer(0, 1);
  client.join("127.0.0.1", rawHost.port());
  bool serverConnected = false;
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while ((!client.connected() || !serverConnected) &&
         std::chrono::steady_clock::now() < deadline) {
    if (auto event = rawHost.pollEvent(1)) {
      serverConnected |= event->type == NetworkEventType::Connected;
    }
    client.step(0.0f);
  }
  REQUIRE(serverConnected);
  REQUIRE(client.connected());
  std::this_thread::sleep_for(std::chrono::milliseconds(60));
  CHECK_THROWS_AS(client.step(0.0f), std::runtime_error);
  CHECK_FALSE(client.active());
  CHECK(client.localPlayerId() == 0);
}

TEST_CASE("Only one remote peer may occupy the second player slot") {
  MatchSession host;
  MatchSession client;
  REQUIRE(connect(host, client));
  MatchSession extra{SessionOptions{.connectionTimeoutSeconds = 0.03f}};
  extra.join("127.0.0.1", host.port());
  bool timedOut = false;
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(1);
  while (!timedOut && std::chrono::steady_clock::now() < deadline) {
    host.step(0.0f);
    client.step(0.0f);
    try {
      extra.step(0.0f);
    } catch (const std::runtime_error&) {
      timedOut = true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(timedOut);
  CHECK_FALSE(extra.active());
  CHECK(host.connected());
  CHECK(client.connected());
  CHECK(host.snapshot().players[1].playerId == 2);
  CHECK(host.snapshot().phase == MatchPhase::InGame);
}
