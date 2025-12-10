// Copyright 2025, David Goodwin. All rights reserved.
#pragma once

#include "cifre_algo.h"
#include "cifre_state.h"

namespace cifre {

//
// MiniMaxAlgo
//
// Implementation of the minimax algorithm with pruning by taking
// advantage that only game outcomes are WINNER1, WINNER2, or DRAW.
//
class MiniMaxAlgo : public Algo {
public:
  MiniMaxAlgo(const std::shared_ptr<common::Logger>& logger);

  // See cifre::Algo.
  void GameStart() override;

  // See cifre::Algo.
  void GameEnd(const State& terminal_state, const StateTerminalStatus& status) override;

  // See cifre::Algo.
  uint64_t NextActionId(const State& state, size_t max_sidegame_turns_remaining) override;

private:
  // Returns <action-id, value> of the best action to take from
  // 'state'.
  std::pair<uint64_t, float> Search(
    const State& state, const size_t game_depth_remaining) const;

  std::shared_ptr<common::Logger> logger_;
};

} // namespace cifre
