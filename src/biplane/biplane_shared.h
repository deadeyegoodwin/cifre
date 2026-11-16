// Copyright 2026, David Goodwin. All rights reserved.
#pragma once

#include <cstdint>
#include <string>

#include "cifre_vec.h"

//
// Shared functions between cifre/biplane and ayrstrx/godot. These
// must be kept in sync with the corresponding functions in shared.gd.
//

namespace cifre { namespace biplane {

//
// Orientations
//
enum class AbsoluteOrientation {
  NORTH = 0,
  NORTH_EAST,
  EAST,
  SOUTH_EAST,
  SOUTH,
  SOUTH_WEST,
  WEST,
  NORTH_WEST
};
static constexpr size_t NUM_ABSOLUTE_ORIENTATIONS = 8;

// Return the string representation of an absolute orientation.
std::string AbsoluteOrientationToShortString(AbsoluteOrientation o);

// Return the string representation for a board-relative orientation.
// Board-relative orientations are named up, down, etc. because they
// are relative to a north-oriented board.
std::string AbsoluteOrientationToBoardShortString(AbsoluteOrientation o);

//
// Actions
//
static constexpr size_t TOTAL_ACTIONID_COUNT = 18;

// Return string representation of an action-id.
std::string ActionIdToString(uint64_t aid);

// Return action-id corresponding to a string. Returns
// TOTAL_ACTIONID_COUNT if 'str' is not a known action.
uint64_t StringToActionId(const std::string& str);

// Return the position delta and orientation delta for an entity
// facing 'orientation' and performing action 'aid'.
void ActionIdToAbsoluteDelta(
  const uint64_t aid, const AbsoluteOrientation orientation,
  MatrixCoordinate& pos_delta, int64_t& orientation_delta);

//
// Utility
//
void RelativePositionToAbsolute(
  const MatrixCoordinate& center_pos, const AbsoluteOrientation& center_orient,
  MatrixCoordinate& pos, AbsoluteOrientation& orient);
void AbsolutePositionToRelative(
  const MatrixCoordinate& center_pos, const AbsoluteOrientation& center_orient,
  MatrixCoordinate& pos, AbsoluteOrientation& orient);
void DecomposeSteps(
  const MatrixCoordinate& pos,
  size_t& diagonal_count, AbsoluteOrientation& diagonal_dir,
  size_t& straight_count, AbsoluteOrientation& straight_dir);

}} // namespace cifre::biplane
