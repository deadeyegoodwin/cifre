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
// AbsoluteOrientation
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
constexpr size_t ABSOLUTE_ORIENTATION_COUNT = 8;

// Convert an integer index to the corresponding
// AbsoluteOrientation. 'idx' may be any integer value and is
// evaluated modulo ABSOLUTE_ORIENTATION_COUNT.
AbsoluteOrientation IndexToAbsoluteOrientation(int64_t idx);

// Return the string representation of an absolute orientation.
std::string AbsoluteOrientationToString(AbsoluteOrientation o);
std::string AbsoluteOrientationToShortString(AbsoluteOrientation o);

//
// Actions
//
constexpr size_t ACTION_STEP_STRAIGHT = 3;
constexpr size_t ACTION_STEP_DIAGONAL = 2;

constexpr size_t TOTAL_ACTIONID_COUNT = 26;

// Return string representation of an action-id.
std::string ActionIdToString(uint64_t aid);

// Return action-id corresponding to a string. Returns
// TOTAL_ACTIONID_COUNT if 'str' is not a known action.
uint64_t StringToActionId(const std::string& str);

// Return the absolute position delta and new absolute orientation for
// an entity facing 'orientation' and performing action 'aid'.
void ActionIdToAbsoluteOutcome(
  const uint64_t aid, const AbsoluteOrientation orientation,
  MatrixCoordinate& position_delta, AbsoluteOrientation& new_orientation);

//
// Normalization
//
bool HasNSEWNormalization(AbsoluteOrientation orientation);
void RelativePositionToAbsolute(
  const MatrixCoordinate& center_pos, const AbsoluteOrientation& center_orient,
  MatrixCoordinate& pos, AbsoluteOrientation& orient);
void AbsolutePositionToRelative(
  const MatrixCoordinate& center_pos, const AbsoluteOrientation& center_orient,
  MatrixCoordinate& pos, AbsoluteOrientation& orient);

}} // namespace cifre::biplane
