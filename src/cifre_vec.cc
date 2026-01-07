// Copyright 2025, David Goodwin. All rights reserved.

#include <algorithm>
#include <regex>

#include "cifre_vec.h"
#include "common_error.h"

namespace cifre {

//
// Vector2
//
Vector2::Vector2(const std::string& str)
{
  static std::regex regex_square("^([1-9][0-9]*)$");
  static std::regex regex_rectangle("^([1-9][0-9]*)[x,]([1-9][0-9]*)$");

  std::smatch m;
  if (!std::regex_match(str, m, regex_rectangle)) {
    THROWIF(!std::regex_match(str, m, regex_square),
            "size must be specified as 'NxM' or 'N,M'");
    row_cnt_ = column_cnt_ = std::stoi(m[1]);
  }
  else {
    row_cnt_ = std::stoi(m[1]);
    column_cnt_ = std::stoi(m[2]);
  }
}

std::string
Vector2::ToString() const
{
  return std::to_string(row_cnt_) + "x" + std::to_string(column_cnt_);
}

} // namespace cifre
