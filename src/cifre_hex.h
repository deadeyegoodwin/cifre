// Copyright 2026, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstdint>
#include <cstring>
#include <optional>

#include "cifre_vec.h"

namespace cifre {

// Directions from a single hex to a "terminal" hex. The directions
// with multiple parts, like UP_UR, don't indicate a path, they
// indicate the relative location of the terminal hex.
enum class HexDirection {
  NONE = 0,
  // Radius 1 (6)
  UP,       // UP
  UR,       // UP-RIGHT
  DR,       // DOWN-RIGHT
  DN,       // DOWN
  DL,       // DOWN-LEFT
  UL,       // UP-LEFT
  // Radius 2 (12)
  UP_UP, UP_UL, UP_UR,
  DN_DN, DN_DL, DN_DR,
  UL_UL, UL_DL,
  UR_UR, UR_DR,
  DL_DL,
  DR_DR
};

static constexpr size_t HEX_RADIUS1_DIRECTION_COUNT = 6;
static constexpr size_t HEX_RADIUS2_DIRECTION_COUNT = 6 + 12;
static constexpr size_t HEX_TOTAL_DIRECTION_COUNT = 1 + 6 + 12;

// Return the string representation of a hex direction.
std::string HexDirectionToString(const HexDirection dir);

// Parse a string into a HexDirection.
std::optional<HexDirection> StringToHexDirection(const std::string& str);

// For a radius-1 'dir'ection, return the direction that is opposite
// 'dir'. If 'dir' is HexDirection::NONE return
// HexDirection::NONE. Error if 'dir' is not a radius-1 direction or
// NONE.
HexDirection HexOppositeDirection(const HexDirection dir);

// If 'from' and 'to' are the position of adjacent hexes, return the
// radius-1 HexDirection of 'from' -> 'to'. If 'from' and 'to' are not
// adjacent, return HexDirection::NONE.
HexDirection HexAdjacentDirection(
  const MatrixCoordinate& from, const MatrixCoordinate& to);

// Return the coordinates of the hex that is 'dir'ection relative to
// hex at position 'pos'. The returned coordinates can be negative if
// the 'dir'ection leads "out of" the matrix.
MatrixCoordinate HexDirectionCoordinate(
  const MatrixCoordinate& pos, HexDirection dir);

} // namespace cifre
