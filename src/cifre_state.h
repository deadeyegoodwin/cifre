// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <cstdint>
#include <unordered_set>
#include <vector>

#include "common_logger.h"

namespace cifre {

//
// State
//
// Interface for game state. State represents all mutable status,
// properties, etc. of a game. State is assumed to be non-thread-safe.
//
class State {
public:
  //
  // Status
  //
  // Status of game state.
  //
  class Status {
  public:
    // Is state a terminal state, that is, does it represent a game
    // ending state.
    virtual bool IsTerminal() const = 0;
  };

public:
  // Return the total number of action IDs available across all
  // variations of this state. Each action ID is a numeric identifier
  // of type uint64_t in the range [0, total-action-id-count).
  virtual size_t TotalActionIdCount() const = 0;

  // Get the status of the state.
  virtual const State::Status& StateStatus() const = 0;

  // Get the action identifiers that are valid for this state.
  virtual const std::unordered_set<uint64_t>& ValidActionIds() const = 0;

  // Apply the action associated with 'action_id' to this state, so
  // that the state is modified due to the action.
  virtual void Update(uint64_t action_id) = 0;

  // Get the representation of the state suitable for feeding to the
  // model via triton. This representation does not need to include
  // all of the state, but must include whatever is required by the
  // model being used.
  virtual const std::vector<uint8_t>& TritonRepresentation() const = 0;

  // Return string representation of the state.
  virtual std::string ToString(
    common::Logger::Level filter_level = common::Logger::Level::LOW) const = 0;
};

} // namespace cifre
