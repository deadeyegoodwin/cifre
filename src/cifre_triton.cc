// Copyright 2024, David Goodwin. All rights reserved.

#include <sstream>

#include "cifre_triton.h"
#include "common_error.h"

namespace cifre {

Triton::Triton(common::Logger* logger) :
  logger_(logger)
{
  const std::string url("localhost:8000");
  const bool verbose = false;
  CheckTritonError(
    triton::client::InferenceServerHttpClient::Create(&client_, url, verbose),
    "unable to create http client");
}

bool
Triton::IsReady()
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
Triton::IsModelReady(const std::string& model)
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
    LogTritonError(err, "unable to access model metadata");
    return false;
  }

  return true;
}

void
Triton::RunModel(const std::string& model)
{
  triton::client::InferOptions options(model);

  std::vector<int64_t> shape{1, 64};

  triton::client::InferInput* input;
  CheckTritonError(
    triton::client::InferInput::Create(&input, "INPUT0", shape, "FP32"),
    "failed to create triton input");
  std::unique_ptr<triton::client::InferInput> uinput(input);

  std::vector<triton::client::InferInput*> inputs = { input };

  // Request all outputs...
  std::vector<const triton::client::InferRequestedOutput*> outputs = {};

  triton::client::InferResult* result;
  auto err = client_->Infer(&result, options, inputs, outputs);
  CheckTritonError(err, "failed to run model '" + model + "'");
}

triton::client::Error
Triton::UpdateModelMetadata(const std::string& model)
{
  std::string model_metadata_str;
  auto err = client_->ModelMetadata(&model_metadata_str, model);
  if (!err.IsOk()) {
    LogTritonError(err, "failed to get model metadata");
    return err;
  }

  rapidjson::Document model_metadata;
  err = triton::client::ParseJson(&model_metadata, model_metadata_str);
  if (!err.IsOk()) {
    LogTritonError(err, "failed to parse model metadata");
    return err;
  }

  ModelMetadata metadata;

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
Triton::UpdateInputMetadata(ModelMetadata* metadata, const rapidjson::Document& model_metadata)
{
  const auto& input_itr = model_metadata.FindMember("inputs");
  const size_t input_count =
    (input_itr == model_metadata.MemberEnd()) ? 0 : input_itr->value.Size();
  if (input_count != 1) {
    triton::client::Error err("expecting 1 model input, got " + std::to_string(input_count));
    LogTritonError(err, "unsupported model metadata");
    return err;
  }

  // Must be one input of datatype FP32 with rank 3 shape where the
  // first dimension is the batch.
  const auto& input_metadata = *input_itr->value.Begin();
  metadata->input_name_ = std::string(
      input_metadata["name"].GetString(),
      input_metadata["name"].GetStringLength());

  const auto input_dtype_itr = input_metadata.FindMember("datatype");
  if (input_dtype_itr == input_metadata.MemberEnd()) {
    triton::client::Error err("expecting datatype for model input '" + metadata->input_name_ + "'");
    LogTritonError(err, "unsupported model metadata");
    return err;
  }

  if (std::string(input_dtype_itr->value.GetString(), input_dtype_itr->value.GetStringLength()) != "FP32") {
    triton::client::Error err("expecting datatype FP32 for model input '" + metadata->input_name_ + "'");
    LogTritonError(err, "unsupported model metadata");
    return err;
  }

  const auto input_shape_itr = input_metadata.FindMember("shape");
  if (input_shape_itr == input_metadata.MemberEnd()) {
    triton::client::Error err("expecting shape for model input '" + metadata->input_name_ + "'");
    LogTritonError(err, "unsupported model metadata");
    return err;
  }

  const rapidjson::Value& input_shape_json = input_shape_itr->value;
  if (input_shape_json.Size() != 3) {
    triton::client::Error err("expecting rank 3 shape for model input '" + metadata->input_name_ + "'");
    LogTritonError(err, "unsupported model metadata");
    return err;
  }

  if (input_shape_json[0].GetInt() != -1) {
    triton::client::Error err("expecting batch dimension for model input '" + metadata->input_name_ + "'");
    LogTritonError(err, "unsupported model metadata");
    return err;
  }

  metadata->input_shape_.push_back(input_shape_json[1].GetInt());
  metadata->input_shape_.push_back(input_shape_json[2].GetInt());

  return triton::client::Error::Success;
}

triton::client::Error
Triton::UpdateOutputMetadata(
  ModelMetadata* metadata, const rapidjson::Document& model_metadata, size_t oidx)
{
  const auto& output_itr = model_metadata.FindMember("outputs");
  const size_t output_count =
    (output_itr == model_metadata.MemberEnd()) ? 0 : output_itr->value.Size();
  if (output_count != 2) {
    triton::client::Error err("expecting 2 model outputs, got " + std::to_string(output_count));
    LogTritonError(err, "unsupported model metadata");
    return err;
  }

  // Output must have datatype FP32 with rank 2 shape where the first
  // dimension is the batch.
  const auto& output_metadata = output_itr->value[oidx];
  const std::string& output_name = std::string(
    output_metadata["name"].GetString(),
    output_metadata["name"].GetStringLength());

  const auto output_dtype_itr = output_metadata.FindMember("datatype");
  if (output_dtype_itr == output_metadata.MemberEnd()) {
    triton::client::Error err("expecting datatype for model output '" + output_name + "'");
    LogTritonError(err, "unsupported model metadata");
    return err;
  }

  if (std::string(output_dtype_itr->value.GetString(), output_dtype_itr->value.GetStringLength()) != "FP32") {
    triton::client::Error err("expecting datatype FP32 for model output '" + output_name + "'");
    LogTritonError(err, "unsupported model metadata");
    return err;
  }

  const auto output_shape_itr = output_metadata.FindMember("shape");
  if (output_shape_itr == output_metadata.MemberEnd()) {
    triton::client::Error err("expecting shape for model output '" + output_name + "'");
    LogTritonError(err, "unsupported model metadata");
    return err;
  }

  const rapidjson::Value& output_shape_json = output_shape_itr->value;
  if (output_shape_json.Size() != 2) {
    triton::client::Error err("expecting rank 2 shape for model output '" + output_name + "'");
    LogTritonError(err, "unsupported model metadata");
    return err;
  }

  if (output_shape_json[0].GetInt() != -1) {
    triton::client::Error err("expecting batch dimension for model output '" + output_name + "'");
    LogTritonError(err, "unsupported model metadata");
    return err;
  }

  // If the non-batch shape is 1, then it is the 'v' output, otherwise
  // it is the 'pi' output. Assumption here is that there will be more
  // than one action so there is no ambiguity.
  if (output_shape_json[1].GetInt() == 1) {
    assert(metadata->output_v_name_.empty());
    metadata->output_v_name_ = std::move(output_name);
  }
  else {
    assert(metadata->output_pi_name_.empty());
    metadata->output_pi_name_ = std::move(output_name);
    metadata->output_pi_shape_.push_back(output_shape_json[1].GetInt());
  }

  return triton::client::Error::Success;
}

void
Triton::CheckTritonError(const triton::client::Error& err, const std::string& msg)
{
  if (!err.IsOk()) {
    std::stringstream sstr;
    sstr << "triton error: " << err.Message();
    THROW(sstr.str());
  }
}

void
Triton::LogTritonError(const triton::client::Error& err, const std::string& msg)
{
  if (!err.IsOk()) {
    std::stringstream sstr;
    sstr << "triton error: " << err.Message();
    logger_->Log(sstr.str());
  }
}

} // namespace cifre
