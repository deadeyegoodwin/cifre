// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <string>

#include "common_logger.h"
#include "http_client.h"

namespace cifre {

//
// Triton
//
// Interface to Triton inference server.
//
class Triton {
public:
  Triton(common::Logger* logger, const std::string& workdir);
  virtual ~Triton();

private:
  void CheckTritonError(const triton::client::Error& err, const std::string& msg);
  void LogTritonError(const triton::client::Error& err, const std::string& msg);

  common::Logger* logger_;
  const std::string workdir_;
};

} // namespace cifre
