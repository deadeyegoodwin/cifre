// Copyright 2025, David Goodwin. All rights reserved.

#include <cmath>
#include <iomanip>
#include <sstream>

#include "cifre_triton.h"
#include "common_error.h"
#include "common_time.h"
#include "common_util.h"

namespace cifre {

namespace {

void
CheckTritonError(const triton::client::Error& err, const std::string& msg)
{
  if (!err.IsOk()) {
    std::stringstream sstr;
    sstr << "triton error: ";
    if (!msg.empty()) {
      sstr << msg << " - ";
    }
    sstr << err.Message();
    THROW(sstr.str());
  }
}

} // namespace

//
// TritonInferResult
//
TritonInferResult::TritonInferResult(
  const TritonModel& triton_model, const triton::client::InferResult* result) :
  input_shape_(triton_model.input_shape_), pi_shape_(triton_model.output_pi_shape_),
  v_shape_(triton_model.output_v_shape_), input_byte_size_(triton_model.required_input_byte_size_)
{
  ValidateResult(result, triton_model.output_v_name_, v_shape_);
  ValidateResult(result, triton_model.output_pi_name_, pi_shape_);

  float* fpi;
  CheckTritonError(result->RawData(triton_model.output_pi_name_, (const uint8_t**)&fpi, &pi_byte_size_),
                   "getting result data for '" + triton_model.output_pi_name_ + "'");
  THROWIF(pi_byte_size_ != triton_model.required_output_pi_byte_size_,
          "size mismatch for result '" + triton_model.output_pi_name_ + "'");

  float* fv;
  CheckTritonError(result->RawData(triton_model.output_v_name_, (const uint8_t**)&fv, &v_byte_size_),
                   "getting result data for '" + triton_model.output_v_name_ + "'");
  THROWIF(v_byte_size_ != triton_model.required_output_v_byte_size_,
          "size mismatch for result '" + triton_model.output_v_name_ + "'");

  pi_.reserve(PiEntryCount());
  for (size_t i = 0; i < PiEntryCount(); ++i) {
    pi_.push_back(std::exp(fpi[i]));
  }

  THROWIF(v_byte_size_ != sizeof(v_),
          "size mismatch for result '" + triton_model.output_v_name_ +
          "', expecting " + std::to_string(sizeof(v_)));
  v_ = *fv;
}

void
TritonInferResult::ValidateResult(
  const triton::client::InferResult* result, const std::string& name,
  const std::vector<int64_t>& shape) const
{
  std::string rdatatype;
  CheckTritonError(result->Datatype(name, &rdatatype), "validating result datatype for '" + name + "'");
  THROWIF(rdatatype != "FP32", "datatype mismatch for result '" + name + "'");

  std::vector<int64_t> rshape;
  CheckTritonError(result->Shape(name, &rshape), "validating result shape for '" + name + "'");
  THROWIF(shape != rshape, "shape mismatch for result '" + name + "'");
}

//
// TritonModelStatistics
//
std::string
TritonModelStatistics::ToString() const
{
  std::stringstream ss;

  assert(cache_lookup_count_ == prediction_count_);

  ss
    << std::fixed << std::setprecision(2)
    << "predictions: " << prediction_count_
    << ", inferences: " << inference_count_ << " ("
    << (100.0 * inference_count_ / prediction_count_) << "%)"
    << ", cache hits: " << cache_hit_count_ << " ("
    << (100.0 * cache_hit_count_ / prediction_count_) << "%)\n";
  ss
    << "cache:\n"
    << "  lookups: " << cache_lookup_count_ << ", ";
  if (cache_lookup_count_ > 0) {
    ss
      << std::fixed << std::setprecision(2)
      << "hits: " << cache_hit_count_ << " ("
      << (100.0 * cache_hit_count_ / cache_lookup_count_) << "%), "
      << "evicts: " << cache_evict_count_ << " ("
      << (100.0 * cache_evict_count_ / cache_lookup_count_) << "%)\n";
  }

  ss
    << "full sweeps: " << cache_sweep_count_
    << ", flushes: " << cache_flush_count_;

  return ss.str();
}

//
// TritonInferStatistics
//
std::string
TritonInferStatistics::ToString() const
{
  std::stringstream ss;

  const common::Time queue_avg_time =
    common::Time(queue_time_ns_ / 1000) / std::max(queue_count_, size_t(1));
  const common::Time compute_input_avg_time =
    common::Time(compute_input_time_ns_ / 1000) / std::max(compute_input_count_, size_t(1));
  const common::Time compute_infer_avg_time =
    common::Time(compute_infer_time_ns_ / 1000) / std::max(compute_infer_count_, size_t(1));
  const common::Time compute_output_avg_time =
    common::Time(compute_output_time_ns_ / 1000) / std::max(compute_output_count_, size_t(1));

  ss
    << "inference:\n"
    << "  inferences: " << inference_count_
    << ", batch-size avg: "
    << std::fixed << std::setprecision(2)
    << ((float)inference_count_ / execution_count_) << '\n'
    << "  queue avg: " << queue_avg_time.ToString()
    << ", compute input/infer/output avg: "
    << compute_input_avg_time.ToString() << " / "
    << compute_infer_avg_time.ToString() << " / "
    << compute_output_avg_time.ToString();

  return ss.str();
}

//
// TritonCache
//
std::shared_ptr<const TritonInferResult>
TritonCache::Lookup(const State& state)
{
  static std::shared_ptr<const TritonInferResult> miss;
  statistics_->cache_lookup_count_++;

  common::ScopedSpinlock sl(lock_);

  auto cache_itr = cache_lookup_.find(&state.TritonRepresentation());
  if (cache_itr == cache_lookup_.end()) {
    return miss;
  }

  statistics_->cache_hit_count_++;

  const size_t idx = cache_itr->second;
  assert(idx < cache_.size());
  assert(cache_[idx].valid_);

  cache_[idx].referenced_ = true;
  return cache_[idx].result_;
}

void
TritonCache::Insert(
  const State& state, const std::shared_ptr<const TritonInferResult>& result)
{
  common::ScopedSpinlock sl(lock_);

  const FlattenedCells& fc = state.TritonRepresentation();

  auto cache_itr = cache_lookup_.find(&fc);
  if (cache_itr == cache_lookup_.end()) {
    // 'state's flattened input cells not in cache so search forward
    // from 'clockhand_' to find a place to insert.
    while (cache_[clockhand_].valid_ && cache_[clockhand_].referenced_) {
      cache_[clockhand_].referenced_ = false;
      clockhand_ = (clockhand_ + 1) % cache_.size();
      if (clockhand_ == 0) {
        statistics_->cache_sweep_count_++;
      }
    }

    CacheEntry& entry = cache_[clockhand_];

    // Evict current cache entry.
#ifndef NDEBUG
    size_t ecnt =
#endif
      cache_lookup_.erase(&entry.inp_);
    assert((ecnt == 1) == entry.valid_);
    if (entry.valid_) {
      statistics_->cache_evict_count_++;
    }

    // Overwrite cache with new entry.
    entry.inp_.assign(fc.begin(), fc.end());
    entry.result_ = result;
    entry.referenced_ = false;
    entry.valid_ = true;

    cache_lookup_[&entry.inp_] = clockhand_;

    clockhand_ = (clockhand_ + 1) % cache_.size();
    if (clockhand_ == 0) {
      statistics_->cache_sweep_count_++;
    }
  }
}

void
TritonCache::Flush()
{
  common::ScopedSpinlock sl(lock_);

  cache_lookup_.clear();
  clockhand_ = 0;
  for (auto& entry : cache_) {
    entry.result_.reset();
    entry.valid_ = false;
  }

  statistics_->cache_flush_count_++;
}

//
// TritonModel
//
TritonModel::TritonModel(const std::string& model_name,
                         const std::shared_ptr<TritonCache>& cache,
                         const std::shared_ptr<TritonModelStatistics>& statistics,
                         const std::string& url, bool verbose) :
  model_name_(model_name), statistics_(statistics), cache_(cache)
{
  CheckTritonError(
    triton::client::InferenceServerHttpClient::Create(&client_, url, verbose),
    "unable to create http client for model '" + model_name_ + "' for server at " + url);

  std::string model_metadata_str;
  CheckTritonError(client_->ModelMetadata(&model_metadata_str, model_name_),
                   "unable to fetch triton model metadata");

  rapidjson::Document model_metadata;
  CheckTritonError(triton::client::ParseJson(&model_metadata, model_metadata_str),
                   "unable to parse triton model metadata");

  CheckTritonError(UpdateInputMetadata(model_metadata),
                   "unable to update model input metadata");
  CheckTritonError(UpdateOutputMetadata(model_metadata, 0),
                   "unable to update model output 0 metadata");
  CheckTritonError(UpdateOutputMetadata(model_metadata, 1),
                   "unable to update model output 1 metadata");
}

bool
TritonModel::IsReady() const
{
  bool ready;
  auto err = client_->IsModelReady(&ready, model_name_);
  if (!err.IsOk()) {
    ready = false;
  }

  return ready;
}

void
TritonModel::FlushCache()
{
  if (cache_ != nullptr) {
    cache_->Flush();
  }
}

std::shared_ptr<const TritonInferResult>
TritonModel::Infer(const State& state)
{
  statistics_->prediction_count_++;

  // See if 'model' has already run a prediction for 'state', and if
  // it has use of the cached result.
  if (cache_ != nullptr) {
    std::shared_ptr<const TritonInferResult> ires = cache_->Lookup(state);
    if (ires != nullptr) {
      return ires;
    }
  }

  const auto& state_triton_repr = state.TritonRepresentation();
  THROWIF(state_triton_repr.size() != required_input_byte_size_,
          "unexpected input size " + std::to_string(state_triton_repr.size()) +
          ", expected " + std::to_string(required_input_byte_size_));

  // Input...
  triton::client::InferInput* input;
  CheckTritonError(
    triton::client::InferInput::Create(&input, input_name_, input_shape_, "FP32"),
    "failed to create input '" + input_name_ + "'");
  std::unique_ptr<triton::client::InferInput> uinput(input);

  CheckTritonError(
    input->AppendRaw(state_triton_repr.data(), state_triton_repr.size()),
    "failed to set data for input '" + input_name_ + "'");

  std::vector<triton::client::InferInput*> inputs = { input };

  // Request all outputs...
  std::vector<const triton::client::InferRequestedOutput*> outputs = {};

  triton::client::InferOptions options(model_name_);
  triton::client::InferResult* result;
  auto err = client_->Infer(&result, options, inputs, outputs);
  CheckTritonError(err, "failed to run model '" + model_name_ + "'");
  statistics_->inference_count_++;

  auto ires = std::make_shared<const TritonInferResult>(*this, result);
  delete result;

  // Update cache with result.
  if (cache_ != nullptr) {
    cache_->Insert(state, ires);
  }

  return ires;
}

triton::client::Error
TritonModel::UpdateInputMetadata(const rapidjson::Document& model_metadata)
{
  const auto& input_itr = model_metadata.FindMember("inputs");
  const size_t input_count =
    (input_itr == model_metadata.MemberEnd()) ? 0 : input_itr->value.Size();
  if (input_count != 1) {
    return triton::client::Error("expecting 1 model input, got " + std::to_string(input_count));
  }

  // Must be one input of datatype FP32 with rank >= 2 shape where the
  // first dimension is the batch.
  const auto& input_metadata = *input_itr->value.Begin();
  input_name_ = std::string(
      input_metadata["name"].GetString(),
      input_metadata["name"].GetStringLength());

  const auto input_dtype_itr = input_metadata.FindMember("datatype");
  if (input_dtype_itr == input_metadata.MemberEnd()) {
    return triton::client::Error("expecting datatype for model input '" + input_name_ + "'");
  }

  if (std::string(input_dtype_itr->value.GetString(), input_dtype_itr->value.GetStringLength()) != "FP32") {
    return triton::client::Error("expecting datatype FP32 for model input '" + input_name_ + "'");
  }

  const auto input_shape_itr = input_metadata.FindMember("shape");
  if (input_shape_itr == input_metadata.MemberEnd()) {
    return triton::client::Error("expecting shape for model input '" + input_name_ + "'");
  }

  const rapidjson::Value& input_shape_json = input_shape_itr->value;
  if (input_shape_json.Size() < 2) {
    return triton::client::Error("expecting rank >= 2 shape for model input '" + input_name_ + "'");
  }

  if (input_shape_json[0].GetInt() != -1) {
    return triton::client::Error("expecting batch dimension for model input '" + input_name_ + "'");
  }

  // Shape includes the batch dimension (set to 1 as currently only
  // batch-size-1 requests are made).
  input_shape_.push_back(1);
  required_input_byte_size_ = 1;
  for (size_t idx = 1; idx < input_shape_json.Size(); ++idx) {
    const int d = input_shape_json[idx].GetInt();
    input_shape_.push_back(d);
    required_input_byte_size_ *= d;
  }
  required_input_byte_size_ *= sizeof(float);

  return triton::client::Error::Success;
}

triton::client::Error
TritonModel::UpdateOutputMetadata(const rapidjson::Document& model_metadata, size_t oidx)
{
  const auto& output_itr = model_metadata.FindMember("outputs");
  const size_t output_count =
    (output_itr == model_metadata.MemberEnd()) ? 0 : output_itr->value.Size();
  if (output_count != 2) {
    return triton::client::Error("expecting 2 model outputs, got " + std::to_string(output_count));
  }

  // Output must have datatype FP32 with rank 2 shape where the first
  // dimension is the batch.
  const auto& output_metadata = output_itr->value[oidx];
  const std::string& output_name = std::string(
    output_metadata["name"].GetString(),
    output_metadata["name"].GetStringLength());

  const auto output_dtype_itr = output_metadata.FindMember("datatype");
  if (output_dtype_itr == output_metadata.MemberEnd()) {
    return triton::client::Error("expecting datatype for model output '" + output_name + "'");
  }

  if (std::string(output_dtype_itr->value.GetString(), output_dtype_itr->value.GetStringLength()) != "FP32") {
    return triton::client::Error("expecting datatype FP32 for model output '" + output_name + "'");
  }

  const auto output_shape_itr = output_metadata.FindMember("shape");
  if (output_shape_itr == output_metadata.MemberEnd()) {
    return triton::client::Error("expecting shape for model output '" + output_name + "'");
  }

  const rapidjson::Value& output_shape_json = output_shape_itr->value;
  if (output_shape_json.Size() != 2) {
    return triton::client::Error("expecting rank 2 shape for model output '" + output_name + "'");
  }

  if (output_shape_json[0].GetInt() != -1) {
    return triton::client::Error("expecting batch dimension for model output '" + output_name + "'");
  }

  // If the non-batch shape is 1, then it is the 'v' output, otherwise
  // it is the 'pi' output. Assumption here is that there will be more
  // than one action so there is no ambiguity.
  if (output_shape_json[1].GetInt() == 1) {
    assert(output_v_name_.empty());
    output_v_name_ = std::move(output_name);

    // Shape includes the batch dimension (set to 1 as currently only
    // batch-size-1 requests are made).
    output_v_shape_.push_back(1);
    output_v_shape_.push_back(output_shape_json[1].GetInt());

    required_output_v_byte_size_ =
      output_v_shape_[0] * output_v_shape_[1] * sizeof(float);
  }
  else {
    assert(output_pi_name_.empty());
    output_pi_name_ = std::move(output_name);

    // Shape includes the batch dimension (set to 1 as currently only
    // batch-size-1 requests are made).
    output_pi_shape_.push_back(1);
    output_pi_shape_.push_back(output_shape_json[1].GetInt());

    required_output_pi_byte_size_ =
      output_pi_shape_[0] * output_pi_shape_[1] * sizeof(float);
  }

  return triton::client::Error::Success;
}

//
// TritonServer
//
TritonServer::TritonServer(const std::string& url, bool verbose) :
  url_(url)
{
  CheckTritonError(
    triton::client::InferenceServerHttpClient::Create(&client_, url_, verbose),
    "unable to create http client for server at " + url_);
}

TritonServer::~TritonServer()
{
  std::unique_lock<std::mutex> lk(client_mu_);

  for (const auto& mn : loaded_model_names_) {
    client_->UnloadModel(mn);
  }

  loaded_model_names_.clear();
}

bool
TritonServer::IsReady() const
{
  std::unique_lock<std::mutex> lk(client_mu_);

  bool ready;
  auto err = client_->IsServerReady(&ready);
  if (!err.IsOk()) {
    ready = false;
  }

  return ready;
}

std::unordered_map<std::string, const TritonRepoIndex>
TritonServer::ModelRepositoryIndex() const
{
  std::string model_repo_index_str;
  {
    std::unique_lock<std::mutex> lk(client_mu_);
    CheckTritonError(client_->ModelRepositoryIndex(&model_repo_index_str),
                     "unable to read repository index");
  }

  rapidjson::Document d;
  CheckTritonError(triton::client::ParseJson(&d, model_repo_index_str),
                   "failed to parse repository index json");
  THROWIF(!d.IsArray(), "expecting model repository index to be json array");

  std::unordered_map<std::string, const TritonRepoIndex> repo_index;

  for (auto& v : d.GetArray()) {
    THROWIF(!v.IsObject(), "expecting repository index entry to be json object");

    const std::string& model_name = v["name"].GetString();
    const std::string& model_version =
      (v.HasMember("version")) ? v["version"].GetString() : "";
    const std::string& model_state =
      (v.HasMember("state")) ? v["state"].GetString() : "";
    const std::string& model_state_reason =
      (v.HasMember("reason")) ? v["reason"].GetString() : "";

#ifndef NDEBUG
    auto created_repo_itr =
#endif
      repo_index.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(model_name),
        std::forward_as_tuple(model_name, model_version, model_state, model_state_reason));
    assert(created_repo_itr.second);
  }

  return repo_index;
}

std::unordered_map<std::string, const TritonInferStatistics>
TritonServer::InferenceStatistics() const
{
  std::string infer_stats_str;
  {
    std::unique_lock<std::mutex> lk(client_mu_);
    CheckTritonError(client_->ModelInferenceStatistics(&infer_stats_str),
                     "unable to read model inference statistics");
  }

  rapidjson::Document d;
  CheckTritonError(triton::client::ParseJson(&d, infer_stats_str),
                   "failed to parse inference statistics json");
  THROWIF(!d.IsObject(), "expecting inference statistics to be json object");
  THROWIF(!d["model_stats"].IsArray(), "expecting inference statistics to contain models array");

  // Parse statistics into map indexed by model name.
  std::unordered_map<std::string, const TritonInferStatistics> stats;
  for (const auto& stat : d["model_stats"].GetArray()) {
    TritonInferStatistics mstat;
    mstat.model_name_ = stat["name"].GetString();
    mstat.model_version_ = stat["version"].GetString();
    mstat.inference_count_ = stat["inference_count"].GetUint64();
    mstat.execution_count_ = stat["execution_count"].GetUint64();
    mstat.success_count_ = stat["inference_stats"]["success"]["count"].GetUint64();
    mstat.queue_count_ = stat["inference_stats"]["queue"]["count"].GetUint64();
    mstat.compute_input_count_ = stat["inference_stats"]["compute_input"]["count"].GetUint64();
    mstat.compute_infer_count_ = stat["inference_stats"]["compute_infer"]["count"].GetUint64();
    mstat.compute_output_count_ = stat["inference_stats"]["compute_output"]["count"].GetUint64();
    mstat.cumm_time_ns_ = stat["inference_stats"]["success"]["ns"].GetUint64();
    mstat.queue_time_ns_ = stat["inference_stats"]["queue"]["ns"].GetUint64();
    mstat.compute_input_time_ns_ = stat["inference_stats"]["compute_input"]["ns"].GetUint64();
    mstat.compute_infer_time_ns_ = stat["inference_stats"]["compute_infer"]["ns"].GetUint64();
    mstat.compute_output_time_ns_ = stat["inference_stats"]["compute_output"]["ns"].GetUint64();

#ifndef NDEBUG
    auto created_stat_itr =
#endif
      stats.emplace(mstat.model_name_, mstat);
    assert(created_stat_itr.second);
  }

  return stats;
}

void
TritonServer::FlushAllCaches()
{
  for (auto& pr : model_cache_) {
    pr.second->Flush();
  }
}

bool
TritonServer::LoadModel(const std::string& model_name, size_t cache_size)
{
  std::unique_lock<std::mutex> lk(client_mu_);

  auto err = client_->LoadModel(model_name);
  if (!err.IsOk()) {
    return false;
  }

  loaded_model_names_.insert(model_name);

  // Create statistics for model and cache doesn't already exist.
  std::shared_ptr<TritonModelStatistics> statistics;
  auto stats_itr = model_statistics_.find(model_name);
  if (stats_itr == model_statistics_.end()) {
    statistics.reset(new TritonModelStatistics());
    model_statistics_[model_name] = statistics;
  }
  else {
    statistics = stats_itr->second;
  }

  // Create a cache for model if requested and doesn't already exist.
  if (cache_size > 0) {
    auto cache_itr = model_cache_.find(model_name);
    if (cache_itr == model_cache_.end()) {
      model_cache_[model_name] = std::make_shared<TritonCache>(cache_size, statistics);
    }
  }

  return true;
}

bool
TritonServer::UnloadModel(const std::string& model_name)
{
  std::unique_lock<std::mutex> lk(client_mu_);

  auto err = client_->UnloadModel(model_name);
  if (!err.IsOk()) {
    return false;
  }

  loaded_model_names_.erase(model_name);
  model_cache_.erase(model_name);

  return true;
}

TritonModel*
TritonServer::CreateTritonModel(const std::string& model_name, bool verbose)
{
  // If 'model_name' is using a cache, then provide it to the
  // TritonModel so that it can use it when inferring.
  std::shared_ptr<TritonCache> cache;
  auto cache_itr = model_cache_.find(model_name);
  if (cache_itr != model_cache_.end()) {
    cache = cache_itr->second;
  }

  // Find the statistics for the model.
  auto stats_itr = model_statistics_.find(model_name);
  THROWIF(stats_itr == model_statistics_.end(), "expecting statistics for model");

  return new TritonModel(model_name, cache, stats_itr->second, url_, verbose);
}

std::string
TritonServer::ToString() const
{
  std::stringstream ss;

  const auto& infer_stats = InferenceStatistics();

  bool first = true;
  for (auto& pr : model_statistics_) {
    const std::string& model_name = pr.first;
    const auto& stats = pr.second;

    if (!first) {
      ss << '\n';
    }

    ss
      << "model " << model_name << '\n'
      << common::DuplicateAndIndent(stats->ToString(), 2 /* indent */);

    auto infer_stats_itr = infer_stats.find(model_name);
    if (infer_stats_itr != infer_stats.end()) {
      const TritonInferStatistics& infer_stat = infer_stats_itr->second;
      ss << '\n' << common::DuplicateAndIndent(infer_stat.ToString(), 2 /* indent */);
    }

    first = false;
  }

  return ss.str();
}

} // namespace cifre
