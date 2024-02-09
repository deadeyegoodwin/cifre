// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <string>

#include "common_logger.h"
#include "tritonserver.h"

namespace cifre {

//
// Triton
//
// Interface to Triton inference server.
//
class Triton {
public:
  Triton(common::Logger* logger, const std::string& repository_path);
  virtual ~Triton();

private:
  void CheckTritonError(TRITONSERVER_Error* err, const std::string& msg);
  void LogTritonError(TRITONSERVER_Error* err, const std::string& msg);

  common::Logger* logger_;
  TRITONSERVER_Server* server_;
  TRITONSERVER_ServerOptions* options_;
};

} // namespace cifre
