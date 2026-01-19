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

static constexpr size_t HEX_MAX_RADIUS = 2;

static constexpr size_t HEX_RADIUS1_FIRST_DIRECTION_IDX = size_t(HexDirection::UP);
static constexpr size_t HEX_RADIUS1_LAST_DIRECTION_IDX = size_t(HexDirection::UL);

static constexpr size_t HEX_RADIUS2_FIRST_DIRECTION_IDX = size_t(HexDirection::UP_UP);
static constexpr size_t HEX_RADIUS2_LAST_DIRECTION_IDX = size_t(HexDirection::DR_DR);

static constexpr size_t HEX_RADIUS_TO_TOTAL_DIRECTION_COUNT(size_t r) {
  return((r == 0) ? 1 :
         ((r == 1) ?
          (HEX_RADIUS1_LAST_DIRECTION_IDX + 1) :
          (HEX_RADIUS2_LAST_DIRECTION_IDX + 1))); }

static constexpr size_t HEX_TOTAL_DIRECTION_COUNT =
  HEX_RADIUS_TO_TOTAL_DIRECTION_COUNT(HEX_MAX_RADIUS);

// Return the string representation of a hex direction.
std::string HexDirectionToString(const HexDirection dir);

// Parse a string into a HexDirection.
std::optional<HexDirection> StringToHexDirection(const std::string& str);

// For a 'dir'ection return the direction that is opposite 'dir'. If
// 'dir' is HexDirection::NONE return HexDirection::NONE.
HexDirection HexOppositeDirection(const HexDirection dir);

// 'from' and 'to' are hex positions, return the HexDirection of
// 'from' -> 'to'. If no HexDirection corresponds to 'from' and 'to'
// then return HexDirection::NONE. Also return HexDirection::NONE if
// 'from' == 'to'.
HexDirection PositionToHexDirection(
  const MatrixCoordinate& from, const MatrixCoordinate& to);

// Return the coordinates of the hex that is 'dir'ection relative to
// hex at position 'pos'. The returned coordinates can be negative if
// the 'dir'ection leads "out of" the matrix.
MatrixCoordinate HexDirectionToPosition(
  const MatrixCoordinate& pos, HexDirection dir);

// Assume P is a hex. S is the hex at 'dir0' relative to P, that is P
// -> dir0 -> S. D is the hex that is 'dir1' relative to S, that is S
// -> dir1 -> D. Return the HexDirection, cdir, that satisfies P ->
// cdir -> D. Return HexDirection::NONE if cdir is not a valid
// HexDirection.
HexDirection HexDirectionConcatenate(HexDirection dir0, HexDirection dir1);

} // namespace cifre
