#include "fireteam/MatchSession.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace fireteam {
namespace {

bool expired(std::chrono::steady_clock::time_point since, float seconds) {
  return std::chrono::duration<float>(std::chrono::steady_clock::now() - since)
             .count() >= seconds;
}

bool validPosition(Vector3 position) {
  return std::isfinite(position.x) && std::isfinite(position.y) &&
         std::isfinite(position.z) && std::abs(position.x) <= 10000.0f &&
         std::abs(position.y) <= 10000.0f &&
         std::abs(position.z) <= 10000.0f;
}

}  // namespace

MatchSession::MatchSession(SessionOptions options) : options_(options) {
  for (float value : {options.maxStepSeconds, options.inputTimeoutSeconds,
                      options.connectionTimeoutSeconds,
                      options.peerTimeoutSeconds}) {
    if (!std::isfinite(value) || value <= 0.0f) {
      throw std::invalid_argument("Session time limits must be finite and positive");
    }
  }
  if (options.maxStepSeconds > 0.1f) {
    throw std::invalid_argument("Session simulation step cannot exceed 0.1 seconds");
  }
}

MatchSession::~MatchSession() { stop(); }

void MatchSession::host(std::uint16_t port) {
  auto network = std::make_unique<NetworkManager>(
      NetworkManager::createServer(port, 1));
  stop();
  network_ = std::move(network);
  localPlayerId_ = 1;
  started_ = lastPacket_ = Clock::now();
  resetPlayers(false);
}

void MatchSession::join(std::string_view host, std::uint16_t port) {
  auto network = std::make_unique<NetworkManager>(
      NetworkManager::connectTo(host, port));
  stop();
  network_ = std::move(network);
  localPlayerId_ = 2;
  started_ = lastPacket_ = Clock::now();
}

void MatchSession::stop() {
  if (network_) {
    network_->disconnect();
    network_.reset();
  }
  connected_ = false;
  localPlayerId_ = 0;
  snapshot_ = {};
  players_ = {};
  inputs_ = {};
}

void MatchSession::resetPlayers(bool includeClient) {
  players_[0] = std::make_unique<Player>(Vector3{-18.0f, 0.0f, 0.0f}, 100);
  players_[1] = includeClient
                    ? std::make_unique<Player>(Vector3{18.0f, 0.0f, 0.0f}, 100)
                    : nullptr;
  inputs_ = {};
  refreshSnapshot(includeClient ? MatchPhase::InGame : MatchPhase::Lobby);
}

void MatchSession::refreshSnapshot(MatchPhase phase) {
  snapshot_ = {};
  snapshot_.phase = phase;
  for (std::size_t i = 0; i < players_.size(); ++i) {
    if (players_[i]) {
      const auto& player = *players_[i];
      snapshot_.players[i] = {
          static_cast<std::uint32_t>(i + 1), player.position(), player.yaw(),
          player.pitch(), player.health(), player.maxHealth()};
    }
  }
}

void MatchSession::publishSnapshot() {
  if (connected_) {
    network_->broadcast(NetworkProtocol::encodeGameSnapshot(snapshot_));
    network_->flush();
  }
}

void MatchSession::requireHost() const {
  if (!isHost()) {
    throw std::logic_error("Operation requires an active authoritative host");
  }
}

void MatchSession::rememberInput(std::size_t slot, const PlayerInput& input) {
  auto& pending = inputs_[slot];
  pending.input = input;
  pending.input.firePressed = false;
  pending.fire |= input.firePressed;
  pending.age = 0.0f;
  pending.received = Clock::now();
}

void MatchSession::submitInput(const PlayerInput& input) {
  if (!isValidPlayerInput(input)) {
    throw std::invalid_argument("Invalid player movement or aim");
  }
  if (!active()) {
    throw std::logic_error("Input requires an active session");
  }
  if (!connected_ || snapshot_.phase != MatchPhase::InGame) {
    return;
  }
  rememberInput(localPlayerId_ - 1, input);
  if (!isHost()) {
    network_->sendToServer(NetworkProtocol::encodeClientInput(input));
    network_->flush();
    inputs_[1].fire = false;
  }
}

void MatchSession::lostPeer() {
  if (isHost()) {
    network_->disconnect();
    connected_ = false;
    resetPlayers(false);
  } else {
    stop();
  }
}

void MatchSession::receivePacket(const std::vector<std::uint8_t>& payload) {
  if (!connected_) {
    throw std::runtime_error("Received packet without a connected peer");
  }
  if (isHost()) {
    auto input = NetworkProtocol::decodeClientInput(payload);
    if (snapshot_.phase == MatchPhase::InGame) {
      rememberInput(1, input);
    }
  } else {
    auto snapshot = NetworkProtocol::decodeGameSnapshot(payload);
    if (snapshot.players[0].playerId != 1 ||
        snapshot.players[1].playerId != 2 ||
        snapshot.phase == MatchPhase::Lobby ||
        (snapshot.phase == MatchPhase::InGame &&
         (snapshot.players[0].health == 0 || snapshot.players[1].health == 0)) ||
        (snapshot.phase == MatchPhase::MatchOver &&
         ((snapshot.players[0].health == 0) ==
          (snapshot.players[1].health == 0)))) {
      throw std::invalid_argument("Snapshot violates two-player match state");
    }
    if (snapshot.phase != snapshot_.phase) {
      inputs_ = {};
    }
    snapshot_ = snapshot;
  }
  lastPacket_ = Clock::now();
}

void MatchSession::simulate(float deltaSeconds) {
  for (std::size_t i = 0; i < players_.size(); ++i) {
    if (snapshot_.phase != MatchPhase::InGame) {
      break;
    }
    auto& player = *players_[i];
    auto& pending = inputs_[i];
    pending.age += deltaSeconds;
    if (pending.age > options_.inputTimeoutSeconds ||
        expired(pending.received, options_.inputTimeoutSeconds)) {
      pending.input = {};
      pending.fire = false;
      continue;
    }
    if (!player.isAlive()) {
      pending.fire = false;
      continue;
    }
    const auto previous = player.position();
    player.applyInput(pending.input, deltaSeconds);
    if (collisionResolver_) {
      // Restore the old position if the resolver fails, before surfacing error.
      const auto desired = player.position();
      player.setPosition(previous);
      const auto resolved = collisionResolver_(static_cast<std::uint32_t>(i + 1),
                                                desired);
      if (!validPosition(resolved)) {
        throw std::invalid_argument("Collision resolver returned invalid position");
      }
      player.setPosition(resolved);
    } else if (!validPosition(player.position())) {
      player.setPosition(previous);
      throw std::runtime_error("Player moved outside supported world bounds");
    }
    const bool fire = std::exchange(pending.fire, false);
    if (fire && fireHandler_) {
      refreshSnapshot(MatchPhase::InGame);
      fireHandler_(static_cast<std::uint32_t>(i + 1));
    }
  }
  refreshSnapshot(snapshot_.phase);
}

void MatchSession::step(float deltaSeconds) {
  if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0f) {
    throw std::invalid_argument("Frame time must be finite and non-negative");
  }
  if (!active()) {
    return;
  }
  const float dt = std::min(deltaSeconds, options_.maxStepSeconds);
  // Bound work per frame: packet floods never create extra simulation steps.
  for (int count = 0; count < 128; ++count) {
    const auto event = network_->pollEvent();
    if (!event) {
      break;
    }
    switch (event->type) {
      case NetworkEventType::Connected:
        connected_ = true;
        lastPacket_ = Clock::now();
        if (isHost()) {
          resetPlayers(true);
          publishSnapshot();
        }
        break;
      case NetworkEventType::Disconnected:
        lostPeer();
        return;
      case NetworkEventType::PacketReceived:
        try {
          receivePacket(event->payload);
        } catch (const std::exception& error) {
          const std::string message = "Invalid peer packet: " + std::string(error.what());
          lostPeer();
          throw std::runtime_error(message);
        }
        break;
    }
  }
  if (!connected_ && !isHost() &&
      expired(started_, options_.connectionTimeoutSeconds)) {
    stop();
    throw std::runtime_error("Connection to host timed out");
  }
  if (connected_ && expired(lastPacket_, options_.peerTimeoutSeconds)) {
    lostPeer();
    throw std::runtime_error("Peer stopped responding");
  }
  if (isHost()) {
    if (snapshot_.phase == MatchPhase::InGame) {
      simulate(dt);
    }
    publishSnapshot();
  } else if (connected_) {
    auto& pending = inputs_[1];
    pending.age += dt;
    auto input = pending.input;
    if (pending.age > options_.inputTimeoutSeconds ||
        expired(pending.received, options_.inputTimeoutSeconds) ||
        snapshot_.phase != MatchPhase::InGame) {
      input = {};
    }
    // A heartbeat also releases stale movement; fire is sent only by submitInput.
    network_->sendToServer(NetworkProtocol::encodeClientInput(input));
    network_->flush();
  }
}

void MatchSession::applyDamage(std::uint32_t playerId, int amount) {
  requireHost();
  if (playerId < 1 || playerId > 2 || amount <= 0) {
    throw std::invalid_argument("Damage requires player ID 1 or 2 and a positive amount");
  }
  if (!connected_ || snapshot_.phase != MatchPhase::InGame) {
    throw std::logic_error("Damage requires a match in progress");
  }
  auto& target = *players_[playerId - 1];
  target.applyDamage(amount);
  refreshSnapshot(target.isAlive() ? MatchPhase::InGame : MatchPhase::MatchOver);
  if (!target.isAlive()) {
    inputs_ = {};
  }
  publishSnapshot();
}

void MatchSession::rematch() {
  requireHost();
  if (!connected_ || snapshot_.phase != MatchPhase::MatchOver) {
    throw std::logic_error("Rematch requires a connected completed match");
  }
  resetPlayers(true);
  publishSnapshot();
}

std::uint32_t MatchSession::winnerId() const {
  if (snapshot_.phase != MatchPhase::MatchOver) {
    return 0;
  }
  for (const auto& player : snapshot_.players) {
    if (player.playerId != 0 && player.health > 0) {
      return player.playerId;
    }
  }
  return 0;
}

void MatchSession::setCollisionResolver(CollisionResolver resolver) {
  collisionResolver_ = std::move(resolver);
}

void MatchSession::setFireHandler(FireHandler handler) {
  fireHandler_ = std::move(handler);
}

}  // namespace fireteam
