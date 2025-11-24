// Copyright 2025, David Goodwin. All rights reserved.

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>

#include "gettysburg_board.h"
#include "gettysburg_goal.h"
#include "common_error.h"
#include "common_util.h"

namespace cifre { namespace gettysburg {

//
// Goal
//
Goal::Goal(size_t row_idx, size_t col_idx) :
  row_idx_(row_idx), column_idx_(col_idx)
{
}

size_t
Goal::Hash() const
{
  constexpr uint64_t mulp = 2654435789;
  uint64_t hash = 104395301;

  hash += (row_idx_ * mulp) ^ (hash >> 23);
  hash += (column_idx_ * mulp) ^ (hash >> 23);

  return hash ^ (hash << 37);
}

bool
Goal::Equal(const Goal* other) const
{
  return ((row_idx_ == other->row_idx_) &&
          (column_idx_ == other->column_idx_));
}

void
Goal::ClearFromBoard(Board* board) const
{
  board->SetGoalAt(BoardCellFactory::CELL_EMPTY, row_idx_, column_idx_);
}

void
Goal::AddToBoard(Board* board) const
{
  board->SetGoalAt(BoardCellFactory::CELL_ACTIVE, row_idx_, column_idx_);
}

}} // namespace cifre::gettysburg
