// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <memory>
#include <string>
#include <unordered_set>

#include "cifre_state.h"
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
  AlphaZero(const std::shared_ptr<common::Logger>& logger, const std::shared_ptr<cifre::Triton>& triton,
            const std::string& model_name);

  // Return the next predicted action ID, given current state.
  uint64_t NextActionId(const cifre::State& state) const;

private:
  std::shared_ptr<common::Logger> logger_;
  std::shared_ptr<Triton> triton_;
  const std::string model_name_;
};

} // namespace cifre
