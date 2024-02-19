// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <cstdint>
#include <vector>

namespace cifre {

//
// State
//
// Interface for game state.
//
class State {
public:
  // Return a deep-copy of this state.
  virtual State* Clone() const = 0;

  // Get the representation of the state suitable for feeding to the
  // model via triton.
  virtual const std::vector<uint8_t>& TritonRepresentation() const = 0;
};

} // namespace cifre
