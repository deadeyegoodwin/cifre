// Copyright 2025, David Goodwin. All rights reserved.
#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <set>
#include <unordered_map>

#include "cifre_cell.h"
#include "cifre_state.h"
#include "http_client.h"
#include "json_utils.h"

namespace cifre {

class TritonServer;
class TritonModel;

//
// TritonInferResult
//
// The output tensors returned by an inference. A TritonInferResult
// can be cached to avoid multiple identical inference requests being
// sent to triton server. To avoid the cached object being modified,
// all methods are const.
//
class TritonInferResult {
public:
  TritonInferResult(const TritonModel& triton_model, const triton::client::InferResult* result);

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
// TritonModelStatistics
//
// Per-model statistics.
//
struct TritonModelStatistics {
  // Return string representation of statistics.
  std::string ToString() const;

  std::atomic<size_t> prediction_count_ {0};
  std::atomic<size_t> inference_count_ {0};

  std::atomic<size_t> cache_lookup_count_ {0};
  std::atomic<size_t> cache_hit_count_ {0};
  std::atomic<size_t> cache_evict_count_ {0};
  std::atomic<size_t> cache_sweep_count_ {0};
  std::atomic<size_t> cache_flush_count_ {0};
};

//
// TritonCache
//
// Cache results to avoid running inference multiple times on the same
// model input. The cache is organized using the "clock" algorithm to
// provide a simple approximation to LRU. All methods are thread-safe.
//
class TritonCache {
public:
  TritonCache(
    size_t cache_size, const std::shared_ptr<TritonModelStatistics>& statistics) :
    clockhand_(0), cache_(cache_size), statistics_(statistics) {
    assert(cache_size > 0); }

  // Flush all entries from cache.
  void Flush();

  // Lookup 'state' and return any cached result.
  std::shared_ptr<const TritonInferResult> Lookup(const State& state);

  // Insert 'result' as the prediction for 'state', if there is not
  // already an entry.
  void Insert(
    const State& state, const std::shared_ptr<const TritonInferResult>& result);

private:
  //
  // CacheEntry
  //
  // Each cache entry holds the State, the corresponding result
  // tensors and the reference bit.
  //
  struct CacheEntry {
    CacheEntry() : valid_(false) { }
    StateUniqueConstPtr state_;
    std::shared_ptr<const TritonInferResult> result_;
    bool valid_;
    bool referenced_;
  };

  // The current insert point into the cache.
  size_t clockhand_;

  // 'cache_lookup_' is map from State to the index of the cache entry
  // in 'cache_'. The key is a pointer to the State held in the
  // corresponding CacheEntry.
  std::unordered_map<const State*, size_t, StateHash, StateEqual> cache_lookup_;
  std::vector<CacheEntry> cache_;

  // Lock providing multiple-reader, single writing synchronization.
  std::shared_mutex mtx_;

  // Statistics for the cache. This object is shared across all
  // models and caches that represent the same nnet model.
  std::shared_ptr<TritonModelStatistics> statistics_;
};

//
// TritonModel
//
// Provides access to triton model for a single thread. Methods of
// this class are not thread safe and the intended usage is that each
// thread that wants to access a model must create its own TritonModel
// object. Having a TritonModel object for a particular model does not
// guarantee that the model is available on the corresponding triton
// server; loading of the model must be managed independently with
// TritonServer.
//
class TritonModel {
public:
  // Return the name of the model.
  const std::string& Name() const { return model_name_; }

  // Return true if the model is ready to receive inference requests.
  bool IsReady() const;

  // Flush all entries from this model's cache.
  void FlushCache();

  // Run model with 'state' as input to get a prediction result. If
  // caching is enable the result can be returned from the cache
  // instead of needing to be generated from triton server.
  std::shared_ptr<const TritonInferResult> Infer(const State& state);

private:
  friend class TritonServer;
  friend class TritonInferResult;

  // Create object that provides access to 'model_name' model running
  // on server 'triton_server'. This method is not publically visible,
  // use TritonServer::CreateTritonModel.
  TritonModel(const std::string& model_name,
              const std::shared_ptr<TritonCache>& cache,
              const std::shared_ptr<TritonModelStatistics>& statistics,
              const std::string& url, bool verbose);

  triton::client::Error UpdateInputMetadata(
    const rapidjson::Document& model_metadata);
  triton::client::Error UpdateOutputMetadata(
    const rapidjson::Document& model_metadata, size_t oidx);

  // Name of the model represented by this object.
  const std::string model_name_;

  // Statistics for the model. This object is shared across all
  // TritonModels that represent the same nnet model.
  std::shared_ptr<TritonModelStatistics> statistics_;

  // Client to access the triton server. See TritonModel description
  // above as to why there is no mutex for this object even though
  // InferenceServerHttpClient is not thread safe.
  std::unique_ptr<triton::client::InferenceServerHttpClient> client_;

  // If non-nullptr, then cache inference results to avoid redundant
  // server inference requests.
  std::shared_ptr<TritonCache> cache_;

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
};

//
// TritonRepoIndex
//
// Repository index results returned by
// TritonServer::ModelRepositoryIndex.
//
struct TritonRepoIndex {
  TritonRepoIndex(const std::string& n, const std::string& v,
                  const std::string& s, const std::string& r) :
    model_name_(n), model_version_(v), model_state_(s), model_state_reason_(r) { }
  const std::string model_name_;
  const std::string model_version_;
  const std::string model_state_;
  const std::string model_state_reason_;
};

//
// TritonInferStatistics
//
// Model inference statistics returned by
// TritonServer::InferenceStatistics.
//
struct TritonInferStatistics {
  // Return string representation of statistics.
  std::string ToString() const;

  std::string model_name_;
  std::string model_version_;
  uint64_t inference_count_;
  uint64_t execution_count_;
  uint64_t success_count_;
  uint64_t queue_count_;
  uint64_t compute_input_count_;
  uint64_t compute_infer_count_;
  uint64_t compute_output_count_;
  uint64_t cumm_time_ns_;
  uint64_t queue_time_ns_;
  uint64_t compute_input_time_ns_;
  uint64_t compute_infer_time_ns_;
  uint64_t compute_output_time_ns_;
};

//
// TritonServer
//
// Interface to Triton inference server. All methods are thread-safe
// and all methods that access the server (IsReady, LoadModel,
// UnloadModel, ModelRepositoryIndex) are serialized.
//
class TritonServer {
public:
  // Create an object for accessing the triton server running at
  // 'url'. Set triton client interface output to verbose if 'verbose'
  // is true.
  TritonServer(const std::string& url, bool verbose = false);
  ~TritonServer();

  // Return true if triton is ready to receive inference requests.
  bool IsReady() const;

  // Return index of models in the triton model repository, as a map
  // from model name to index information about the model.
  std::unordered_map<std::string, const TritonRepoIndex> ModelRepositoryIndex() const;

  // Return a map from model name to a TritonInferStatistics object
  // holding the inference statistics for the model.
  std::unordered_map<std::string, const TritonInferStatistics> InferenceStatistics() const;

  // Flush all entries from all caches.
  void FlushAllCaches();

  // Load 'model_name' into triton. Enable caching for the model if
  // 'cache_size' is greater than 0. Nothing is done if the model is
  // already loaded into triton. Return true if model load is
  // successful.
  bool LoadModel(const std::string& model_name, size_t cache_size);

  // Unload 'model_name' from triton. Remove any results cached for the
  // model. Return true if successful.
  bool UnloadModel(const std::string& model_name);

  // Return a TritonModel object that can be used to access
  // 'model_name' on this server.  Set triton client interface output
  // to verbose if 'verbose' is true.
  TritonModel* CreateTritonModel(const std::string& model_name, bool verbose = false);

  // Return string representation.
  std::string ToString() const;

private:
  const std::string url_;

  // Client to access the triton server. InferenceServerHttpClient is
  // not thread-safe so use of 'client_' is protected by 'client_mu_'.
  std::unique_ptr<triton::client::InferenceServerHttpClient> client_;

  // Mutex to serialize access to 'client_'.
  mutable std::mutex client_mu_;

  // Names of all models loaded by this TritonServer that have not
  // been unloaded. Protected by 'client_mu_'.
  std::set<std::string> loaded_model_names_;

  // Map from model name to statistics for that model. All TritonModel
  // objects that represent the same model name share a
  // TritonModelStatistics object from this map.
  std::unordered_map<std::string, std::shared_ptr<TritonModelStatistics>> model_statistics_;

  // Map from model name to the cache of results for that model.
  std::unordered_map<std::string, std::shared_ptr<TritonCache>> model_cache_;
};

} // namespace cifre
