// Copyright 2025, David Goodwin. All rights reserved.

#include <algorithm>
#include <limits>
#include <regex>

#include "cifre_vec.h"
#include "common_error.h"

namespace cifre {

//
// MatrixSize
//
MatrixSize::MatrixSize(const std::string& str)
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
MatrixSize::ToString() const
{
  return std::to_string(row_cnt_) + "x" + std::to_string(column_cnt_);
}

//
// MatrixCoordinate
//
MatrixCoordinate::MatrixCoordinate(const std::string& str)
{
  static std::regex regex_coord("^([1-9][0-9]*),([1-9][0-9]*)$");

  std::smatch m;
  if (!std::regex_match(str, m, regex_coord)) {
    THROW("coordinate must be specified as 'N,M'");
  }
  else {
    row_ = std::stoi(m[1]);
    column_ = std::stoi(m[2]);
  }
}

std::string
MatrixCoordinate::ToString() const
{
  return std::to_string(row_) + "," + std::to_string(column_);
}

} // namespace cifre
