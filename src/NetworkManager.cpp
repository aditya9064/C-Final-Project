#include "fireteam/NetworkManager.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace fireteam {

struct NetworkManager::Runtime {
  Runtime() {
    if (enet_initialize() != 0) {
      throw std::runtime_error("Failed to initialize ENet");
    }
  }

  ~Runtime() { enet_deinitialize(); }
};

namespace {

ENetPacket* makePacket(std::span<const std::uint8_t> payload, bool reliable) {
  const enet_uint32 flags = reliable ? ENET_PACKET_FLAG_RELIABLE : 0;
  ENetPacket* packet = enet_packet_create(payload.data(), payload.size(), flags);
  if (!packet) {
    throw std::runtime_error("Failed to create ENet packet");
  }
  return packet;
}

}  // namespace

std::shared_ptr<NetworkManager::Runtime> NetworkManager::acquireRuntime() {
  // ENet initialization is process-wide. Keep one shared runtime alive until
  // shutdown so destroying one manager cannot deinitialize ENet while
  // another manager still owns a host.
  static const auto runtime = std::make_shared<Runtime>();
  return runtime;
}

void NetworkManager::HostDeleter::operator()(ENetHost* host) const noexcept {
  if (host) {
    enet_host_destroy(host);
  }
}

NetworkManager::NetworkManager(std::shared_ptr<Runtime> runtime, HostPtr host,
                               ENetPeer* serverPeer, bool isServer,
                               std::uint16_t port)
    : runtime_(std::move(runtime)),
      host_(std::move(host)),
      serverPeer_(serverPeer),
      isServer_(isServer),
      port_(port) {}

NetworkManager& NetworkManager::operator=(NetworkManager&& other) noexcept {
  if (this != &other) {
    // Destroy the host before releasing its ENet runtime.
    host_.reset();
    runtime_.reset();
    runtime_ = std::move(other.runtime_);
    host_ = std::move(other.host_);
    serverPeer_ = std::exchange(other.serverPeer_, nullptr);
    isServer_ = other.isServer_;
    port_ = other.port_;
  }
  return *this;
}

NetworkManager NetworkManager::createServer(std::uint16_t port,
                                           std::size_t maxPeers) {
  if (maxPeers == 0) {
    throw std::invalid_argument("Server must allow at least one peer");
  }

  auto runtime = acquireRuntime();
  ENetAddress address{};
  address.host = ENET_HOST_ANY;
  address.port = port;

  HostPtr host{enet_host_create(&address, maxPeers, 1, 0, 0)};
  if (!host) {
    throw std::runtime_error("Failed to create ENet server host");
  }

  ENetAddress boundAddress{};
  if (enet_socket_get_address(host->socket, &boundAddress) < 0) {
    throw std::runtime_error("Failed to get bound ENet server port");
  }

  return NetworkManager{std::move(runtime), std::move(host), nullptr, true,
                        boundAddress.port};
}

NetworkManager NetworkManager::connectTo(std::string_view hostName,
                                         std::uint16_t port) {
  if (hostName.empty()) {
    throw std::invalid_argument("Server address cannot be empty");
  }

  auto runtime = acquireRuntime();
  HostPtr host{enet_host_create(nullptr, 1, 1, 0, 0)};
  if (!host) {
    throw std::runtime_error("Failed to create ENet client host");
  }

  ENetAddress address{};
  address.port = port;
  const std::string addressText{hostName};
  if (enet_address_set_host(&address, addressText.c_str()) < 0) {
    throw std::invalid_argument("Invalid or unresolvable server address");
  }

  ENetPeer* peer = enet_host_connect(host.get(), &address, 1, 0);
  if (!peer) {
    throw std::runtime_error("Failed to start ENet connection");
  }

  return NetworkManager{std::move(runtime), std::move(host), peer, false, port};
}

void NetworkManager::broadcast(std::span<const std::uint8_t> payload,
                               bool reliable) {
  if (!isServer_) {
    throw std::logic_error("Only a server can broadcast");
  }
  enet_host_broadcast(host_.get(), 0, makePacket(payload, reliable));
}

void NetworkManager::sendToServer(std::span<const std::uint8_t> payload,
                                  bool reliable) {
  if (isServer_ || !serverPeer_ ||
      serverPeer_->state != ENET_PEER_STATE_CONNECTED) {
    throw std::logic_error("Client is not connected to a server");
  }

  ENetPacket* packet = makePacket(payload, reliable);
  if (enet_peer_send(serverPeer_, 0, packet) < 0) {
    enet_packet_destroy(packet);
    throw std::runtime_error("Failed to queue ENet packet");
  }
}

std::optional<NetworkEvent> NetworkManager::pollEvent(std::uint32_t timeoutMs) {
  ENetEvent event{};
  const int result = enet_host_service(host_.get(), &event, timeoutMs);
  if (result < 0) {
    throw std::runtime_error("ENet failed while polling for events");
  }
  if (result == 0) {
    return std::nullopt;
  }

  switch (event.type) {
    case ENET_EVENT_TYPE_CONNECT:
      return NetworkEvent{NetworkEventType::Connected, {}};

    case ENET_EVENT_TYPE_DISCONNECT:
      if (event.peer == serverPeer_) {
        serverPeer_ = nullptr;
      }
      return NetworkEvent{NetworkEventType::Disconnected, {}};

    case ENET_EVENT_TYPE_RECEIVE: {
      std::unique_ptr<ENetPacket, decltype(&enet_packet_destroy)> packet{
          event.packet, &enet_packet_destroy};
      NetworkEvent received{NetworkEventType::PacketReceived, {}};
      if (packet->dataLength > 0) {
        received.payload.assign(packet->data,
                                packet->data + packet->dataLength);
      }
      return received;
    }

    case ENET_EVENT_TYPE_NONE:
      return std::nullopt;
  }
  throw std::runtime_error("ENet returned an unknown event type");
}

void NetworkManager::flush() { enet_host_flush(host_.get()); }

}  // namespace fireteam
