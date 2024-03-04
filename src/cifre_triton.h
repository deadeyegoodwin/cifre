// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <memory>
#include <string>

#include "common_logger.h"
#include "http_client.h"
#include "json_utils.h"

namespace cifre {

//
// TritonModelMetadata
//
struct TritonModelMetadata {
  std::string input_name_;
  std::string output_pi_name_;
  std::string output_v_name_;

  // Shapes include the batch dimension (set to 1 as currently only
  // batch-size-1 requests are made).
  std::vector<int64_t> input_shape_;
  std::vector<int64_t> output_pi_shape_;
  std::vector<int64_t> output_v_shape_;

  size_t required_input_byte_size_;
  size_t required_output_pi_byte_size_;
  size_t required_output_v_byte_size_;
};

//
// TritonInferResult
//
// The output tensors returned by an inference.
//
class TritonInferResult {
public:
  TritonInferResult(const TritonModelMetadata& metadata, triton::client::InferResult* r);
  ~TritonInferResult();

  // Return the size of 'pi', in bytes.
  size_t PiByteSize() const { return pi_byte_size_; }

  // Return the size of 'v', in bytes.
  size_t VByteSize() const { return v_byte_size_; }

  // Return 'pi'.
  const float* Pi() const { return pi_; }

  // Return 'v'.
  const float* V() const { return v_; }

private:
  void ValidateResult(const std::string& name, const std::vector<int64_t>& shape) const;

  triton::client::InferResult* result_;
  size_t pi_byte_size_;
  float* pi_;
  size_t v_byte_size_;
  float* v_;
};

//
// Triton
//
// Interface to Triton inference server.
//
class Triton {
public:
  Triton(common::Logger* logger);
  ~Triton();

  // Return true if triton is ready to receive inference requests.
  bool IsReady() const;

  // Return true if triton is ready to receive inference requests for 'model'.
  bool IsModelReady(const std::string& model) const;

  // Load 'model' onto triton. Return true if model load is successful.
  bool LoadModel(const std::string& model);

  // Unload 'model' from triton. Return true if model unload is successful.
  bool UnloadModel(const std::string& model);

  // Run 'model'.
  TritonInferResult RunModel(
    const std::string& model, const std::vector<uint8_t>& state_triton_repr) const;

private:
  triton::client::Error UpdateModelMetadata(const std::string& model);
  triton::client::Error UpdateInputMetadata(
    TritonModelMetadata* metadata, const rapidjson::Document& model_metadata);
  triton::client::Error UpdateOutputMetadata(
    TritonModelMetadata* metadata, const rapidjson::Document& model_metadata, size_t oidx);

  void LogTritonError(
    const triton::client::Error& err, const std::string& msg = std::string()) const;

  common::Logger* logger_;
  std::unique_ptr<triton::client::InferenceServerHttpClient> client_;
  std::unordered_map<std::string, TritonModelMetadata> model_metadata_;
};

} // namespace cifre
