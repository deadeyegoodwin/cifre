// Copyright 2026, David Goodwin. All rights reserved.

#include "cifre_algo_random.h"
#include "common_error.h"

namespace cifre {

//
// RandomAlgo
//
void
RandomAlgo::GameStart()
{
}

void
RandomAlgo::GameEnd(const State& terminal_state, const StateTerminalStatus& status)
{
  THROWIF(status.Kind() == cifre::StateTerminalStatus::TerminalKind::NON_TERMINAL,
          "game ended with NON-TERMINAL status");
}

uint64_t
RandomAlgo::NextActionId(const State& state, size_t max_game_sideturns_remaining)
{
  // Pick a random action from the valid actions. At least one action
  // must be valid.
  const auto& action_ids = state.ValidActionIds();
  THROWIF(action_ids.empty(), "no valid action to choose from");

  return
    *std::next(action_ids.begin(), rnd_(uint64_t(0), uint64_t(action_ids.size() - 1)));
}

} // namespace cifre
