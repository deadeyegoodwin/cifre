// Copyright 2026, David Goodwin. All rights reserved.
#pragma once

#include "cifre_algo.h"
#include "common_random.h"

namespace cifre {

//
// RandomAlgo
//
// Implementation of the random action sepection algorithm.
//
class RandomAlgo : public Algo {
public:
  // See cifre::Algo.
  void GameStart() override;

  // See cifre::Algo.
  void GameEnd(const State& terminal_state, const StateTerminalStatus& status) override;

  // See cifre::Algo.
  uint64_t NextActionId(const State& state, size_t max_game_sideturns_remaining) override;

private:
  common::Random rnd_;
};

} // namespace cifre
