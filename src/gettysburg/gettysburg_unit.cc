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
  plane_idx_(UnitPlane(army_kind, unit_kind, combat_factor, move_factor)),
  move_factor_expended_(0.0), row_idx_(row_idx), column_idx_(col_idx)
{
}

size_t
Unit::UnitPlane(
  ArmyKind army_kind, UnitKind unit_kind, float combat_factor, float move_factor) const
{
  switch (army_kind) {
  case ArmyKind::UNION: {
    switch (unit_kind) {
    case UnitKind::HEADQUARTERS:
      return BoardCellFactory::PLANE_IDX_UNION_HEADQUARTERS;
    case UnitKind::INFANTRY:
      if (move_factor == 5) {
        if (combat_factor == 1) return BoardCellFactory::PLANE_IDX_UNION_INFANTRY_1_5;
        if (combat_factor == 2) return BoardCellFactory::PLANE_IDX_UNION_INFANTRY_2_5;
        if (combat_factor == 3) return BoardCellFactory::PLANE_IDX_UNION_INFANTRY_3_5;
        if (combat_factor == 4) return BoardCellFactory::PLANE_IDX_UNION_INFANTRY_4_5;
      }
      break;
    case UnitKind::CAVALRY:
      if (move_factor == 7) {
        if (combat_factor == 1) return BoardCellFactory::PLANE_IDX_UNION_CAVALRY_1_7;
        if (combat_factor == 2) return BoardCellFactory::PLANE_IDX_UNION_CAVALRY_2_7;
      }
      break;
    case UnitKind::ARTILLERY:
    case UnitKind::HORSE_ARTILLERY:
      if (move_factor == 5) {
        if (combat_factor == 1) return BoardCellFactory::PLANE_IDX_UNION_ARTILLERY_1_5;
        if (combat_factor == 2) return BoardCellFactory::PLANE_IDX_UNION_ARTILLERY_2_5;
      }
      else if (move_factor == 7) {
        if (combat_factor == 1) return BoardCellFactory::PLANE_IDX_UNION_ARTILLERY_1_7;
        if (combat_factor == 2) return BoardCellFactory::PLANE_IDX_UNION_ARTILLERY_2_7;
      }
      break;
    }
    break;
  }

  case ArmyKind::CONFEDERATE: {
    switch (unit_kind) {
    case UnitKind::HEADQUARTERS:
      return BoardCellFactory::PLANE_IDX_CONFEDERATE_HEADQUARTERS;
    case UnitKind::INFANTRY:
      if (move_factor == 5) {
        if (combat_factor == 2) return BoardCellFactory::PLANE_IDX_CONFEDERATE_INFANTRY_2_5;
        if (combat_factor == 3) return BoardCellFactory::PLANE_IDX_CONFEDERATE_INFANTRY_3_5;
        if (combat_factor == 4) return BoardCellFactory::PLANE_IDX_CONFEDERATE_INFANTRY_4_5;
        if (combat_factor == 5) return BoardCellFactory::PLANE_IDX_CONFEDERATE_INFANTRY_5_5;
        if (combat_factor == 6) return BoardCellFactory::PLANE_IDX_CONFEDERATE_INFANTRY_6_5;
      }
      break;
    case UnitKind::CAVALRY:
      if (move_factor == 7) {
        if (combat_factor == 1) return BoardCellFactory::PLANE_IDX_CONFEDERATE_CAVALRY_1_7;
      }
      break;
    case UnitKind::ARTILLERY:
    case UnitKind::HORSE_ARTILLERY:
      if (move_factor == 5) {
        if (combat_factor == 1) return BoardCellFactory::PLANE_IDX_CONFEDERATE_ARTILLERY_1_5;
        if (combat_factor == 2) return BoardCellFactory::PLANE_IDX_CONFEDERATE_ARTILLERY_2_5;
      }
      else if (move_factor == 7) {
        if (combat_factor == 1) return BoardCellFactory::PLANE_IDX_CONFEDERATE_ARTILLERY_1_7;
      }
      break;
    }
    break;
  }}

  THROW("failed to find plane for unit");
  return 0; // not reached
}

size_t
Unit::Hash() const
{
  constexpr uint64_t mulp = 2654435789;
  uint64_t hash = 104395301;

  // Don't hash 'name_', it is just descriptive...
  //
  // Don't hash 'army_kind_, 'unit_kind_', 'combat_factor_' or
  // 'move_factor_' as they are redundant with 'plane_idx_'.

  hash += (plane_idx_ * mulp) ^ (hash >> 23);
  hash += (uint64_t(move_factor_expended_ * 100000.0) * mulp) ^ (hash >> 23);
  hash += (row_idx_ * mulp) ^ (hash >> 23);
  hash += (column_idx_ * mulp) ^ (hash >> 23);

  return hash ^ (hash << 37);
}

bool
Unit::Equal(const Unit* other) const
{
  // Don't compare 'name_', it is just descriptive...
  //
  // Don't compare 'army_kind_, 'unit_kind_', 'combat_factor_' or
  // 'move_factor_' as they are redundant with 'plane_idx_'.

  return ((plane_idx_ == other->plane_idx_) &&
          (move_factor_expended_ == other->move_factor_expended_) &&
          (row_idx_ == other->row_idx_) &&
          (column_idx_ == other->column_idx_));
}

void
Unit::RemoveFromBoard(Board* board) const
{
  float v = board->BoardCellValueAt(row_idx_, column_idx_, plane_idx_);
  assert(v >= 1.0);
  board->SetBoardCellValueAt(v - 1.0, row_idx_, column_idx_, plane_idx_);
}

void
Unit::AddToBoard(Board* board) const
{
  float v = board->BoardCellValueAt(row_idx_, column_idx_, plane_idx_);
  board->SetBoardCellValueAt(v + 1.0, row_idx_, column_idx_, plane_idx_);
}

void
Unit::ApplyAction(
  ActionKind kind, const std::array<float, Board2DHex::DIRECTION_COUNT>& move_cost)
{
  switch (kind) {
  case ActionKind::MOVE_UP:
    assert(row_idx_ > 0);
    row_idx_ -= 1;
    move_factor_expended_ += move_cost[size_t(Board2DHex::Direction::UP)];
    break;
  case ActionKind::MOVE_DOWN:
    row_idx_ += 1;
    move_factor_expended_ += move_cost[size_t(Board2DHex::Direction::DN)];
    break;
  case ActionKind::MOVE_UPRIGHT:
    if ((column_idx_ % 2) == 0) {
      assert(row_idx_ > 0);
      row_idx_ -= 1;
    }
    column_idx_ += 1;
    move_factor_expended_ += move_cost[size_t(Board2DHex::Direction::UP_RT)];
    break;
  case ActionKind::MOVE_DOWNRIGHT:
    if ((column_idx_ % 2) == 1) {
      row_idx_ += 1;
    }
    column_idx_ += 1;
    move_factor_expended_ += move_cost[size_t(Board2DHex::Direction::DN_RT)];
    break;
  case ActionKind::MOVE_DOWNLEFT:
    if ((column_idx_ % 2) == 1) {
      row_idx_ += 1;
    }
    assert(column_idx_ > 0);
    column_idx_ -= 1;
    move_factor_expended_ += move_cost[size_t(Board2DHex::Direction::DN_LT)];
    break;
  case ActionKind::MOVE_UPLEFT:
    if ((column_idx_ % 2) == 0) {
      assert(row_idx_ > 0);
      row_idx_ -= 1;
    }
    assert(column_idx_ > 0);
    column_idx_ -= 1;
    move_factor_expended_ += move_cost[size_t(Board2DHex::Direction::UP_LT)];
    break;
  case ActionKind::MOVE_END:
    move_factor_expended_ = move_factor_;
    break;
  case ActionKind::ACTION_COUNT:
    THROW("unexpected unit action");
  }

  assert(move_factor_expended_ <= move_factor_);
}

Board2DHex::Direction
Unit::AdjacentDirection(const Unit& unit) const
{
  const int64_t row_delta = unit.row_idx_ - row_idx_;
  const int64_t col_delta = unit.column_idx_ - column_idx_;
  if (((row_delta == 0) && (col_delta == 0)) ||
      (abs(row_delta) > 1) || (abs(col_delta) > 1)) {
    return Board2DHex::Direction::NONE;
  }

  if (col_delta == 0) {
    return (row_delta == 1) ? Board2DHex::Direction::DN : Board2DHex::Direction::UP;
  }

  if ((column_idx_ % 2) == 0) {
    if (row_delta == 0) {
      return (col_delta == 1) ? Board2DHex::Direction::DN_RT : Board2DHex::Direction::DN_LT;
    }
    else if (row_delta == -1) {
      return (col_delta == 1) ? Board2DHex::Direction::UP_RT : Board2DHex::Direction::UP_LT;
    }
  }
  else { // (column_idx_ % 2) == 1
    if (row_delta == 0) {
      return (col_delta == 1) ? Board2DHex::Direction::UP_RT : Board2DHex::Direction::UP_LT;
    }
    else if (row_delta == 1) {
      return (col_delta == 1) ? Board2DHex::Direction::DN_RT : Board2DHex::Direction::DN_LT;
    }
  }

  return Board2DHex::Direction::NONE;
}

std::string
Unit::ToString() const
{
  std::stringstream sstr;

  sstr
    << ArmyKindToString(army_kind_) << " " << UnitKindToString(unit_kind_)
    << " (" << name_ << ") " << combat_factor_ << "-" << move_factor_
    << " (expended " << move_factor_expended_ << ")";

  return sstr.str();
}

}} // namespace cifre::gettysburg
