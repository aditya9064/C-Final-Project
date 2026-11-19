#pragma once

#include <array>

#include <godot_cpp/classes/character_body3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/dictionary.hpp>

#include "fireteam/MatchSession.hpp"

namespace fireteam {

class FireteamSession : public godot::Node3D {
  GDCLASS(FireteamSession, godot::Node3D)

 public:
  void _ready() override;
  void _physics_process(double delta) override;
  bool host_game(int port);
  bool join_game(const godot::String& address, int port);
  void stop_game();
  bool set_input(double moveX, double moveZ, double yaw, double pitch, bool fire,
                 bool reload = false);
  bool damage_player(int playerId, int amount);
  bool restart_match();
  godot::Dictionary get_state() const;

 protected:
  static void _bind_methods();

 private:
  MatchSession session_;
  std::array<godot::CharacterBody3D*, 2> bodies_{};
  Vector3 resolve_collision(std::uint32_t playerId, Vector3 desired);
  void synchronize_bodies();
  void report_error(const std::exception& error);
};

}  // namespace fireteam
