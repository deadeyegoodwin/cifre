// Copyright 2025, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "cifre_action.h"

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
  Unit(ArmyKind army_kind, size_t row_idx, size_t col_idx);

  // Copy constructor.
  Unit(const Unit& other) = default;

  ArmyKind Army() const { return army_kind_; }
  size_t Row() const { return row_idx_; }
  size_t Column() const { return column_idx_; }

  // Update 'board' to remove this unit.
  void ClearFromBoard(Board* board) const;

  // Update 'board' to reflect current status of this unit.
  void AddToBoard(Board* board) const;

private:
  const ArmyKind army_kind_;
  size_t row_idx_;
  size_t column_idx_;
};

}} // namespace cifre::gettysburg
