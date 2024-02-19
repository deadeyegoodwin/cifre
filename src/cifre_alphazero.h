// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <string>

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

private:
  common::Logger* logger_;
  Triton triton_;
};

} // namespace cifre
