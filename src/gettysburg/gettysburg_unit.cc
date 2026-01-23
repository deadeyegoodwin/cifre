// Copyright 2026, David Goodwin. All rights reserved.

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
           float combat_factor, float move_factor, const MatrixCoordinate& position) :
  name_(name), army_kind_(army_kind), unit_kind_(unit_kind),
  combat_factor_(combat_factor), move_factor_(move_factor),
  move_plane_idx_(MoveFactorToMovePlaneIndex(move_factor)),
  move_factor_expended_(0.0), position_(position)
{
}

size_t
Unit::Hash() const
{
  constexpr uint64_t mulp = 2654435789;
  uint64_t hash = 104395301;

  // Don't hash 'name_', it is just descriptive. Don't hash
  // 'move_plane_idx_', it is redundant with 'move_factor_'.

  hash += ((uint64_t(army_kind_) + 1) * mulp) ^ (hash >> 23);
  hash += ((uint64_t(unit_kind_) + 1) * mulp) ^ (hash >> 23);
  hash += (uint64_t(combat_factor_ * 100000.0) * mulp) ^ (hash >> 23);
  hash += (uint64_t(move_factor_ * 100000.0) * mulp) ^ (hash >> 23);
  hash += (uint64_t(move_factor_expended_ * 100000.0) * mulp) ^ (hash >> 23);
  hash += ((position_.Row() + 1) * mulp) ^ (hash >> 23);
  hash += ((position_.Column() + 1) * mulp) ^ (hash >> 23);

  return hash ^ (hash << 37);
}

bool
Unit::Equal(const Unit& other) const
{
  // Don't compare 'name_', it is just descriptive. Don't compare
  // 'move_plane_idx_', it is redundant with 'move_factor_'.

  return ((army_kind_ == other.army_kind_) &&
          (unit_kind_ == other.unit_kind_) &&
          (combat_factor_ == other.combat_factor_) &&
          (move_factor_ == other.move_factor_) &&
          (move_factor_expended_ == other.move_factor_expended_) &&
          (position_ == other.position_));
}

size_t
Unit::MoveFactorToMovePlaneIndex(float move_factor) const
{
  if (move_factor == 5.0) {
    return BoardCellFactory::PLANE_IDX_MOVE_5;
  }
  else if (move_factor == 7.0) {
    return BoardCellFactory::PLANE_IDX_MOVE_7;
  }
  else if (move_factor == 10.0) {
    return BoardCellFactory::PLANE_IDX_MOVE_10;
  }

  THROW("unexpected move-factor");
  return BoardCellFactory::PLANE_IDX_MOVE_0; // unreachable
}

void
Unit::ApplyActionId(
  uint64_t aid, const cifre::array<float, MOVE_ACTIONID_COUNT>& move_cost,
  const cifre::array<bool, MOVE_ACTIONID_COUNT>& end_turn)
{
  assert(move_cost[aid] >= 0.0);

  if (aid != ACTIONID_MOVE_END) {
    const auto dir = ActionIdToHexDirection(aid);
    position_ = HexDirectionToPosition(position_, dir);
    assert((position_.Row() >= 0) && (position_.Column() >= 0));
    move_factor_expended_ += move_cost[aid];
  }

  // If move is ended then show no move-factor remaining...
  if ((aid == ACTIONID_MOVE_END) || end_turn[aid]) {
    move_factor_expended_ = move_factor_;
  }

  assert(move_factor_expended_ <= move_factor_);
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
