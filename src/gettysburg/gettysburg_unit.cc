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
Unit::Unit(ArmyKind army_kind, size_t row_idx, size_t col_idx) :
  army_kind_(army_kind), row_idx_(row_idx), column_idx_(col_idx)
{
}

void
Unit::ClearFromBoard(Board* board) const
{
  switch (army_kind_) {
  case ArmyKind::UNION:
    board->SetUnionAt(Board::CELL_EMPTY, row_idx_, column_idx_);
    break;
  case ArmyKind::CONFEDERATE:
    board->SetConfederateAt(Board::CELL_EMPTY, row_idx_, column_idx_);
    break;
  }
}

void
Unit::AddToBoard(Board* board) const
{
  switch (army_kind_) {
  case ArmyKind::UNION:
    board->SetUnionAt(Board::CELL_ACTIVE, row_idx_, column_idx_);
    break;
  case ArmyKind::CONFEDERATE:
    board->SetConfederateAt(Board::CELL_ACTIVE, row_idx_, column_idx_);
    break;
  }
}

}} // namespace cifre::gettysburg
