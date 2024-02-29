// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <memory>
#include <string>

#include "common_logger.h"
#include "http_client.h"
#include "json_utils.h"

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

  // Run 'model'.
  void RunModel(const std::string& model);

private:
  struct ModelMetadata {
    std::string input_name_;
    std::string output_pi_name_;
    std::string output_v_name_;
    std::vector<size_t> input_shape_;
    std::vector<size_t> output_pi_shape_;
  };

  triton::client::Error UpdateModelMetadata(const std::string& model);
  triton::client::Error UpdateInputMetadata(
    ModelMetadata* metadata, const rapidjson::Document& model_metadata);
  triton::client::Error UpdateOutputMetadata(
    ModelMetadata* metadata, const rapidjson::Document& model_metadata, size_t oidx);

  void CheckTritonError(const triton::client::Error& err, const std::string& msg);
  void LogTritonError(const triton::client::Error& err, const std::string& msg);

  common::Logger* logger_;
  std::unique_ptr<triton::client::InferenceServerHttpClient> client_;
  std::unordered_map<std::string, Triton::ModelMetadata> model_metadata_;
};

} // namespace cifre
