#include <doctest/doctest.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

#include "fireteam/NetworkManager.hpp"

using fireteam::NetworkEventType;
using fireteam::NetworkManager;

namespace {

bool waitForConnection(NetworkManager& server, NetworkManager& client) {
  bool serverConnected = false;
  bool clientConnected = false;
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(3);

  while (std::chrono::steady_clock::now() < deadline &&
         (!serverConnected || !clientConnected)) {
    if (auto event = server.pollEvent(2)) {
      serverConnected |= event->type == NetworkEventType::Connected;
    }
    if (auto event = client.pollEvent(2)) {
      clientConnected |= event->type == NetworkEventType::Connected;
    }
  }
  return serverConnected && clientConnected;
}

bool waitForPayload(NetworkManager& receiver,
                    const std::vector<std::uint8_t>& expected) {
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (std::chrono::steady_clock::now() < deadline) {
    if (auto event = receiver.pollEvent(2);
        event && event->type == NetworkEventType::PacketReceived) {
      return event->payload == expected;
    }
  }
  return false;
}

}  // namespace

TEST_CASE("Server and client connect over loopback and exchange packets") {
  auto server = NetworkManager::createServer(0);
  auto client = NetworkManager::connectTo("127.0.0.1", server.port());

  REQUIRE(server.isServer());
  CHECK_FALSE(client.isServer());
  CHECK(server.port() != 0);
  REQUIRE(waitForConnection(server, client));

  const std::array<std::uint8_t, 4> clientMessage{0x50, 0x49, 0x4E, 0x47};
  client.sendToServer(clientMessage);
  client.flush();
  CHECK(waitForPayload(
      server, std::vector<std::uint8_t>(clientMessage.begin(),
                                        clientMessage.end())));

  const std::array<std::uint8_t, 4> serverMessage{0x50, 0x4F, 0x4E, 0x47};
  server.broadcast(serverMessage);
  server.flush();
  CHECK(waitForPayload(
      client, std::vector<std::uint8_t>(serverMessage.begin(),
                                        serverMessage.end())));
}

TEST_CASE("NetworkManager safely transports a short opaque payload") {
  auto server = NetworkManager::createServer(0);
  auto client = NetworkManager::connectTo("127.0.0.1", server.port());
  REQUIRE(waitForConnection(server, client));

  // The transport does not parse application messages. It returns the exact
  // bytes so a future protocol decoder can validate them without unsafe reads.
  const std::array<std::uint8_t, 1> shortPayload{0x01};
  client.sendToServer(shortPayload);
  client.flush();
  CHECK(waitForPayload(
      server, std::vector<std::uint8_t>(shortPayload.begin(),
                                        shortPayload.end())));
}

TEST_CASE("Client cannot send before connection and wrong roles are rejected") {
  auto server = NetworkManager::createServer(0);
  auto client = NetworkManager::connectTo("127.0.0.1", server.port());
  const std::array<std::uint8_t, 1> payload{0x01};

  CHECK_THROWS_AS(client.sendToServer(payload), std::logic_error);
  CHECK_THROWS_AS(server.sendToServer(payload), std::logic_error);
  CHECK_THROWS_AS(client.broadcast(payload), std::logic_error);
  CHECK_THROWS_AS(NetworkManager::createServer(0, 0), std::invalid_argument);
  CHECK_THROWS_AS(NetworkManager::connectTo("", server.port()),
                  std::invalid_argument);
}

TEST_CASE("Moving a NetworkManager transfers ownership of its ENet host") {
  auto source = NetworkManager::createServer(0);
  const auto sourcePort = source.port();
  auto destination = NetworkManager::createServer(0);
  destination = std::move(source);

  CHECK(destination.isServer());
  CHECK(destination.port() == sourcePort);
}
