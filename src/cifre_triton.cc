// Copyright 2024, David Goodwin. All rights reserved.

#include <sstream>

#include "cifre_triton.h"
#include "common_error.h"

namespace cifre {

Triton::Triton(common::Logger* logger, const std::string& workdir) :
  logger_(logger), workdir_(workdir)
{
}

Triton::~Triton()
{
}

void
Triton::CheckTritonError(const triton::client::Error& err, const std::string& msg)
{
  if (!err.IsOk()) {
    std::stringstream sstr;
    sstr << "Triton error: " << err.Message();
    THROWIF(false, sstr.str());
  }
}

void
Triton::LogTritonError(const triton::client::Error& err, const std::string& msg)
{
  if (!err.IsOk()) {
    std::stringstream sstr;
    sstr << "Triton error: " << err.Message();
    logger_->LogLow(sstr.str());
  }
}

} // namespace cifre
