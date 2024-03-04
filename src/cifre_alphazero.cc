// Copyright 2024, David Goodwin. All rights reserved.

#include <limits>

#include "cifre_alphazero.h"
#include "common_error.h"

namespace cifre {

AlphaZero::AlphaZero(
  common::Logger* logger, const std::shared_ptr<Triton>& triton, const std::string& model_name) :
  logger_(logger), triton_(triton), model_name_(model_name)
{
  THROWIF(model_name_.empty(), "model must be specified for alpha-zero");

  // Load model and make sure it is ready.
  THROWIF(!triton_->LoadModel(model_name_),
          std::string("triton model '" + model_name_ + "' failed to load"));
  THROWIF(!triton_->IsModelReady(model_name_),
          std::string("triton model '" + model_name_ + "' is not ready"));
}

uint64_t
AlphaZero::NextActionId(
  const std::vector<uint8_t>& state_triton_repr, const std::unordered_set<uint64_t>& valid_action_ids) const
{
  const auto& result = triton_->RunModel(model_name_, state_triton_repr);

  // Return the highest value pi that represents a valid action ID.
  const size_t pi_size = result.PiByteSize() / sizeof(float);
  const float *pi = result.Pi();

  float max_pi = -std::numeric_limits<float>::max();
  uint64_t action_id = std::numeric_limits<uint64_t>::max();
  for (uint64_t aid : valid_action_ids) {
    THROWIF(aid >= pi_size, "unexpected action ID indexing pi");
    if (pi[aid] > max_pi) {
      max_pi = pi[aid];
      action_id = aid;
    }
  }

  THROWIF(action_id == std::numeric_limits<uint64_t>::max(),
          "alpha-zero unable to find action ID");
  return action_id;
}

} // namespace cifre
