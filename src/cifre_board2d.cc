// Copyright 2024, David Goodwin. All rights reserved.

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <regex>
#include <string>
#include <sstream>

#include "cifre_board2d.h"

namespace cifre {

//
// BoardSize2D
//
BoardSize2D::BoardSize2D(const std::string& str)
{
  static std::regex regex_square("^([1-9][0-9]*)$");
  static std::regex regex_rectangle("^([1-9][0-9]*)x([1-9][0-9]*)$");

  std::smatch m;
  if (!std::regex_match(str, m, regex_rectangle)) {
    THROWIF(!std::regex_match(str, m, regex_square),
            "board size must be specified as 'NxM'");
    row_cnt_ = column_cnt_ = std::stoi(m[1]);
  }
  else {
    row_cnt_ = std::stoi(m[1]);
    column_cnt_ = std::stoi(m[2]);
  }
}

std::string
BoardSize2D::ToString() const
{
  return std::to_string(row_cnt_) + "x" + std::to_string(column_cnt_);
}

} // namespace cifre
