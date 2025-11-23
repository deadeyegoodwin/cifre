// Copyright 2024, David Goodwin. All rights reserved.

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>

#include "gettysburg_board.h"
#include "gettysburg_unit.h"
#include "common_error.h"
#include "common_util.h"

namespace cifre { namespace gettysburg {

//
// Unit
//
Unit::Unit(ArmyKind army_kind, float move_factor, size_t row_idx, size_t col_idx) :
  army_kind_(army_kind), move_factor_(move_factor),
  row_idx_(row_idx), column_idx_(col_idx)
{
}

size_t
Unit::Hash() const
{
  constexpr uint64_t mulp = 2654435789;
  uint64_t hash = 104395301;

  hash += (uint64_t(army_kind_) * mulp) ^ (hash >> 23);
  hash += (uint64_t(move_factor_ * 100000.0) * mulp) ^ (hash >> 23);
  hash += (row_idx_ * mulp) ^ (hash >> 23);
  hash += (column_idx_ * mulp) ^ (hash >> 23);

  return hash ^ (hash << 37);
}

bool
Unit::Equal(const Unit* other) const
{
  return ((army_kind_ == other->army_kind_) &&
          (move_factor_ == other->move_factor_) &&
          (row_idx_ == other->row_idx_) &&
          (column_idx_ == other->column_idx_));
}

void
Unit::ClearFromBoard(Board* board) const
{
  switch (army_kind_) {
  case ArmyKind::UNION:
    board->SetUnionAt(BoardCellFactory::CELL_EMPTY, row_idx_, column_idx_);
    break;
  case ArmyKind::CONFEDERATE:
    board->SetConfederateAt(BoardCellFactory::CELL_EMPTY, row_idx_, column_idx_);
    break;
  }
}

void
Unit::AddToBoard(Board* board) const
{
  switch (army_kind_) {
  case ArmyKind::UNION:
    board->SetUnionAt(BoardCellFactory::CELL_ACTIVE, row_idx_, column_idx_);
    break;
  case ArmyKind::CONFEDERATE:
    board->SetConfederateAt(BoardCellFactory::CELL_ACTIVE, row_idx_, column_idx_);
    break;
  }
}

void
Unit::ApplyAction(ActionKind kind, Board* board)
{
  if (kind != ActionKind::MOVE_END) {
    ClearFromBoard(board);

    switch (kind) {
    case ActionKind::MOVE_UP:
      row_idx_ -= 1;
      break;
    case ActionKind::MOVE_DOWN:
      row_idx_ += 1;
      break;
    case ActionKind::MOVE_UPRIGHT:
      if ((column_idx_ % 2) == 0) {
        row_idx_ -= 1;
      }
      column_idx_ += 1;
      break;
    case ActionKind::MOVE_DOWNRIGHT:
      if ((column_idx_ % 2) == 1) {
        row_idx_ += 1;
      }
      column_idx_ += 1;
      break;
    case ActionKind::MOVE_DOWNLEFT:
      if ((column_idx_ % 2) == 1) {
        row_idx_ += 1;
      }
      column_idx_ -= 1;
      break;
    case ActionKind::MOVE_UPLEFT:
      if ((column_idx_ % 2) == 0) {
        row_idx_ -= 1;
      }
      column_idx_ -= 1;
      break;
    case ActionKind::MOVE_END:
    case ActionKind::ACTION_COUNT:
      THROW("unexpected unit action");
    }

    AddToBoard(board);
  }
}

}} // namespace cifre::gettysburg
