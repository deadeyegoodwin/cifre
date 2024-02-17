// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "celcath_player.h"
#include "common_logger.h"

namespace cifre { namespace celcath {

class BoardSize;
class Board;
class Cell;

//
// BoardSize
//
// Board size in rows and columns.
//
class BoardSize {
public:
  // The maximum size allowed for a board.
  static const BoardSize& MaxSize();

public:
  // A square board size with 'cnt' rows and columns.
  BoardSize(size_t cnt) : BoardSize(cnt, cnt) {}

  // A rectangular board size with 'rcnt' rows and 'ccnt' columns.
  BoardSize(size_t rcnt, size_t ccnt) : row_cnt_(rcnt), column_cnt_(ccnt) {}

  // Create a BoardSize by parsing a string.
  BoardSize(const std::string& str);

  // Return the number of rows in the board size.
  size_t RowCount() const { return row_cnt_; }

  // Return the number of columns in the board size.
  size_t ColumnCount() const { return column_cnt_; }

  // Return the board size as a std::pair { row-count, column-count }.
  std::pair<size_t, size_t> Size() const { return { row_cnt_, column_cnt_ }; }

  // Return string representation of the board size.
  std::string ToString() const;

private:
  size_t row_cnt_;
  size_t column_cnt_;
};

//
// Board
//
// The playing board.
//
class Board {
public:
  // The kinds of borders that can be used on the board.
  enum class BorderKind { INVALID, DEAD, LIVE };

  // Convert string to a BorderKind.
  static BorderKind StringToBorderKind(const std::string& name);

public:
  // Create a board of 'size' (not including border cells).
  Board(common::Logger* logger, BoardSize size);

  // The size of the board, not including border cells.
  const BoardSize& Size() const { return size_; }

  // The size of the board, including border cells.
  const BoardSize& SizeWithBorder() const { return size_with_border_; }

  // Set the kind of border to use for the board.
  void SetBorder(Board::BorderKind kind);

  // Return the cell at the given row and column index, not including
  // border cells. Return nullptr if row or column index are invalid.
  Cell* CellAt(size_t row_idx, size_t col_idx);

  // Return the cell at the given row and column index, including the
  // border cells. Thus, 'row_idx' == 0 and 'row_idx' == (Size() + 1)
  // are border rows, and 'col_idx' == 0 and 'col_idx' == (Size() + 1)
  // are border columns. Return nullptr if row or column index are
  // invalid.
  Cell* CellAtWithBorder(size_t row_idx, size_t col_idx);

  // Return the cell specifed by 'str', or return nullptr if 'str' is
  // an invalid cell specification. Border cells are not included.
  Cell* StringToCell(const std::string& str);

  // Return the cell specifed by 'str', or return nullptr if 'str' is
  // an invalid cell specification. Border cells are included.
  Cell* StringToCellWithBorder(const std::string& str);

  // Return string representation of the board.
  std::string ToString(common::Logger::Level filter_level = common::Logger::Level::LOW) const;

private:
  const BoardSize size_;
  const BoardSize size_with_border_;
  Player always_dead_player_;
  Player always_live_player_;

  // 'cells_' includes border cells...
  std::vector<std::vector<Cell>> cells_;
};

//
// Cell
//
// A cell on the board.
//
class Cell {
public:
  Cell() : owner_(nullptr) { }

  // The Player that owns the cell, or nullptr if no owner
  // (i.e. empty).
  Player* Owner() const { return owner_; }

  // Set the Player that owns the cell, or nullptr if no owner
  // (i.e. empty).
  void SetOwner(Player* owner) { owner_ = owner; }

  // Return string representation of the contents of the cell.
  std::string ToString(common::Logger::Level filter_level = common::Logger::Level::LOW) const;

private:
  Player* owner_;
};

}} // namespace cifre::celcath
