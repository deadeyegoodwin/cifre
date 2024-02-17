// Copyright 2024, David Goodwin. All rights reserved.

#include <cstddef>
#include <cstdint>
#include <string>

#include "celcath_alphazero_player.h"
#include "cifre_triton.h"
#include "common_error.h"

namespace cifre { namespace celcath {

AlphaZeroPlayer::AlphaZeroPlayer(common::Logger* logger, const std::string& name, char board_char,
                                 const std::string& model_name) :
  Player(logger, name, board_char, 1.0 /* neighbor_value */)
{
  THROWIF(model_name.empty(), "--model must be specified");

  // Initialize triton.
  cifre::Triton triton(logger_);

  // Make sure triton is ready.
  THROWIF(!triton.IsReady(), "triton is not ready");

  // Load model and make sure it is ready.
  THROWIF(!triton.LoadModel(model_name),
          std::string("triton model '" + model_name + "' failed to load"));
  THROWIF(!triton.IsModelReady(model_name),
          std::string("triton model '" + model_name + "' is not ready"));
}

}} // namespace cifre::celcath
