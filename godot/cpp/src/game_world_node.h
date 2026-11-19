// GameWorldNode: the match-flow adapter (part of issues #6/#7, Koushik).
//
// Owns the engine-agnostic fireteam::GameState (Lobby/InGame/MatchOver) plus the
// per-match numbers the HUD (#7) shows -- elapsed time and the two team scores --
// so match logic stays in C++ and the HUD only formats it. The win condition
// itself (#11) is Aditya's; add_score() is the seam combat/netcode calls.
#pragma once

#include <godot_cpp/classes/node3d.hpp>

#include "fireteam/GameState.hpp"

namespace godot {

class GameWorldNode : public Node3D {
    GDCLASS(GameWorldNode, Node3D)

    fireteam::GameState state_;
    double match_time_ = 0.0;   // seconds elapsed while InGame
    int score_teal_ = 0;
    int score_orange_ = 0;

protected:
    static void _bind_methods();

public:
    void start_match();   // Lobby -> InGame
    void end_match();      // InGame -> MatchOver

    void _process(double delta) override;

    int get_phase() const;            // 0 Lobby, 1 InGame, 2 MatchOver
    double get_match_time() const { return match_time_; }
    int get_score(int team) const;    // 0 teal, 1 orange
    void add_score(int team, int points);  // seam for combat/netcode (#8-#11)
};

}  // namespace godot
