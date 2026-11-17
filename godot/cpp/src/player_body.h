// PlayerBody: the Godot adapter for issue #6 (Koushik).
//
// This is a THIN bridge, not gameplay logic. The gameplay model lives in the
// engine-agnostic fireteam::Player (src/Player.cpp, unit-tested). PlayerBody:
//   * is a CharacterBody3D so Godot gives us 3D collision + move_and_slide,
//   * owns one fireteam::Player,
//   * each physics tick: feeds input intent in, runs the model, applies the
//     model's planar velocity + 3D gravity/jump, slides, writes the resolved
//     X/Z position back into the model,
//   * exposes read-only getters the HUD (#7) formats,
//   * exposes a damage sink (apply_damage) and an authoritative respawn() that
//     rebuilds the model at the team spawn (the host/server authority calls it).
//
// Hit DETECTION + damage VALUES (#8/#9) and network replication / respawn
// AUTHORIZATION (#10/#11) are NOT here; this only provides the seams (the ammo +
// snapshot model, the damage sink, and respawn as an action the authority triggers).
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
    int team_ = 0;            // remembered so respawn() rebuilds the same team
    Vector3 spawn_{};         // remembered team spawn point (set in setup)
    Vector2 intent_{};        // world-plane (x, z) movement intent from the controller
    bool want_jump_ = false;
    double vertical_ = 0.0;   // persisted Y velocity for gravity/jump (metres/sec)

    float gravity_ = 18.0f;   // m/s^2 (snappier than Earth; typical for shooters)
    float jump_speed_ = 6.0f; // m/s initial upward velocity

    void build_model_();      // (re)create the model at the stored team + spawn

protected:
    static void _bind_methods();

public:
    PlayerBody();
    ~PlayerBody();

    // Create the model for a team (0 = teal, 1 = orange) and place the body.
    void setup(int team, Vector3 spawn);

    // Authoritative respawn: rebuild a fresh model (full health + ammo, alive) at
    // the stored team spawn and reset motion. The ONLY caller is the host/server
    // respawn authority (see respawn_authority.gd) -- never client input, so a
    // client cannot revive itself. Emits "respawned" for the rig to reset camera +
    // animation. Rebuilding keeps health restoration out of any teammate file;
    // note model() returns a NEW pointer afterwards.
    void respawn();

    // ---- per-frame input (from the GDScript controller / network proxy) ----
    void set_move_intent(Vector2 planar_dir);   // (x, z) in world space
    void set_running(bool running);
    void set_yaw(float radians);
    void request_jump();
    bool request_fire();     // true if a round left the muzzle (no hit calc here)
    void request_reload();

    // Damage sink for combat (#8/#9): a node-level entry point that forwards to
    // the model's applyDamage(). How Khushal's hit detection delivers damage is
    // his design -- either this bound method on the resolved target node, or the
    // core path Weapon::fire(Entity&) against model() below. Hit DETECTION and
    // damage VALUES stay in #8/#9; the HUD harness uses this to exercise the bar.
    void apply_damage(int amount);

    // Unbound (C++-only) access to the engine-agnostic model, so combat's
    // Weapon::fire(fireteam::Entity&) can target the core Player directly.
    fireteam::Player *model() const { return model_.get(); }

    void _physics_process(double delta) override;

    // ---- read-only state for the HUD (#7) ----
    int get_health() const;
    int get_max_health() const;
    bool is_ready_to_respawn() const;  // dead + countdown elapsed, nothing respawned it yet
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
