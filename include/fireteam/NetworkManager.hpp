#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <enet/enet.h>

namespace fireteam {

enum class NetworkEventType { Connected, Disconnected, PacketReceived };

struct NetworkEvent {
  NetworkEventType type;
  std::vector<std::uint8_t> payload;
};

class NetworkManager {
 public:
  static NetworkManager createServer(std::uint16_t port,
                                    std::size_t maxPeers = 8);
  static NetworkManager connectTo(std::string_view host, std::uint16_t port);

  NetworkManager(const NetworkManager&) = delete;
  NetworkManager& operator=(const NetworkManager&) = delete;
  NetworkManager(NetworkManager&&) noexcept = default;
  NetworkManager& operator=(NetworkManager&& other) noexcept;
  ~NetworkManager() = default;

  // Server sends to every connected client; client sends to its server.
  void broadcast(std::span<const std::uint8_t> payload, bool reliable = true);
  void sendToServer(std::span<const std::uint8_t> payload,
                    bool reliable = true);

  // Poll one event. timeoutMs == 0 is non-blocking.
  std::optional<NetworkEvent> pollEvent(std::uint32_t timeoutMs = 0);

  void flush();
  bool isServer() const { return isServer_; }
  std::uint16_t port() const { return port_; }

 private:
  struct Runtime;
  struct HostDeleter {
    void operator()(ENetHost* host) const noexcept;
  };
  using HostPtr = std::unique_ptr<ENetHost, HostDeleter>;

  static std::shared_ptr<Runtime> acquireRuntime();
  NetworkManager(std::shared_ptr<Runtime> runtime, HostPtr host,
                 ENetPeer* serverPeer, bool isServer, std::uint16_t port);

  std::shared_ptr<Runtime> runtime_;
  HostPtr host_;
  ENetPeer* serverPeer_;  // non-owning; owned by host_
  bool isServer_;
  std::uint16_t port_;
};

}  // namespace fireteam
