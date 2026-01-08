// Copyright 2026, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstdint>
#include <cstring>

#include "cifre_vec.h"

namespace cifre {

// Hex directions.
enum class HexDirection { UP=0, UP_RT=1, DN_RT=2, DN=3, DN_LT=4, UP_LT=5, NONE=6 };
static constexpr size_t HEX_DIRECTION_COUNT = 6;

// Return the string representation of a hex direction.
std::string HexDirectionToString(const HexDirection d);

// Return the coordinates of the hex that is adjacent to the hex at
// position 'pos' in direction 'dir'. Return MatrixCoordinate::UNKNOWN
// if the adjacent hex has negative coordinates.
MatrixCoordinate HexAdjacentCoordinate(
  const MatrixCoordinate& pos, HexDirection dir);

// If 'from' and 'to' are the position of adjacent hexes, return the
// HexDirection of 'from' -> 'to'. If 'from' and 'to' are not
// adjacent, return HexDirection::NONE.
HexDirection HexAdjacentDirection(
  const MatrixCoordinate& from, const MatrixCoordinate& to);

} // namespace cifre
