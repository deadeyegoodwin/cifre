// Copyright 2024, David Goodwin. All rights reserved.

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <regex>
#include <string>
#include <sstream>

#include "celcath_board.h"
#include "common_error.h"
#include "common_util.h"

namespace celcath {

//
// BoardSize
//
BoardSize::BoardSize(const std::string& str)
{
  static std::regex regex_square("^([1-9][0-9]*)$");
  static std::regex regex_rectangle("^([1-9][0-9]*)x([1-9][0-9]*)$");

  std::smatch m;
  if (!std::regex_match(str, m, regex_rectangle)) {
    THROWIF(!std::regex_match(str, m, regex_square),
            "board size must be specified as 'NxM'");
    row_cnt_ = column_cnt_ = std::stoi(m[1]);
  } else {
    row_cnt_ = std::stoi(m[1]);
    column_cnt_ = std::stoi(m[2]);
  }
}

const BoardSize&
BoardSize::MaxSize()
{
  // Currently board logic assumes 1 digit for row name, so size
  // limited to 9.
  static const BoardSize max{9, 9};
  return max;
}

std::string
BoardSize::ToString() const
{
  return std::to_string(row_cnt_) + "x" + std::to_string(column_cnt_);
}

//
// Board
//
Board::Board(BoardSize size) :
  size_(size), size_with_border_(size.RowCount() + 2, size.ColumnCount() + 2)
{
  auto max_size = BoardSize::MaxSize();
  THROWIF(size.RowCount() > max_size.RowCount(),
          "Maximum number of board rows is " + std::to_string(max_size.RowCount()));
  THROWIF(size.ColumnCount() > max_size.ColumnCount(),
          "Maximum number of board columns is " + std::to_string(max_size.ColumnCount()));

  cells_.resize(size_with_border_.RowCount(), std::vector<Cell>(size_with_border_.ColumnCount()));

  // Default to a LIVE border.
  SetBorder(BorderKind::LIVE);
}

Board::BorderKind
Board::StringToBorderKind(const std::string& name)
{
  std::string lname(name);
  common::ToLower(lname);
  if (lname == "dead") {
    return BorderKind::DEAD;
  }
  else if (name == "live") {
    return BorderKind::LIVE;
  }

  return BorderKind::INVALID;
}

void
Board::SetBorder(Board::BorderKind kind)
{
  // For LIVE and DEAD set every border cell to Player::LIVE or
  // Player::DEAD respectively.
  // Board with each row number on the left.
  if ((kind == BorderKind::LIVE) || (kind == BorderKind::DEAD)) {
    Player* player = (kind == BorderKind::LIVE) ? &Player::LIVE : &Player::DEAD;
    size_t row_idx = 0;
    for (auto& r : cells_) {
      size_t col_idx = 0;
      for (auto& c : r) {
        if ((row_idx == 0) || (row_idx == (size_with_border_.RowCount() - 1)) ||
            (col_idx == 0) || (col_idx == (size_with_border_.ColumnCount() - 1))) {
          c.SetOwner(player);
        }
        col_idx++;
      }

      row_idx++;
    }
  }
}

Cell*
Board::CellAt(size_t row_idx, size_t col_idx)
{
  if ((row_idx >= size_.RowCount()) || (col_idx >= size_.ColumnCount())) {
    return nullptr;
  }

  return &cells_[row_idx + 1][col_idx + 1];
}

Cell*
Board::CellAtWithBorder(size_t row_idx, size_t col_idx)
{
  if ((row_idx >= size_with_border_.RowCount()) ||
      (col_idx >= size_with_border_.ColumnCount())) {
    return nullptr;
  }

  return &cells_[row_idx][col_idx];
}

Cell*
Board::StringToCell(const std::string& str)
{
  std::string posstr(str);
  common::Trim(posstr);

  // String must be 2 characters, a column letter ('a' based) followed
  // by a row number (1 based).
  if (posstr.size() == 2) {
    int32_t col_idx = posstr[0] - 'a';
    int32_t row_idx = posstr[1] - '1';
    if ((col_idx >= 0) && ((size_t)col_idx < size_.ColumnCount()) &&
        (row_idx >= 0) && ((size_t)row_idx < size_.RowCount())) {
      return CellAt(row_idx, col_idx);
    }
  }

  return nullptr;
}

Cell*
Board::StringToCellWithBorder(const std::string& str)
{
  std::string posstr(str);
  common::Trim(posstr);

  // String must be 2 characters, a column letter ('a' based) followed
  // by a row number (1 based).
  if (posstr.size() == 2) {
    int32_t col_idx = posstr[0] - 'a';
    int32_t row_idx = posstr[1] - '1';
    if ((col_idx >= 0) && ((size_t)col_idx < size_with_border_.ColumnCount()) &&
        (row_idx >= 0) && ((size_t)row_idx < size_with_border_.RowCount())) {
      return CellAtWithBorder(row_idx, col_idx);
    }
  }

  return nullptr;
}

std::string
Board::ToString(common::Logger::Level filter_level) const
{
  std::stringstream sstr;

  // Column letters across the top.
  for (size_t col_idx = 0; col_idx < (size_with_border_.ColumnCount() - 1); ++col_idx) {
    if (col_idx == 0) {
      sstr << "       ";
    } else {
      sstr << char('a' + col_idx - 1) << ' ';
    }
  }
  sstr << "\n    " << std::string(size_with_border_.ColumnCount() * 2, '-') << '\n';

  // Board with each row number on the left.
  size_t row_idx = 0;
  for (const auto& r : cells_) {
    if (row_idx == 0) {
      sstr << "   ";
    } else {
      sstr << '\n';
      if (row_idx < (size_.RowCount() + 1)) {
        sstr << ' ' << row_idx << ' ';
      } else {
        sstr << "   ";
      }
    }

    size_t col_idx = 0;
    for (const auto& c : r) {
      if (col_idx == 0) {
        sstr << '|';
      }
      sstr << ' ' << c.ToString(filter_level);
      col_idx++;
    }

    row_idx++;
  }

  return sstr.str();
}

//
// Cell
//
std::string
Cell::ToString(common::Logger::Level filter_level) const
{
  char bchar = ' ';
  if (owner_ != nullptr) {
    bchar = owner_->BoardCharacter();
  }

  std::string str{bchar};
  return str;
}


} // celcath
