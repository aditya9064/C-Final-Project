#include "fireteam/GameState.hpp"

#include <stdexcept>

namespace fireteam {

void GameState::transitionTo(MatchPhase next) {
  const bool allowed =
      (phase_ == MatchPhase::Lobby && next == MatchPhase::InGame) ||
      (phase_ == MatchPhase::InGame && next == MatchPhase::MatchOver) ||
      (phase_ == MatchPhase::MatchOver && next == MatchPhase::Lobby);

  if (!allowed) {
    throw std::logic_error("Illegal match phase transition");
  }
  phase_ = next;
}

}  // namespace fireteam
