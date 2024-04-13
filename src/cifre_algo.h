// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <atomic>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "cifre_state.h"

namespace cifre {

//
// Algo
//
// Base class for game-playing algorithms.
//
class Algo {
public:
  virtual ~Algo() = default;

  // Called to notify that a new game is starting.
  virtual void GameStart() = 0;

  // Called to notify that game has ended at 'terminal_state' with a
  // terminal 'status'. Note the 'status' may differ from the actual
  // terminal status of 'terminal_state', but any end-game actions
  // should respect 'status'. 'status' of NON_TERMINAL is not allowed
  // and GameEnd should throw exception if that value is received.
  virtual void GameEnd(const State& terminal_state, const StateTerminalStatus& status) = 0;

  // Return the next predicted action ID, given current 'state'. If
  // 'state' represents a terminal state the behavior is undefined,
  // but the recommended behavior is for the function to throw and
  // error. 'max_game_turns_remaining' indicates the maximum number of
  // game turns remaining.
  virtual uint64_t NextActionId(const State& state, size_t max_game_turns_remaining) = 0;
};

} // namespace cifre
