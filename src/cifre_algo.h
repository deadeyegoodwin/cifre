// Copyright 2025, David Goodwin. All rights reserved.
#pragma once

#include <atomic>
#include <cmath>
#include <memory>
#include <optional>
#include <string>

#include "cifre_state.h"

namespace cifre {

//
// Algo
//
// Base class for game-playing algorithms.
//
// Memory Management: An algorithm may implement one or more monotonic
// pmr memory resources to allocate game state. It is the state's
// responsibility to use the provided memory resource for all
// allocations. Note that if an algorithm provides a memory resource
// that it must be monotonic, which means that the state does not need
// to concern itself with deallocation; it is the responsibility of
// the the algorithm to free memory as appropriate. See also
// cifre::State.
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
  // but the recommended behavior is for the function to throw an
  // error. 'max_game_sideturns_remaining' indicates the maximum
  // number of game side-turns remaining. A "side-turn" is a player
  // taking a turn. A game with a maximum lenth of N turns, with P
  // players will have N*P side-turns.
  virtual uint64_t NextActionId(const State& state, size_t max_game_sideturns_remaining) = 0;
};

} // namespace cifre
