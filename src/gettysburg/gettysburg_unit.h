// Copyright 2026, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "cifre_std.h"
#include "gettysburg_action.h"

namespace cifre { namespace gettysburg {

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

public:
  // Create unit at a given board position.
  Unit(const std::string& name, ArmyKind army_kind, UnitKind unit_kind,
       float combat_factor, float move_factor, const MatrixCoordinate& position);

  // Create undefined unit.
  Unit() = default;

  // Copy constructor.
  Unit(const Unit& other) = default;

  // Hash and equal...
  size_t Hash() const;
  bool Equal(const Unit& other) const;

  const std::string& Name() const { return name_; }
  ArmyKind Army() const { return army_kind_; }
  UnitKind Kind() const { return unit_kind_; }
  float CombatFactor() const { return combat_factor_; }
  float MoveFactor() const { return move_factor_; }
  const MatrixCoordinate& Position() const { return position_; }

  size_t MovePlaneIndex() const { return move_plane_idx_; }

  float MoveFactorExpended() const { return move_factor_expended_; }
  float MoveFactorRemaining() const {
    return move_factor_ - move_factor_expended_; }
  void ZeroMoveFactorRemaining() { move_factor_expended_ = move_factor_; }
  void ResetMoveFactorRemaining() { move_factor_expended_ = 0.0; }

  // Update unit based on action-id.
  void ApplyActionId(
    uint64_t aid, const cifre::array<float, MOVE_ACTIONID_COUNT>& move_cost,
    const cifre::array<bool, MOVE_ACTIONID_COUNT>& end_turn);

  // Return string representation of the unit.
  std::string ToString() const;

private:
  size_t MoveFactorToMovePlaneIndex(float move_factor) const;

  std::string name_;
  ArmyKind army_kind_;
  UnitKind unit_kind_;
  float combat_factor_;
  float move_factor_;

  size_t move_plane_idx_;
  float move_factor_expended_;
  MatrixCoordinate position_;
};

}} // namespace cifre::gettysburg
