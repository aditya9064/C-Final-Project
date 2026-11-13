#pragma once

namespace fireteam {

enum class MatchPhase { Lobby, InGame, MatchOver };

class GameState {
 public:
  MatchPhase phase() const { return phase_; }
  void transitionTo(MatchPhase next);

 private:
  MatchPhase phase_ = MatchPhase::Lobby;
};

}  // namespace fireteam
