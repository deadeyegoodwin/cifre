// Copyright 2024, David Goodwin. All rights reserved.

#include <sstream>

#include "cifre_triton.h"
#include "common_error.h"

namespace cifre {

Triton::Triton(common::Logger* logger, const std::string& repository_path)
{
  logger_ = logger;

  CheckTritonError(
    TRITONSERVER_ServerOptionsNew(&options_),
    "initializing triton options");

  CheckTritonError(
    TRITONSERVER_ServerNew(&server_, options_),
    "initializing triton");
}

Triton::~Triton()
{
  LogTritonError(
    TRITONSERVER_ServerStop(server_),
    "stopping trition");

  LogTritonError(
    TRITONSERVER_ServerOptionsDelete(options_),
    "deleting triton options");
}

void
Triton::CheckTritonError(TRITONSERVER_Error* err, const std::string& msg)
{
  if (err != nullptr) {
    std::stringstream sstr;
    sstr << "error: " << msg << ": "
         << TRITONSERVER_ErrorCodeString(err) << " - "
         << TRITONSERVER_ErrorMessage(err);
    TRITONSERVER_ErrorDelete(err);
    THROWIF(false, sstr.str());
  }
}

void
Triton::LogTritonError(TRITONSERVER_Error* err, const std::string& msg)
{
  if (err != nullptr) {
    std::stringstream sstr;
    sstr << "error: " << msg << ": "
         << TRITONSERVER_ErrorCodeString(err) << " - "
         << TRITONSERVER_ErrorMessage(err);
    TRITONSERVER_ErrorDelete(err);
    logger_->LogLow(sstr.str());
  }
}

} // namespace cifre
