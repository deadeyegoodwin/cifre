// Copyright 2024, David Goodwin. All rights reserved.

#include <sstream>

#include "cifre_triton.h"
#include "common_error.h"

namespace cifre {

Triton::Triton(common::Logger* logger, const std::string& workdir) :
  logger_(logger), workdir_(workdir)
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

void
Triton::CheckTritonError(const triton::client::Error& err, const std::string& msg)
{
  if (!err.IsOk()) {
    std::stringstream sstr;
    sstr << "triton error: " << err.Message();
    THROWIF(false, sstr.str());
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
