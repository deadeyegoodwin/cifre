// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <memory>
#include <string>

#include "common_logger.h"
#include "http_client.h"

namespace cifre {

//
// Triton
//
// Interface to Triton inference server.
//
class Triton {
public:
  Triton(common::Logger* logger);

  // Return true if triton is ready to receive inference requests.
  bool IsReady();

  // Return true if triton is ready to receive inference requests for 'model'.
  bool IsModelReady(const std::string& model);

  // Load 'model' onto triton. Return true if model load is successful.
  bool LoadModel(const std::string& model);

private:
  void CheckTritonError(const triton::client::Error& err, const std::string& msg);
  void LogTritonError(const triton::client::Error& err, const std::string& msg);

  common::Logger* logger_;
  std::unique_ptr<triton::client::InferenceServerHttpClient> client_;
};

} // namespace cifre
