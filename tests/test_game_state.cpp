#include <doctest/doctest.h>

#include <stdexcept>

#include "fireteam/GameState.hpp"

using fireteam::GameState;
using fireteam::MatchPhase;

TEST_CASE("GameState starts in Lobby and permits a full match cycle") {
  GameState state;
  CHECK(state.phase() == MatchPhase::Lobby);
  state.transitionTo(MatchPhase::InGame);
  CHECK(state.phase() == MatchPhase::InGame);
  state.transitionTo(MatchPhase::MatchOver);
  CHECK(state.phase() == MatchPhase::MatchOver);
  state.transitionTo(MatchPhase::Lobby);
  CHECK(state.phase() == MatchPhase::Lobby);
}

TEST_CASE("GameState rejects every illegal transition without changing phase") {
  const MatchPhase phases[] = {
      MatchPhase::Lobby, MatchPhase::InGame, MatchPhase::MatchOver};
  for (int from = 0; from < 3; ++from) {
    for (int to = 0; to < 3; ++to) {
      if (to == (from + 1) % 3) {
        continue;
      }
      GameState state;
      for (int step = 1; step <= from; ++step) {
        state.transitionTo(phases[step]);
      }
      CHECK_THROWS_AS(state.transitionTo(phases[to]), std::logic_error);
      CHECK(state.phase() == phases[from]);
    }
  }
}
