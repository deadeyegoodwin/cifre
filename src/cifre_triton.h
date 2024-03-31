// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "cifre_state.h"
#include "common_logger.h"
#include "http_client.h"
#include "json_utils.h"

namespace cifre {

class TritonModelData;

//
// TritonInferResult
//
// The output tensors returned by an inference. A TritonInferResult
// can be cached to avoid mulitple identical inference requests being
// sent to triton server. To avoid the cached object being modified,
// all methods are const.
//
class TritonInferResult {
public:
  TritonInferResult(const TritonModelData& model_data, const triton::client::InferResult* result);

  // Don't allow move or copy (to avoid accidental performance
  // overhead).
  TritonInferResult(TritonInferResult& other) = delete;
  TritonInferResult(const TritonInferResult& other) = delete;
  TritonInferResult(const TritonInferResult&& other) = delete;
  TritonInferResult(TritonInferResult&& other) = delete;

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
// TritonModelData
//
// Metadata collected from triton about a model, and also any cached
// results for the model.
//
struct TritonModelData {
  // Input and output tensor names for the model.
  std::string input_name_;
  std::string output_pi_name_;
  std::string output_v_name_;

  // Shapes include the batch dimension (set to 1 as currently only
  // batch-size-1 requests are made).
  std::vector<int64_t> input_shape_;
  std::vector<int64_t> output_pi_shape_;
  std::vector<int64_t> output_v_shape_;

  // Size of input and output tensors, in bytes.
  size_t required_input_byte_size_;
  size_t required_output_pi_byte_size_;
  size_t required_output_v_byte_size_;

  // Cache results to avoid running inference multiple times on the
  // same State. The "State*" key is a pointer to the same state that
  // is owned by the unique_ptr<State> in the CacheEntry itself.
  struct CacheEntry {
    CacheEntry(std::unique_ptr<const State> state,
               std::shared_ptr<const TritonInferResult> result) :
      state_(std::move(state)), result_(result) { }
    const std::unique_ptr<const State> state_;
    const std::shared_ptr<const TritonInferResult> result_;
  };
  struct CacheMapHash {
    size_t operator()(const State* s) const { return s->Hash(); }
  };
  struct CacheMapEqual {
    bool operator()(const State* a, const State* b) const { return a->Equal(b); }
  };
  std::unordered_map<const State*, CacheEntry, CacheMapHash, CacheMapEqual> cache_;
  bool cache_enabled_;
};

//
// TritonStatistics
//
class TritonStatistics {
public:
  // Return string representation of statistics.
  std::string ToString() const;

  size_t prediction_count_ {0};
  size_t inference_count_ {0};
  size_t cache_hit_count_ {0};
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

  // Load 'model' onto triton. If the model is already loaded into
  // triton, it will remain loaded but metadata for the model will be
  // re-initialized from the server, and any cached results for the
  // model will be discarded. To avoid these side-effects, use
  // IsModelReady to determine if the model is already loaded. Return
  // true if model load is successful.
  bool LoadModel(const std::string& model, bool enable_cache);

  // Unload 'model' from triton. Return true if model unload is successful.
  bool UnloadModel(const std::string& model);

  // Run 'model' to get a prediction result. If caching is enable the
  // result can be returned from the cache instead of needing to be
  // generated from triton server.
  std::shared_ptr<const TritonInferResult> RunModel(
    const std::string& model, const State& state);

  // Return string representation.
  std::string ToString() const { return statistics_.ToString(); }

private:
  triton::client::Error UpdateModelData(const std::string& model, bool enable_cache);
  triton::client::Error UpdateInputMetadata(
    TritonModelData* model_data, const rapidjson::Document& model_metadata);
  triton::client::Error UpdateOutputMetadata(
    TritonModelData* model_data, const rapidjson::Document& model_metadata, size_t oidx);

  void LogTritonError(
    const triton::client::Error& err, const std::string& msg = std::string()) const;

  std::shared_ptr<common::Logger> logger_;
  std::unique_ptr<triton::client::InferenceServerHttpClient> client_;
  std::unordered_map<std::string, TritonModelData> model_datas_;

  TritonStatistics statistics_;
};

} // namespace cifre
