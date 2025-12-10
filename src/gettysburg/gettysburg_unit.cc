// Copyright 2025, David Goodwin. All rights reserved.

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
Unit::Unit(const std::string& name, ArmyKind army_kind, UnitKind unit_kind,
           float combat_factor, float move_factor, size_t row_idx, size_t col_idx) :
  name_(name), army_kind_(army_kind), unit_kind_(unit_kind),
  combat_factor_(combat_factor), move_factor_(move_factor),
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
Unit::RemoveFromBoard(Board* board) const
{
  switch (army_kind_) {
  case ArmyKind::UNION: {
    uint32_t uc = uint32_t(board->UnionAt(row_idx_, column_idx_));
    assert(uc >= 1);
    board->SetUnionAt(uc - 1, row_idx_, column_idx_);
    break;
  }
  case ArmyKind::CONFEDERATE: {
    uint32_t cc = uint32_t(board->ConfederateAt(row_idx_, column_idx_));
    assert(cc >= 1);
    board->SetConfederateAt(cc - 1, row_idx_, column_idx_);
    break;
  }}
}

void
Unit::AddToBoard(Board* board) const
{
  switch (army_kind_) {
  case ArmyKind::UNION:
    board->SetUnionAt(board->UnionAt(row_idx_, column_idx_) + 1.0, row_idx_, column_idx_);
    break;
  case ArmyKind::CONFEDERATE:
    board->SetConfederateAt(board->ConfederateAt(row_idx_, column_idx_) + 1.0, row_idx_, column_idx_);
    break;
  }
}

void
Unit::ApplyAction(ActionKind kind)
{
  if (kind != ActionKind::MOVE_END) {
    switch (kind) {
    case ActionKind::MOVE_UP:
      assert(row_idx_ > 0);
      row_idx_ -= 1;
      break;
    case ActionKind::MOVE_DOWN:
      row_idx_ += 1;
      break;
    case ActionKind::MOVE_UPRIGHT:
      if ((column_idx_ % 2) == 0) {
        assert(row_idx_ > 0);
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
      assert(column_idx_ > 0);
      column_idx_ -= 1;
      break;
    case ActionKind::MOVE_UPLEFT:
      if ((column_idx_ % 2) == 0) {
        assert(row_idx_ > 0);
        row_idx_ -= 1;
      }
      assert(column_idx_ > 0);
      column_idx_ -= 1;
      break;
    case ActionKind::MOVE_END:
    case ActionKind::ACTION_COUNT:
      THROW("unexpected unit action");
    }
  }
}

std::string
Unit::ToString() const
{
  std::stringstream sstr;

  sstr
    << UnitKindToString(unit_kind_) << " (" << name_ << ") "
    << combat_factor_ << "-" << move_factor_;

  return sstr.str();
}

}} // namespace cifre::gettysburg
