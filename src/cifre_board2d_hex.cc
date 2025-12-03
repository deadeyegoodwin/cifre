// Copyright 2025, David Goodwin. All rights reserved.

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <regex>
#include <string>
#include <sstream>

#include "cifre_board2d_hex.h"
#include "common_error.h"

namespace cifre {

//
// Board2DHex
//
Board2DHex::Board2DHex(
  const size_t plane_cnt, const cifre::BoardSize2D& size,
  const std::vector<Player*>& players, size_t starting_player_id) :
  Board2D(plane_cnt, size, players, starting_player_id)
{
  THROWIF(players_.size() != PLAYER_COUNT, "exactly 2 players are required");
  THROWIF(NoPlayerId() != PLAYER_COUNT, "exactly 2 players are required");
}

std::string
Board2DHex::ToString() const
{
  std::stringstream sstr;

  // Player key.
  sstr
    << " active(p" << ActivePlayerId() << ")=" << ActivePlayer()->Name()
    << "(" << ActivePlayer()->Char() << "), "
    << "opponent(p" << OpponentPlayerId() << ")=" << OpponentPlayer()->Name()
    << "(" << OpponentPlayer()->Char() << ")"
    << "\n";

  //  __    __    __    __
  // /0 \__/0 \__/0 \__/0 \_
  // \_0/0 \_2/0 \_4/0 \_6/
  // /1 \_1/1 \_3/1 \_5/1 \_
  // \_0/1 \_2/1 \_4/1 \_6/
  // /2 \_1/2 \_3/2 \_5/2 \_
  // \_0/2 \_2/2 \_4/2 \_6/
  // /  \_1/  \_3/  \_5/  \_

  // Top of first row of hex cells.
  for (size_t col_idx = 0; col_idx < size_.ColumnCount(); col_idx += 2) {
    if (col_idx == 0) {
      sstr << " __";
    }
    else {
      sstr << "    __";
    }
  }
  sstr << "\n";

  // Rows...
  for (size_t row_idx = 0; row_idx < size_.RowCount(); ++row_idx) {
    for (size_t col_idx = 0; col_idx < size_.ColumnCount(); ++col_idx) {
      if ((col_idx % 2) == 0) {
        sstr << "/" << BoardCellUpperString(row_idx, col_idx) << "\\";
      }
      else if (row_idx > 0) {
        sstr << BoardCellLowerString(row_idx - 1, col_idx);
      }
      else {
        sstr << "__";
      }
    }
    if ((row_idx != 0) && ((size_.ColumnCount() % 2) == 0)) {
      sstr << "/";
    }
    sstr << "\n";

    for (size_t col_idx = 0; col_idx < size_.ColumnCount(); ++col_idx) {
      if ((col_idx % 2) == 0) {
        sstr << "\\" << BoardCellLowerString(row_idx, col_idx) << "/";
      }
      else {
        sstr << BoardCellUpperString(row_idx, col_idx);
      }
    }
    if ((size_.ColumnCount() % 2) == 0) {
      sstr << "\\";
    }
    sstr << "\n";
  }

  // Lower half of last row.
  sstr << " ";
  for (size_t col_idx = 0; col_idx < size_.ColumnCount(); ++col_idx) {
    if ((col_idx % 2) == 1) {
      sstr << "  \\" << BoardCellLowerString(size_.RowCount() - 1, col_idx) << "/";
    }
  }
  sstr << "\n";

  return sstr.str();
}

std::string
Board2DHex::BoardCellUpperString(size_t row_idx, size_t col_idx) const
{
  std::string str = std::to_string(row_idx);
  if (str.size() == 1) {
    str += " ";
  }
  return str;
}

std::string
Board2DHex::BoardCellLowerString(size_t row_idx, size_t col_idx) const
{
  std::string str = std::to_string(col_idx);
  if (str.size() == 1) {
    str = "_" + str;
  }
  return str;
}

} // namespace cifre
