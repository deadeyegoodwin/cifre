// Copyright 2024, David Goodwin. All rights reserved.

#include "cifre_alphazero.h"
#include "common_error.h"

namespace cifre {

AlphaZero::AlphaZero(common::Logger* logger, const std::string& model_name) :
  logger_(logger), triton_(logger)
{
  THROWIF(model_name.empty(), "model must be specified for alpha-zero");

  // Make sure triton is ready.
  THROWIF(!triton_.IsReady(), "triton is not ready");

  // Load model and make sure it is ready.
  THROWIF(!triton_.LoadModel(model_name),
          std::string("triton model '" + model_name + "' failed to load"));
  THROWIF(!triton_.IsModelReady(model_name),
          std::string("triton model '" + model_name + "' is not ready"));
}

} // namespace cifre
