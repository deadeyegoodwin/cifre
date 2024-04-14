// Copyright 2024, David Goodwin. All rights reserved.

#include "cifre_algo_minimax.h"
#include "common_error.h"

namespace cifre {

constexpr float DRAW_VALUE = 0.0;
constexpr float WINNER_VALUE = 1.0;
constexpr float LOSER_VALUE = -1.0;

//
// MiniMaxAlgo
//
MiniMaxAlgo::MiniMaxAlgo(const std::shared_ptr<common::Logger>& logger) :
  logger_(logger)
{
}

void
MiniMaxAlgo::GameStart()
{
}

void
MiniMaxAlgo::GameEnd(const State& terminal_state, const StateTerminalStatus& status)
{
  THROWIF(status.Kind() == cifre::StateTerminalStatus::TerminalKind::NON_TERMINAL,
          "game ended with NON-TERMINAL status");
}

uint64_t
MiniMaxAlgo::NextActionId(const State& state, size_t max_game_turns_remaining)
{
  THROWIF(state.TerminalStatus().Kind() != StateTerminalStatus::TerminalKind::NON_TERMINAL,
          "minimax cannot find next action for terminal state");

  // FIXME here we assume the game has two players, and that a player
  // only gets to choose a single action each turn.
  //
  // Use the std::max to avoid issues with overflow as
  // std::numeric_limits<size_t>::max() may be the value of
  // 'max_game_turns_remaining'.
  const size_t max_game_depth =
    std::max(max_game_turns_remaining, max_game_turns_remaining * 2);

  LOGGER_LOGHIGH(*logger_, "minimax: starting state");
  LOGGER_LOGHIGH_IE(*logger_, state.ToString(), 2 /* indent */, true /* endline */);

  // Search from 'state' to find the best action.
  LOGGER_LOGHIGH_IE(*logger_, "search: ", 0 /* indent */, false /* endline */);
  auto pr = Search(state, max_game_depth);
  LOGGER_LOGHIGH_IE(*logger_, "\n", 0 /* indent */, false /* endline */);

  return pr.first;
}

std::pair<uint64_t, float>
MiniMaxAlgo::Search(const State& state, const size_t game_depth_remaining) const
{
  // If 'state' is terminal return its value.
  const auto& tstatus = state.TerminalStatus();
  if (tstatus.Kind() != StateTerminalStatus::TerminalKind::NON_TERMINAL) {
    const float tvalue =
      (tstatus.Kind() == StateTerminalStatus::TerminalKind::DRAW) ? DRAW_VALUE :
      ((tstatus.WinnerId() == state.ActivePlayerId()) ? WINNER_VALUE : LOSER_VALUE);
    LOGGER_LOGHIGH(*logger_, "active p" + std::to_string(state.ActivePlayerId()) +
                   ", terminal value " + std::to_string(tvalue));
    LOGGER_LOGHIGH_IE(*logger_, state.ToString(), 2 /* indent */, true /* endline */);
    return std::make_pair(std::numeric_limits<uint64_t>::max(), tvalue);
  }

  // If game depth has been reached then assume game is a draw.
  if (game_depth_remaining == 0) {
    LOGGER_LOGHIGH(*logger_, "active p" + std::to_string(state.ActivePlayerId()) +
                   ", max-game-depth value " + std::to_string(DRAW_VALUE));
    LOGGER_LOGHIGH_IE(*logger_, state.ToString(), 2 /* indent */, true /* endline */);
    return std::make_pair(std::numeric_limits<uint64_t>::max(), DRAW_VALUE);
  }

  // Search every valid action, return immediately if we find an
  // action that leads to win.
  uint64_t best_aid = std::numeric_limits<uint64_t>::max();
  float best_v = -std::numeric_limits<float>::max();

  for (uint64_t aid : state.ValidActionIds()) {
    std::unique_ptr<State> nstate(state.Clone());
    nstate->Update(aid);

    // If 'nstate' has a different active player than 'state', then
    // need to invert 'nstate's value so that it is respective to how
    // 'state' views value.
    auto pr = Search(*nstate, game_depth_remaining - 1);
    float v = pr.second;
    if (state.ActivePlayerId() != nstate->ActivePlayerId()) {
      v = -v;
    }

    if (v > best_v) {
      best_v = v;
      best_aid = aid;
      if (v == WINNER_VALUE) {
        break;
      }
    }
  }

  LOGGER_LOGHIGH(*logger_, "active p" + std::to_string(state.ActivePlayerId()) +
                 ", depth remaining " + std::to_string(game_depth_remaining));
  LOGGER_LOGHIGH(
    *logger_, "best " + state.ActionToString(Action(Action::ActionKind::ACTIONID, best_aid)) +
    ", value " + std::to_string(best_v));
  LOGGER_LOGHIGH_IE(*logger_, state.ToString(), 2 /* indent */, true /* endline */);

  return std::make_pair(best_aid, best_v);
}

} // namespace cifre
