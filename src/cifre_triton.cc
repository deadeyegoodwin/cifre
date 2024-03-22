// Copyright 2024, David Goodwin. All rights reserved.

#include <cmath>
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
  const TritonModelMetadata& metadata, triton::client::InferResult* r) :
  result_(r), input_shape_(metadata.input_shape_),
  pi_shape_(metadata.output_pi_shape_), v_shape_(metadata.output_v_shape_),
  input_byte_size_(metadata.required_input_byte_size_)
{
  ValidateResult(metadata.output_v_name_, v_shape_);
  ValidateResult(metadata.output_pi_name_, pi_shape_);

  float* fpi;
  CheckTritonError(result_->RawData(metadata.output_pi_name_, (const uint8_t**)&fpi, &pi_byte_size_),
                   "getting result data for '" + metadata.output_pi_name_ + "'");
  THROWIF(pi_byte_size_ != metadata.required_output_pi_byte_size_,
          "size mismatch for result '" + metadata.output_pi_name_ + "'");

  CheckTritonError(result_->RawData(metadata.output_v_name_, (const uint8_t**)&v_, &v_byte_size_),
                   "getting result data for '" + metadata.output_v_name_ + "'");
  THROWIF(v_byte_size_ != metadata.required_output_v_byte_size_,
          "size mismatch for result '" + metadata.output_v_name_ + "'");

  pi_.reserve(PiEntryCount());
  for (size_t i = 0; i < PiEntryCount(); ++i) {
    pi_.push_back(std::exp(fpi[i]));
  }

  // FIXME now that we calculate pi_, do we need to keep result_?
  // Instead why not just save 'v' and delete result_.
}

void
TritonInferResult::ValidateResult(const std::string& name, const std::vector<int64_t>& shape) const
{
  std::string rdatatype;
  CheckTritonError(result_->Datatype(name, &rdatatype), "validating result datatype for '" + name + "'");
  THROWIF(rdatatype != "FP32", "datatype mismatch for result '" + name + "'");

  std::vector<int64_t> rshape;
  CheckTritonError(result_->Shape(name, &rshape), "validating result shape for '" + name + "'");
  THROWIF(shape != rshape, "shape mismatch for result '" + name + "'");
}

TritonInferResult::~TritonInferResult()
{
  delete result_;
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
  // 'model_metadata_' so need to do this in a safe way.
  std::vector<std::string> mnames;
  for (const auto& kv : model_metadata_) {
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

bool
Triton::LoadModel(const std::string& model)
{
  auto err = client_->LoadModel(model);
  if (!err.IsOk()) {
    LogTritonError(err, "unable to load model");
    return false;
  }

  err = UpdateModelMetadata(model);
  if (!err.IsOk()) {
    LogTritonError(err, "model metadata");
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

  model_metadata_.erase(model);

  return true;
}


TritonInferResult
Triton::RunModel(
  const std::string& model, const std::vector<uint8_t>& state_triton_repr) const
{
  auto metadata_itr = model_metadata_.find(model);
  THROWIF(metadata_itr == model_metadata_.end(),
          "no metadata found for model '" + model + "'");

  const TritonModelMetadata& metadata = metadata_itr->second;

  THROWIF(state_triton_repr.size() != metadata.required_input_byte_size_,
          "unexpected input size " + std::to_string(state_triton_repr.size()) +
          ", expected " + std::to_string(metadata.required_input_byte_size_));

  // Input...
  triton::client::InferInput* input;
  CheckTritonError(
    triton::client::InferInput::Create(&input, metadata.input_name_, metadata.input_shape_, "FP32"),
    "failed to create input '" + metadata.input_name_ + "'");
  std::unique_ptr<triton::client::InferInput> uinput(input);

  CheckTritonError(
    input->AppendRaw(state_triton_repr.data(), state_triton_repr.size()),
    "failed to set data for input '" + metadata.input_name_ + "'");

  std::vector<triton::client::InferInput*> inputs = { input };

  // Request all outputs...
  std::vector<const triton::client::InferRequestedOutput*> outputs = {};

  triton::client::InferOptions options(model);
  triton::client::InferResult* result;
  auto err = client_->Infer(&result, options, inputs, outputs);
  CheckTritonError(err, "failed to run model '" + model + "'");

  return TritonInferResult(metadata, result);
}

triton::client::Error
Triton::UpdateModelMetadata(const std::string& model)
{
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

  TritonModelMetadata metadata;

  // Collect information about model input.
  err = UpdateInputMetadata(&metadata, model_metadata);
  if (!err.IsOk()) {
    return err;
  }

  // Collect information about model outputs.
  err = UpdateOutputMetadata(&metadata, model_metadata, 0);
  if (!err.IsOk()) {
    return err;
  }
  err = UpdateOutputMetadata(&metadata, model_metadata, 1);
  if (!err.IsOk()) {
    return err;
  }

  model_metadata_[model] = metadata;

  return triton::client::Error::Success;
}

triton::client::Error
Triton::UpdateInputMetadata(TritonModelMetadata* metadata, const rapidjson::Document& model_metadata)
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
  metadata->input_name_ = std::string(
      input_metadata["name"].GetString(),
      input_metadata["name"].GetStringLength());

  const auto input_dtype_itr = input_metadata.FindMember("datatype");
  if (input_dtype_itr == input_metadata.MemberEnd()) {
    return triton::client::Error("expecting datatype for model input '" + metadata->input_name_ + "'");
  }

  if (std::string(input_dtype_itr->value.GetString(), input_dtype_itr->value.GetStringLength()) != "FP32") {
    return triton::client::Error("expecting datatype FP32 for model input '" + metadata->input_name_ + "'");
  }

  const auto input_shape_itr = input_metadata.FindMember("shape");
  if (input_shape_itr == input_metadata.MemberEnd()) {
    return triton::client::Error("expecting shape for model input '" + metadata->input_name_ + "'");
  }

  const rapidjson::Value& input_shape_json = input_shape_itr->value;
  if (input_shape_json.Size() != 3) {
    return triton::client::Error("expecting rank 3 shape for model input '" + metadata->input_name_ + "'");
  }

  if (input_shape_json[0].GetInt() != -1) {
    return triton::client::Error("expecting batch dimension for model input '" + metadata->input_name_ + "'");
  }

  // Shape includes the batch dimension (set to 1 as currently only
  // batch-size-1 requests are made).
  metadata->input_shape_.push_back(1);
  metadata->input_shape_.push_back(input_shape_json[1].GetInt());
  metadata->input_shape_.push_back(input_shape_json[2].GetInt());
  metadata->required_input_byte_size_ =
    metadata->input_shape_[0] * metadata->input_shape_[1] * metadata->input_shape_[2] * sizeof(float);

  return triton::client::Error::Success;
}

triton::client::Error
Triton::UpdateOutputMetadata(
  TritonModelMetadata* metadata, const rapidjson::Document& model_metadata, size_t oidx)
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
    assert(metadata->output_v_name_.empty());
    metadata->output_v_name_ = std::move(output_name);

    // Shape includes the batch dimension (set to 1 as currently only
    // batch-size-1 requests are made).
    metadata->output_v_shape_.push_back(1);
    metadata->output_v_shape_.push_back(output_shape_json[1].GetInt());

    metadata->required_output_v_byte_size_ =
      metadata->output_v_shape_[0] * metadata->output_v_shape_[1] * sizeof(float);
  }
  else {
    assert(metadata->output_pi_name_.empty());
    metadata->output_pi_name_ = std::move(output_name);

    // Shape includes the batch dimension (set to 1 as currently only
    // batch-size-1 requests are made).
    metadata->output_pi_shape_.push_back(1);
    metadata->output_pi_shape_.push_back(output_shape_json[1].GetInt());

    metadata->required_output_pi_byte_size_ =
      metadata->output_pi_shape_[0] * metadata->output_pi_shape_[1] * sizeof(float);
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
