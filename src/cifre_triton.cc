// Copyright 2024, David Goodwin. All rights reserved.

#include <cmath>
#include <iomanip>
#include <sstream>

#include "cifre_triton.h"
#include "common_error.h"

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
  const TritonModelData& model_data, const triton::client::InferResult* result) :
  input_shape_(model_data.input_shape_), pi_shape_(model_data.output_pi_shape_),
  v_shape_(model_data.output_v_shape_), input_byte_size_(model_data.required_input_byte_size_)
{
  ValidateResult(result, model_data.output_v_name_, v_shape_);
  ValidateResult(result, model_data.output_pi_name_, pi_shape_);

  float* fpi;
  CheckTritonError(result->RawData(model_data.output_pi_name_, (const uint8_t**)&fpi, &pi_byte_size_),
                   "getting result data for '" + model_data.output_pi_name_ + "'");
  THROWIF(pi_byte_size_ != model_data.required_output_pi_byte_size_,
          "size mismatch for result '" + model_data.output_pi_name_ + "'");

  float* fv;
  CheckTritonError(result->RawData(model_data.output_v_name_, (const uint8_t**)&fv, &v_byte_size_),
                   "getting result data for '" + model_data.output_v_name_ + "'");
  THROWIF(v_byte_size_ != model_data.required_output_v_byte_size_,
          "size mismatch for result '" + model_data.output_v_name_ + "'");

  pi_.reserve(PiEntryCount());
  for (size_t i = 0; i < PiEntryCount(); ++i) {
    pi_.push_back(std::exp(fpi[i]));
  }

  THROWIF(v_byte_size_ != sizeof(v_),
          "size mismatch for result '" + model_data.output_v_name_ +
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
// TritonStatistics
//
std::string
TritonStatistics::ToString() const
{
  std::stringstream ss;

  ss
    << std::fixed << std::setprecision(2)
    << "predictions: " << prediction_count_
    << ", inferences: " << inference_count_ << " ("
    << (100.0 * inference_count_ / prediction_count_) << "%)"
    << ", cache hits: " << cache_hit_count_ << " ("
    << (100.0 * cache_hit_count_ / prediction_count_) << "%)";

  return ss.str();
}

//
// Triton
//
Triton::Triton(const std::shared_ptr<common::Logger>& logger) :
  logger_(logger)
{
  const std::string url("localhost:8000");
  const bool verbose = false;
  CheckTritonError(
    triton::client::InferenceServerHttpClient::Create(&client_, url, verbose),
    "unable to create http client");
}

Triton::~Triton()
{
  // Attempt to unload all loaded models. Unloading model will modify
  // 'model_datas_' so need to do this in a safe way.
  std::vector<std::string> mnames;
  for (const auto& kv : model_datas_) {
    mnames.push_back(kv.first);
  }

  for (const auto& n : mnames) {
    UnloadModel(n);
  }
}

bool
Triton::IsReady() const
{
  bool ready;
  auto err = client_->IsServerReady(&ready);
  if (!err.IsOk()) {
    LogTritonError(err, "unable to get server readiness");
    ready = false;
  }

  return ready;
}

bool
Triton::IsModelReady(const std::string& model) const
{
  bool ready;
  auto err = client_->IsModelReady(&ready, model);
  if (!err.IsOk()) {
    LogTritonError(err, "unable to get model readiness");
    ready = false;
  }

  return ready;
}

std::unordered_map<std::string, const TritonRepoIndex>
Triton::ModelRepositoryIndex() const
{
  std::string model_repo_index_str;
  CheckTritonError(client_->ModelRepositoryIndex(&model_repo_index_str),
                   "unable to read repository index");

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

bool
Triton::LoadModel(const std::string& model, bool enable_cache)
{
  auto err = client_->LoadModel(model);
  if (!err.IsOk()) {
    LogTritonError(err, "unable to load model");
    return false;
  }

  err = UpdateModelData(model, enable_cache);
  if (!err.IsOk()) {
    LogTritonError(err, "update model metadata");
    return false;
  }

  return true;
}

bool
Triton::UnloadModel(const std::string& model)
{
  auto err = client_->UnloadModel(model);
  if (!err.IsOk()) {
    LogTritonError(err, "unable to unload model");
    return false;
  }

  model_datas_.erase(model);

  return true;
}

std::shared_ptr<const TritonInferResult>
Triton::RunModel(const std::string& model, const State& state)
{
  statistics_.prediction_count_++;

  auto model_data_itr = model_datas_.find(model);
  THROWIF(model_data_itr == model_datas_.end(),
          "no data found for model '" + model + "'");

  TritonModelData& model_data = model_data_itr->second;

  // If caching is enabled for 'model', then see if 'model' has
  // already run a prediction for 'state', and if it has use the
  // cached result.
  if (model_data.cache_enabled_) {
    auto cache_itr = model_data.cache_.find(&state);
    if (cache_itr != model_data.cache_.end()) {
      statistics_.cache_hit_count_++;
      return cache_itr->second.result_;
    }
  }

  const auto& state_triton_repr = state.TritonRepresentation();
  THROWIF(state_triton_repr.size() != model_data.required_input_byte_size_,
          "unexpected input size " + std::to_string(state_triton_repr.size()) +
          ", expected " + std::to_string(model_data.required_input_byte_size_));

  // Input...
  triton::client::InferInput* input;
  CheckTritonError(
    triton::client::InferInput::Create(&input, model_data.input_name_, model_data.input_shape_, "FP32"),
    "failed to create input '" + model_data.input_name_ + "'");
  std::unique_ptr<triton::client::InferInput> uinput(input);

  CheckTritonError(
    input->AppendRaw(state_triton_repr.data(), state_triton_repr.size()),
    "failed to set data for input '" + model_data.input_name_ + "'");

  std::vector<triton::client::InferInput*> inputs = { input };

  // Request all outputs...
  std::vector<const triton::client::InferRequestedOutput*> outputs = {};

  triton::client::InferOptions options(model);
  triton::client::InferResult* result;
  auto err = client_->Infer(&result, options, inputs, outputs);
  CheckTritonError(err, "failed to run model '" + model + "'");
  statistics_.inference_count_++;

  auto ires = std::make_shared<const TritonInferResult>(model_data, result);
  delete result;

  // Update cache with result.
  if (model_data.cache_enabled_) {
    std::unique_ptr<const State> ce_state(state.Clone());
#ifndef NDEBUG
    auto created_cache_itr =
#endif
      model_data.cache_.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(ce_state.get()),
        std::forward_as_tuple(std::move(ce_state), ires));
    assert(created_cache_itr.second);
  }

  return ires;
}

triton::client::Error
Triton::UpdateModelData(const std::string& model, bool enable_cache)
{
  TritonModelData model_data;
  model_data.cache_enabled_ = enable_cache;
  model_data.cache_.clear();

  std::string model_metadata_str;
  auto err = client_->ModelMetadata(&model_metadata_str, model);
  if (!err.IsOk()) {
    return err;
  }

  rapidjson::Document model_metadata;
  err = triton::client::ParseJson(&model_metadata, model_metadata_str);
  if (!err.IsOk()) {
    return err;
  }

  // Collect information about model input.
  err = UpdateInputMetadata(&model_data, model_metadata);
  if (!err.IsOk()) {
    return err;
  }

  // Collect information about model outputs.
  err = UpdateOutputMetadata(&model_data, model_metadata, 0);
  if (!err.IsOk()) {
    return err;
  }
  err = UpdateOutputMetadata(&model_data, model_metadata, 1);
  if (!err.IsOk()) {
    return err;
  }

  model_datas_[model] = std::move(model_data);

  return triton::client::Error::Success;
}

triton::client::Error
Triton::UpdateInputMetadata(TritonModelData* model_data, const rapidjson::Document& model_metadata)
{
  const auto& input_itr = model_metadata.FindMember("inputs");
  const size_t input_count =
    (input_itr == model_metadata.MemberEnd()) ? 0 : input_itr->value.Size();
  if (input_count != 1) {
    return triton::client::Error("expecting 1 model input, got " + std::to_string(input_count));
  }

  // Must be one input of datatype FP32 with rank 3 shape where the
  // first dimension is the batch.
  const auto& input_metadata = *input_itr->value.Begin();
  model_data->input_name_ = std::string(
      input_metadata["name"].GetString(),
      input_metadata["name"].GetStringLength());

  const auto input_dtype_itr = input_metadata.FindMember("datatype");
  if (input_dtype_itr == input_metadata.MemberEnd()) {
    return triton::client::Error("expecting datatype for model input '" + model_data->input_name_ + "'");
  }

  if (std::string(input_dtype_itr->value.GetString(), input_dtype_itr->value.GetStringLength()) != "FP32") {
    return triton::client::Error("expecting datatype FP32 for model input '" + model_data->input_name_ + "'");
  }

  const auto input_shape_itr = input_metadata.FindMember("shape");
  if (input_shape_itr == input_metadata.MemberEnd()) {
    return triton::client::Error("expecting shape for model input '" + model_data->input_name_ + "'");
  }

  const rapidjson::Value& input_shape_json = input_shape_itr->value;
  if (input_shape_json.Size() != 3) {
    return triton::client::Error("expecting rank 3 shape for model input '" + model_data->input_name_ + "'");
  }

  if (input_shape_json[0].GetInt() != -1) {
    return triton::client::Error("expecting batch dimension for model input '" + model_data->input_name_ + "'");
  }

  // Shape includes the batch dimension (set to 1 as currently only
  // batch-size-1 requests are made).
  model_data->input_shape_.push_back(1);
  model_data->input_shape_.push_back(input_shape_json[1].GetInt());
  model_data->input_shape_.push_back(input_shape_json[2].GetInt());
  model_data->required_input_byte_size_ =
    model_data->input_shape_[0] * model_data->input_shape_[1] * model_data->input_shape_[2] * sizeof(float);

  return triton::client::Error::Success;
}

triton::client::Error
Triton::UpdateOutputMetadata(
  TritonModelData* model_data, const rapidjson::Document& model_metadata, size_t oidx)
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
    assert(model_data->output_v_name_.empty());
    model_data->output_v_name_ = std::move(output_name);

    // Shape includes the batch dimension (set to 1 as currently only
    // batch-size-1 requests are made).
    model_data->output_v_shape_.push_back(1);
    model_data->output_v_shape_.push_back(output_shape_json[1].GetInt());

    model_data->required_output_v_byte_size_ =
      model_data->output_v_shape_[0] * model_data->output_v_shape_[1] * sizeof(float);
  }
  else {
    assert(model_data->output_pi_name_.empty());
    model_data->output_pi_name_ = std::move(output_name);

    // Shape includes the batch dimension (set to 1 as currently only
    // batch-size-1 requests are made).
    model_data->output_pi_shape_.push_back(1);
    model_data->output_pi_shape_.push_back(output_shape_json[1].GetInt());

    model_data->required_output_pi_byte_size_ =
      model_data->output_pi_shape_[0] * model_data->output_pi_shape_[1] * sizeof(float);
  }

  return triton::client::Error::Success;
}

void
Triton::LogTritonError(const triton::client::Error& err, const std::string& msg) const
{
  if (!err.IsOk()) {
    std::stringstream sstr;
    sstr << "triton error: ";
    if (!msg.empty()) {
      sstr << msg << " - ";
    }
    sstr << err.Message();
    logger_->Log(sstr.str());
  }
}

} // namespace cifre
