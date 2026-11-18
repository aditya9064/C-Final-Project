#include "fireteam_session.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <godot_cpp/classes/capsule_mesh.hpp>
#include <godot_cpp/classes/capsule_shape3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/kinematic_collision3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace fireteam {
namespace {

godot::Vector3 to_godot(Vector3 value) { return {value.x, value.y, value.z}; }
Vector3 from_godot(godot::Vector3 value) {
  return {static_cast<float>(value.x), static_cast<float>(value.y),
          static_cast<float>(value.z)};
}

std::uint16_t validate_port(int port, bool allowZero) {
  if (port < (allowZero ? 0 : 1) || port > 65535) {
    throw std::invalid_argument("Port must be between 1 and 65535 (host may use 0)");
  }
  return static_cast<std::uint16_t>(port);
}

}  // namespace

void FireteamSession::_bind_methods() {
  using namespace godot;
  ClassDB::bind_method(D_METHOD("host_game", "port"), &FireteamSession::host_game);
  ClassDB::bind_method(D_METHOD("join_game", "address", "port"),
                       &FireteamSession::join_game);
  ClassDB::bind_method(D_METHOD("stop_game"), &FireteamSession::stop_game);
  ClassDB::bind_method(D_METHOD("set_input", "move_x", "move_z", "yaw", "pitch", "fire"),
                       &FireteamSession::set_input);
  ClassDB::bind_method(D_METHOD("damage_player", "player_id", "amount"),
                       &FireteamSession::damage_player);
  ClassDB::bind_method(D_METHOD("restart_match"), &FireteamSession::restart_match);
  ClassDB::bind_method(D_METHOD("get_state"), &FireteamSession::get_state);
  ADD_SIGNAL(MethodInfo("session_error", PropertyInfo(Variant::STRING, "message")));
  ADD_SIGNAL(MethodInfo("fire_requested", PropertyInfo(Variant::INT, "player_id")));
}

void FireteamSession::_ready() {
  for (std::size_t i = 0; i < bodies_.size(); ++i) {
    auto* body = memnew(godot::CharacterBody3D);
    body->set_name(i == 0 ? "HostPlayer" : "ClientPlayer");
    body->set_collision_layer(2);
    body->set_collision_mask(1);
    add_child(body);
    bodies_[i] = body;
    godot::Ref<godot::CapsuleShape3D> shape;
    shape.instantiate();
    shape->set_radius(0.3);
    shape->set_height(1.8);
    auto* collision = memnew(godot::CollisionShape3D);
    collision->set_shape(shape);
    collision->set_position({0, 0.9, 0});
    body->add_child(collision);

    godot::Ref<godot::CapsuleMesh> mesh;
    mesh.instantiate();
    mesh->set_radius(0.3);
    mesh->set_height(1.8);
    godot::Ref<godot::StandardMaterial3D> material;
    material.instantiate();
    material->set_albedo(i == 0 ? godot::Color(0, 0.65, 0.7) :
                                  godot::Color(1, 0.4, 0.05));
    auto* visual = memnew(godot::MeshInstance3D);
    visual->set_mesh(mesh);
    visual->set_material_override(material);
    visual->set_position({0, 0.9, 0});
    body->add_child(visual);
    body->hide();
  }
  session_.setCollisionResolver([this](std::uint32_t id, Vector3 position) {
    return resolve_collision(id, position);
  });
  session_.setFireHandler([this](std::uint32_t id) {
    emit_signal("fire_requested", static_cast<int>(id));
  });
  set_physics_process(true);
}

void FireteamSession::report_error(const std::exception& error) {
  godot::UtilityFunctions::push_error(godot::String(error.what()));
  emit_signal("session_error", godot::String(error.what()));
}

bool FireteamSession::host_game(int port) {
  try {
    session_.host(validate_port(port, true));
    synchronize_bodies();
    return true;
  } catch (const std::exception& error) {
    report_error(error);
    return false;
  }
}

bool FireteamSession::join_game(const godot::String& address, int port) {
  try {
    session_.join(address.utf8().get_data(), validate_port(port, false));
    return true;
  } catch (const std::exception& error) {
    report_error(error);
    return false;
  }
}

void FireteamSession::stop_game() {
  session_.stop();
  synchronize_bodies();
}

bool FireteamSession::set_input(double moveX, double moveZ, double yaw,
                                double pitch, bool fire) {
  try {
    session_.submitInput({static_cast<float>(moveX), static_cast<float>(moveZ),
                          static_cast<float>(yaw), static_cast<float>(pitch), fire});
    return true;
  } catch (const std::exception& error) {
    report_error(error);
    return false;
  }
}

bool FireteamSession::damage_player(int playerId, int amount) {
  try {
    if (playerId < 1 || playerId > 2) {
      throw std::invalid_argument("Player ID must be 1 or 2");
    }
    session_.applyDamage(static_cast<std::uint32_t>(playerId), amount);
    return true;
  } catch (const std::exception& error) {
    report_error(error);
    return false;
  }
}

bool FireteamSession::restart_match() {
  try {
    session_.rematch();
    synchronize_bodies();
    return true;
  } catch (const std::exception& error) {
    report_error(error);
    return false;
  }
}

godot::Dictionary FireteamSession::get_state() const {
  const auto snapshot = session_.snapshot();
  godot::Dictionary result;
  result["local_player_id"] = static_cast<int>(session_.localPlayerId());
  result["port"] = static_cast<int>(session_.port());
  result["host"] = session_.isHost();
  result["active"] = session_.active();
  result["connected"] = session_.connected();
  result["phase"] = static_cast<int>(snapshot.phase);
  result["winner_id"] = static_cast<int>(session_.winnerId());
  godot::Array players;
  for (const auto& player : snapshot.players) {
    godot::Dictionary entry;
    entry["id"] = static_cast<int>(player.playerId);
    entry["position"] = to_godot(player.position);
    entry["yaw"] = player.yaw;
    entry["pitch"] = player.pitch;
    entry["health"] = player.health;
    entry["max_health"] = player.maxHealth;
    players.push_back(entry);
  }
  result["players"] = players;
  return result;
}

Vector3 FireteamSession::resolve_collision(std::uint32_t playerId,
                                           Vector3 desired) {
  auto* body = bodies_.at(playerId - 1);
  // A join/rematch may reset core spawns before the visual synchronization.
  body->set_position(to_godot(session_.snapshot().players.at(playerId - 1).position));
  const auto collision =
      body->move_and_collide(to_godot(desired) - body->get_position());
  if (collision.is_valid()) {
    body->move_and_collide(collision->get_remainder().slide(collision->get_normal()));
  }
  return from_godot(body->get_position());
}

void FireteamSession::synchronize_bodies() {
  const auto snapshot = session_.snapshot();
  for (std::size_t i = 0; i < bodies_.size(); ++i) {
    auto* body = bodies_[i];
    if (!body) {
      continue;
    }
    const auto& player = snapshot.players[i];
    body->set_visible(player.playerId != 0 && player.health > 0 &&
                       player.playerId != session_.localPlayerId());
    body->set_collision_layer(player.playerId != 0 && session_.isHost() ? 2 : 0);
    body->set_collision_mask(session_.isHost() ? 1 : 0);
    body->set_position(to_godot(player.position));
    body->set_rotation({0, -player.yaw, 0});
  }
}

void FireteamSession::_physics_process(double delta) {
  if (!session_.active()) {
    return;
  }
  try {
    // Bound frame stalls; packet senders never supply simulation time.
    session_.step(static_cast<float>(std::min(delta, 0.1)));
    synchronize_bodies();
  } catch (const std::exception& error) {
    // Peer rejection/timeouts already return a healthy host to Lobby.
    if (!session_.isHost() || session_.snapshot().phase != MatchPhase::Lobby) {
      session_.stop();
    }
    synchronize_bodies();
    report_error(error);
  }
}

}  // namespace fireteam
