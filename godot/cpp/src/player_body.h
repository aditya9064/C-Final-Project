// PlayerBody: the Godot adapter for issue #6 (Koushik).
//
// This is a THIN bridge, not gameplay logic. The gameplay model lives in the
// engine-agnostic fireteam::Player (src/Player.cpp, unit-tested). PlayerBody:
//   * is a CharacterBody3D so Godot gives us 3D collision + move_and_slide,
//   * owns one fireteam::Player,
//   * each physics tick: feeds input intent in, runs the model, applies the
//     model's planar velocity + 3D gravity/jump, slides, writes the resolved
//     X/Z position back into the model,
//   * exposes read-only getters the HUD (#7) formats.
//
// Combat damage (#8/#9) and network replication (#10/#11) are NOT here; the
// model already carries the ammo + snapshot seams those systems plug into.
#pragma once

#include <godot_cpp/classes/character_body3d.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <memory>

namespace fireteam { class Player; }

namespace godot {

class PlayerBody : public CharacterBody3D {
    GDCLASS(PlayerBody, CharacterBody3D)

    std::unique_ptr<fireteam::Player> model_;
    Vector2 intent_{};        // world-plane (x, z) movement intent from the controller
    bool want_jump_ = false;
    double vertical_ = 0.0;   // persisted Y velocity for gravity/jump (metres/sec)

    float gravity_ = 18.0f;   // m/s^2 (snappier than Earth; typical for shooters)
    float jump_speed_ = 6.0f; // m/s initial upward velocity

protected:
    static void _bind_methods();

public:
    PlayerBody();
    ~PlayerBody();

    // Create the model for a team (0 = teal, 1 = orange) and place the body.
    void setup(int team, Vector3 spawn);

    // ---- per-frame input (from the GDScript controller / network proxy) ----
    void set_move_intent(Vector2 planar_dir);   // (x, z) in world space
    void set_running(bool running);
    void set_yaw(float radians);
    void request_jump();
    bool request_fire();     // true if a round left the muzzle (no hit calc here)
    void request_reload();

    void _physics_process(double delta) override;

    // ---- read-only state for the HUD (#7) ----
    int get_health() const;
    int get_ammo() const;
    int get_magazine_size() const;
    int get_team() const;
    float get_walk_speed() const;   // single source for the rig's anim thresholds
    float get_run_speed() const;
    bool is_reloading() const;
    float get_reload_progress() const;
    float get_respawn_remaining() const;
    bool is_awaiting_respawn() const;
    Vector3 get_planar_velocity() const;
};

}  // namespace godot
