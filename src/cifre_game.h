// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <cstdint>
#include <vector>

namespace cifre {

//
// Game
//
// Interface to a game.
//
class Game {
public:
  // Return the number of actions available in the game. Each action
  // is represented by a numeric identifier of type uint64_t in the
  // range [0, total-action-count).
  virtual uint64_t TotalActionCount() const = 0;

  // Get the action identifiers available for a given state.
  virtual const std::vector<uint64_t>& StateActions(const State& state) const = 0;

  // Apply an action to a state, in place, so that the state is
  // modified due to the action.
  virtual void ApplyAction(State* state) const = 0;
};


} // namespace cifre
