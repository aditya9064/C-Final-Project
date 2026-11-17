#include "game_world_node.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

void GameWorldNode::_bind_methods() {
    ClassDB::bind_method(D_METHOD("start_match"), &GameWorldNode::start_match);
    ClassDB::bind_method(D_METHOD("end_match"), &GameWorldNode::end_match);
    ClassDB::bind_method(D_METHOD("get_phase"), &GameWorldNode::get_phase);
    ClassDB::bind_method(D_METHOD("get_match_time"), &GameWorldNode::get_match_time);
    ClassDB::bind_method(D_METHOD("get_score", "team"), &GameWorldNode::get_score);
    ClassDB::bind_method(D_METHOD("add_score", "team", "points"), &GameWorldNode::add_score);
}

void GameWorldNode::start_match() {
    try {
        state_.transitionTo(fireteam::MatchPhase::InGame);
        match_time_ = 0.0;
    } catch (const std::exception &e) {
        UtilityFunctions::push_error(String("start_match: ") + e.what());
    }
}

void GameWorldNode::end_match() {
    try {
        state_.transitionTo(fireteam::MatchPhase::MatchOver);
    } catch (const std::exception &e) {
        UtilityFunctions::push_error(String("end_match: ") + e.what());
    }
}

void GameWorldNode::_process(double delta) {
    if (state_.phase() == fireteam::MatchPhase::InGame && delta > 0.0) {
        match_time_ += delta;
    }
}

int GameWorldNode::get_phase() const { return (int)state_.phase(); }

int GameWorldNode::get_score(int team) const {
    return (team == 1) ? score_orange_ : score_teal_;
}

void GameWorldNode::add_score(int team, int points) {
    if (team == 1) score_orange_ += points; else score_teal_ += points;
}
