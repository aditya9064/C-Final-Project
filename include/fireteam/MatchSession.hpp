#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <vector>

#include "fireteam/NetworkManager.hpp"
#include "fireteam/NetworkProtocol.hpp"

namespace fireteam {

struct SessionOptions {
  float maxStepSeconds = 0.1f;
  float inputTimeoutSeconds = 0.25f;
  float connectionTimeoutSeconds = 5.0f;
  float peerTimeoutSeconds = 10.0f;
};

// One authoritative host (ID 1) and one input-only client (ID 2).
// Call step regularly on both peers, even in Lobby and MatchOver.
class MatchSession {
 public:
  using CollisionResolver = std::function<Vector3(std::uint32_t, Vector3)>;
  using FireHandler = std::function<void(std::uint32_t)>;

  explicit MatchSession(SessionOptions options = {});
  ~MatchSession();
  MatchSession(const MatchSession&) = delete;
  MatchSession& operator=(const MatchSession&) = delete;

  void host(std::uint16_t port);
  void join(std::string_view host, std::uint16_t port);
  void stop();
  // Host-owned time only: clamp simulation to maxStepSeconds, discard excess.
  // Timeouts/malformed packets disconnect the peer and throw runtime_error.
  void step(float deltaSeconds);
  // Valid input during Lobby/connecting/MatchOver is ignored, not buffered.
  void submitInput(const PlayerInput& input);
  void applyDamage(std::uint32_t playerId, int amount);
  void rematch();

  const GameSnapshot& snapshot() const { return snapshot_; }
  std::uint32_t localPlayerId() const { return localPlayerId_; }
  // Host's bound port, or the client's destination port; zero when stopped.
  std::uint16_t boundPort() const { return network_ ? network_->port() : 0; }
  std::uint16_t port() const { return boundPort(); }
  bool active() const { return network_ != nullptr; }
  bool connected() const { return connected_; }
  bool isHost() const { return network_ && network_->isServer(); }
  std::uint32_t winnerId() const;

  // Invoked only on the host. Fire may synchronously call applyDamage().
  void setCollisionResolver(CollisionResolver resolver);
  void setFireHandler(FireHandler handler);

 private:
  using Clock = std::chrono::steady_clock;
  struct PendingInput {
    PlayerInput input;
    float age = 0.0f;
    Clock::time_point received{};
    bool fire = false;
  };

  void resetPlayers(bool includeClient);
  void refreshSnapshot(MatchPhase phase);
  void publishSnapshot();
  void lostPeer();
  void receivePacket(const std::vector<std::uint8_t>& payload);
  void rememberInput(std::size_t slot, const PlayerInput& input);
  void simulate(float deltaSeconds);
  void requireHost() const;

  SessionOptions options_;
  std::unique_ptr<NetworkManager> network_;
  std::array<std::unique_ptr<Player>, 2> players_;
  std::array<PendingInput, 2> inputs_{};
  GameSnapshot snapshot_;
  std::uint32_t localPlayerId_ = 0;
  bool connected_ = false;
  Clock::time_point started_{};
  Clock::time_point lastPacket_{};
  CollisionResolver collisionResolver_;
  FireHandler fireHandler_;
};

}  // namespace fireteam
