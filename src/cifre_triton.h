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
  TritonInferResult(const TritonModelMetadata& metadata, const triton::client::InferResult* result);

  // Move/copy constructors
  TritonInferResult(TritonInferResult& other) = delete;
  TritonInferResult(const TritonInferResult& other) = delete;
  TritonInferResult(const TritonInferResult&& other) = delete;
  TritonInferResult(TritonInferResult&& other) = default;

  // Return the shape input and output tensors.
  const std::vector<int64_t>& InputShape() const { return input_shape_; }
  const std::vector<int64_t>& PiShape() const { return pi_shape_; }
  const std::vector<int64_t>& VShape() const { return v_shape_; }

  // Return the byte-size of input and output tensors.
  size_t InputByteSize() const { return input_byte_size_; }
  size_t PiByteSize() const { return pi_byte_size_; }
  size_t VByteSize() const { return v_byte_size_; }

  // Return the number of entries in each input and output tensor.
  size_t InputEntryCount() const { return input_shape_[0] * input_shape_[1] * input_shape_[2]; }
  size_t PiEntryCount() const { return pi_shape_[0] * pi_shape_[1]; }
  size_t VEntryCount() const { return v_shape_[0] * v_shape_[1]; }

  // Return output tensor data.
  const std::vector<float>& Pi() const { return pi_; }
  float V() const { return v_; }

  // Return mutable versions of output tensor data.
  std::vector<float>& MutablePi() { return pi_; }
  float& MutableV() { return v_; }

private:
  void ValidateResult(
    const triton::client::InferResult* result, const std::string& name,
    const std::vector<int64_t>& shape) const;

  std::vector<int64_t> input_shape_;
  std::vector<int64_t> pi_shape_;
  std::vector<int64_t> v_shape_;

  size_t input_byte_size_;
  size_t pi_byte_size_;
  size_t v_byte_size_;

  // PI from the model is LogSoftmax so we store in pi_ exp(PI).
  std::vector<float> pi_;
  float v_;
};

//
// Triton
//
// Interface to Triton inference server.
//
class Triton {
public:
  Triton(const std::shared_ptr<common::Logger>& logger);
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

  std::shared_ptr<common::Logger> logger_;
  std::unique_ptr<triton::client::InferenceServerHttpClient> client_;
  std::unordered_map<std::string, TritonModelMetadata> model_metadata_;
};

} // namespace cifre
