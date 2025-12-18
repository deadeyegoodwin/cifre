// Copyright 2025, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "cifre_action.h"
#include "cifre_board2d_hex.h"

namespace cifre { namespace gettysburg {

class Board;

//
// Unit
//
// The gettysburg unit.
//
class Unit {
public:
  enum class ArmyKind {
    UNION = 0, CONFEDERATE };

  enum class UnitKind {
    HEADQUARTERS = 0, INFANTRY, CAVALRY, ARTILLERY, HORSE_ARTILLERY };

  enum class ActionKind {
    MOVE_UP = 0, MOVE_UPRIGHT, MOVE_DOWNRIGHT, MOVE_DOWN, MOVE_DOWNLEFT, MOVE_UPLEFT,
    MOVE_END, ACTION_COUNT };

  static std::string ArmyKindToString(const ArmyKind kind) {
    switch (kind) {
    case ArmyKind::UNION: return "UNION";
    case ArmyKind::CONFEDERATE: return "CONFEDERATE";
    }
    return "<none>"; // unreachable
  }

  static std::string UnitKindToString(const UnitKind kind) {
    switch (kind) {
    case UnitKind::HEADQUARTERS: return "HEADQUARTERS";
    case UnitKind::INFANTRY: return "INFANTRY";
    case UnitKind::CAVALRY: return "CAVALRY";
    case UnitKind::ARTILLERY: return "ARTILLERY";
    case UnitKind::HORSE_ARTILLERY: return "HORSE_ARTILLERY";
    }
    return "<none>"; // unreachable
  }

  static std::string ActionKindToString(const ActionKind kind) {
    switch (kind) {
    case ActionKind::MOVE_UP: return "MOVE_UP";
    case ActionKind::MOVE_UPRIGHT: return "MOVE_UPRIGHT";
    case ActionKind::MOVE_DOWNRIGHT: return "MOVE_DOWNRIGHT";
    case ActionKind::MOVE_DOWN: return "MOVE_DOWN";
    case ActionKind::MOVE_DOWNLEFT: return "MOVE_DOWNLEFT";
    case ActionKind::MOVE_UPLEFT: return "MOVE_UPLEFT";
    case ActionKind::MOVE_END: return "MOVE_END";
    case ActionKind::ACTION_COUNT:
      break;
    }
    return "<none>"; // unreachable
  }

public:
  // Create unit at a given board position.
  Unit(const std::string& name, ArmyKind army_kind, UnitKind unit_kind,
       float combat_factor, float move_factor, size_t row_idx, size_t col_idx);

  // Copy constructor.
  Unit(const Unit& other) = default;

  // Hash and equal...
  size_t Hash() const;
  bool Equal(const Unit* other) const;

  const std::string& Name() const { return name_; }
  ArmyKind Army() const { return army_kind_; }
  UnitKind Kind() const { return unit_kind_; }
  float CombatFactor() const { return combat_factor_; }
  float MoveFactor() const { return move_factor_; }
  size_t Row() const { return row_idx_; }
  size_t Column() const { return column_idx_; }

  float MoveFactorExpended() const { return move_factor_expended_; }
  float MoveFactorRemaining() const {
    return move_factor_ - move_factor_expended_; }
  void ResetMoveFactorRemaining() { move_factor_expended_ = 0.0; }

  // Update unit based on action 'kind'.
  void ApplyAction(
    ActionKind kind, const std::array<float, Board2DHex::DIRECTION_COUNT>& move_cost);

  // Update 'board' to remove this unit.
  void RemoveFromBoard(Board* board) const;

  // Update 'board' to reflect current status of this unit.
  void AddToBoard(Board* board) const;

  // If 'this' and 'unit' are in adjacent hexes, return the direction
  // from 'this' unit to 'unit'. Otherwise return Direction::NONE.
  Board2DHex::Direction AdjacentDirection(const Unit& unit) const;

  // Return string representation of the unit.
  std::string ToString() const;

private:
  size_t UnitPlane(
    ArmyKind army_kind, UnitKind unit_kind,
    float combat_factor, float move_factor) const;

  const std::string name_;
  const ArmyKind army_kind_;
  const UnitKind unit_kind_;
  const float combat_factor_;
  const float move_factor_;
  const size_t plane_idx_;

  float move_factor_expended_;
  size_t row_idx_;
  size_t column_idx_;
};

}} // namespace cifre::gettysburg
