#include "player_body.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "fireteam/Player.hpp"

using namespace godot;

PlayerBody::PlayerBody() = default;
PlayerBody::~PlayerBody() = default;

void PlayerBody::_bind_methods() {
    ClassDB::bind_method(D_METHOD("setup", "team", "spawn"), &PlayerBody::setup);
    ClassDB::bind_method(D_METHOD("set_move_intent", "planar_dir"), &PlayerBody::set_move_intent);
    ClassDB::bind_method(D_METHOD("set_running", "running"), &PlayerBody::set_running);
    ClassDB::bind_method(D_METHOD("set_yaw", "radians"), &PlayerBody::set_yaw);
    ClassDB::bind_method(D_METHOD("request_jump"), &PlayerBody::request_jump);
    ClassDB::bind_method(D_METHOD("request_fire"), &PlayerBody::request_fire);
    ClassDB::bind_method(D_METHOD("request_reload"), &PlayerBody::request_reload);
    ClassDB::bind_method(D_METHOD("respawn"), &PlayerBody::respawn);
    ClassDB::bind_method(D_METHOD("apply_damage", "amount"), &PlayerBody::apply_damage);
    ClassDB::bind_method(D_METHOD("get_health"), &PlayerBody::get_health);
    ClassDB::bind_method(D_METHOD("get_max_health"), &PlayerBody::get_max_health);
    ClassDB::bind_method(D_METHOD("is_ready_to_respawn"), &PlayerBody::is_ready_to_respawn);
    ClassDB::bind_method(D_METHOD("get_ammo"), &PlayerBody::get_ammo);
    ClassDB::bind_method(D_METHOD("get_magazine_size"), &PlayerBody::get_magazine_size);
    ClassDB::bind_method(D_METHOD("get_team"), &PlayerBody::get_team);
    ClassDB::bind_method(D_METHOD("get_walk_speed"), &PlayerBody::get_walk_speed);
    ClassDB::bind_method(D_METHOD("get_run_speed"), &PlayerBody::get_run_speed);
    ClassDB::bind_method(D_METHOD("is_reloading"), &PlayerBody::is_reloading);
    ClassDB::bind_method(D_METHOD("get_reload_progress"), &PlayerBody::get_reload_progress);
    ClassDB::bind_method(D_METHOD("get_respawn_remaining"), &PlayerBody::get_respawn_remaining);
    ClassDB::bind_method(D_METHOD("is_awaiting_respawn"), &PlayerBody::is_awaiting_respawn);
    ClassDB::bind_method(D_METHOD("get_planar_velocity"), &PlayerBody::get_planar_velocity);

    ADD_SIGNAL(MethodInfo("respawned"));
}

void PlayerBody::build_model_() {
    fireteam::Team t = (team_ == 1) ? fireteam::Team::Orange : fireteam::Team::Teal;
    model_ = std::make_unique<fireteam::Player>(
        t, fireteam::Vector2{(float)spawn_.x, (float)spawn_.z});
    set_global_position(spawn_);
    vertical_ = 0.0;
    intent_ = Vector2();
    want_jump_ = false;
}

void PlayerBody::setup(int team, Vector3 spawn) {
    team_ = team;
    spawn_ = spawn;
    build_model_();
}

void PlayerBody::respawn() {
    build_model_();            // fresh Player = full health + ammo, alive, at spawn
    emit_signal("respawned");  // rig resets camera + animation
}

void PlayerBody::set_move_intent(Vector2 planar_dir) { intent_ = planar_dir; }
void PlayerBody::set_running(bool running) { if (model_) model_->setRunning(running); }
void PlayerBody::set_yaw(float radians) { if (model_) model_->setYaw(radians); }
void PlayerBody::request_jump() { want_jump_ = true; }

bool PlayerBody::request_fire() { return model_ ? model_->fire() : false; }
void PlayerBody::request_reload() { if (model_) model_->reload(); }
void PlayerBody::apply_damage(int amount) { if (model_) model_->applyDamage(amount); }

void PlayerBody::_physics_process(double delta) {
    // Engine-called virtual: catch everything so a C++ exception never unwinds
    // into godot-cpp (built -fno-exceptions). This is the boundary rule.
    try {
        if (!model_ || delta <= 0.0) return;

        // 1) feed intent into the engine-agnostic model and advance it.
        model_->setMoveIntent(fireteam::Vector2{(float)intent_.x, (float)intent_.y});
        model_->update((float)delta);

        // 2) planar velocity comes from the model; Y is gravity/jump we own here.
        const fireteam::Vector2 pv = model_->velocity();
        if (is_on_floor()) {
            vertical_ = 0.0;
            if (want_jump_ && !model_->awaitingRespawn() && model_->isAlive()) {
                vertical_ = jump_speed_;
            }
        } else {
            vertical_ -= gravity_ * delta;
        }
        want_jump_ = false;

        set_velocity(Vector3(pv.x, (float)vertical_, pv.y));  // model.y is world Z
        move_and_slide();

        // 3) write the collision-resolved X/Z back so the model stays authoritative.
        const Vector3 p = get_global_position();
        model_->setPosition(fireteam::Vector2{(float)p.x, (float)p.z});
    } catch (const std::exception &e) {
        UtilityFunctions::push_error(String("PlayerBody tick error: ") + e.what());
    } catch (...) {
        UtilityFunctions::push_error("PlayerBody tick error: unknown");
    }
}

int PlayerBody::get_health() const { return model_ ? model_->health() : 0; }
int PlayerBody::get_max_health() const { return fireteam::Player::kMaxHealth; }
bool PlayerBody::is_ready_to_respawn() const { return model_ ? model_->readyToRespawn() : false; }
int PlayerBody::get_ammo() const { return model_ ? model_->ammo() : 0; }
int PlayerBody::get_magazine_size() const { return model_ ? model_->magazineSize() : 0; }
int PlayerBody::get_team() const { return model_ ? (int)model_->team() : 0; }
float PlayerBody::get_walk_speed() const { return fireteam::Player::kWalkSpeed; }
float PlayerBody::get_run_speed() const { return fireteam::Player::kRunSpeed; }
bool PlayerBody::is_reloading() const { return model_ ? model_->isReloading() : false; }
float PlayerBody::get_reload_progress() const { return model_ ? model_->reloadProgress() : 1.0f; }
float PlayerBody::get_respawn_remaining() const { return model_ ? model_->respawnRemaining() : 0.0f; }
bool PlayerBody::is_awaiting_respawn() const { return model_ ? model_->awaitingRespawn() : false; }

Vector3 PlayerBody::get_planar_velocity() const {
    if (!model_) return Vector3();
    const fireteam::Vector2 v = model_->velocity();
    return Vector3(v.x, 0.0f, v.y);
}
