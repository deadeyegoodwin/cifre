// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <string>
#include <unordered_set>

#include "cifre_triton.h"
#include "common_logger.h"

namespace cifre {

//
// AlphaZero
//
// Implementation of the alpha-zero algorithm.
//
class AlphaZero {
public:
  // Create alphazero object using 'model-name' for predictions.
  AlphaZero(common::Logger* logger, const std::string& model_name);

  // Return the next predicted action ID, given current state in
  // triton representation.
  uint64_t NextActionId(
    const std::vector<uint8_t>& state_triton_repr,
    const std::unordered_set<uint64_t>& valid_action_ids) const;

private:
  common::Logger* logger_;
  const std::string model_name_;
  Triton triton_;
};

} // namespace cifre
