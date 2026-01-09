// Copyright 2026, David Goodwin. All rights reserved.

#include <array>
#include <string>

#include "cifre_hex.h"
#include "common_error.h"
#include "common_util.h"

namespace cifre {

std::string
HexDirectionToString(const HexDirection dir)
{
  switch (dir) {
  case HexDirection::NONE: return "NONE";
  // Radius 1
  case HexDirection::UP: return "UP";
  case HexDirection::UR: return "UR";
  case HexDirection::DR: return "DR";
  case HexDirection::DN: return "DN";
  case HexDirection::DL: return "DL";
  case HexDirection::UL: return "UL";
  // Radius 2
  case HexDirection::UP_UP: return "UP-UP";
  case HexDirection::UP_UL: return "UP-UL";
  case HexDirection::UP_UR: return "UP-UR";
  case HexDirection::DN_DN: return "DN-DN";
  case HexDirection::DN_DL: return "DN-DL";
  case HexDirection::DN_DR: return "DN-DR";
  case HexDirection::UL_UL: return "UL-UL";
  case HexDirection::UL_DL: return "UL-DL";
  case HexDirection::UR_UR: return "UR-UR";
  case HexDirection::UR_DR: return "UR-DR";
  case HexDirection::DL_DL: return "DL-DL";
  case HexDirection::DR_DR: return "DR-DR";
  }

  return "<none>"; // unreachable
}

HexDirection
HexOppositeDirection(const HexDirection dir)
{
  switch (dir) {
  case HexDirection::NONE: return HexDirection::NONE;
  case HexDirection::UP: return HexDirection::DN;
  case HexDirection::UR: return HexDirection::DL;
  case HexDirection::DR: return HexDirection::UL;
  case HexDirection::DN: return HexDirection::UP;
  case HexDirection::DL: return HexDirection::UR;
  case HexDirection::UL: return HexDirection::DR;
  default:
    break;
  }

  THROW("HexOppositeDirection() requires radius-1 direction");
}

HexDirection
HexAdjacentDirection(const MatrixCoordinate& from, const MatrixCoordinate& to)
{
  const int64_t row_delta = int64_t(to.Row()) - int64_t(from.Row());
  const int64_t col_delta = int64_t(to.Column()) - int64_t(from.Column());
  if (((row_delta == 0) && (col_delta == 0)) ||
      (abs(row_delta) > 1) || (abs(col_delta) > 1)) {
    return HexDirection::NONE;
  }

  if (col_delta == 0) {
    return (row_delta == 1) ? HexDirection::DN : HexDirection::UP;
  }

  if ((from.Column() % 2) == 0) {
    if (row_delta == 0) {
      return (col_delta == 1) ? HexDirection::DR : HexDirection::DL;
    }
    else if (row_delta == -1) {
      return (col_delta == 1) ? HexDirection::UR : HexDirection::UL;
    }
  }
  else { // (from.Column() % 2) == 1
    if (row_delta == 0) {
      return (col_delta == 1) ? HexDirection::UR : HexDirection::UL;
    }
    else if (row_delta == 1) {
      return (col_delta == 1) ? HexDirection::DR : HexDirection::DL;
    }
  }

  return HexDirection::NONE;
}

std::optional<HexDirection>
StringToHexDirection(const std::string& str)
{
  std::string ustr(str);
  common::ToUpper(ustr);

  for (size_t d = 0; d < HEX_TOTAL_DIRECTION_COUNT; ++d) {
    std::string dstr = HexDirectionToString(HexDirection(d));
    if (dstr == ustr) {
      return std::make_optional<HexDirection>(HexDirection(d));
    }
  }

  return std::nullopt;
}

namespace {

//
// MC
//
// Utility class to allow us to compute the below data tables at
// compile time (constexpr).
//
struct MC {
  constexpr MC(const MC& other) = default;
  constexpr MC(int64_t r, int64_t c) : row_(r), col_(c) {}

  constexpr MC DirectionDelta(HexDirection dir) const {
    const bool even_column = ((col_ % 2) == 0);

    switch (dir) {
    case HexDirection::UP:
      return MC(row_ - 1, col_);
    case HexDirection::UR:
      return MC(row_ - ((even_column) ? 1 : 0), col_ + 1);
    case HexDirection::DR:
      return MC(row_ + ((even_column) ? 0 : 1), col_ + 1);
    case HexDirection::DN:
      return MC(row_ + 1, col_);
    case HexDirection::DL:
      return MC(row_ + ((even_column) ? 0 : 1), col_ - 1);
    case HexDirection::UL:
      return MC(row_ - ((even_column) ? 1 : 0), col_ - 1);
    default:
      break;
    }

    return MC(row_, col_);
  }

  int64_t row_;
  int64_t col_;
};

constexpr MC
operator+(const MC& lhs, const MC& rhs) {
  return MC(lhs.row_ + rhs.row_, lhs.col_ + rhs.col_); }

constexpr MC
operator-(const MC& lhs, const MC& rhs) {
  return MC(lhs.row_ - rhs.row_, lhs.col_ - rhs.col_); }

static constexpr MC epos(10, 10);
static constexpr std::array<MC, HEX_TOTAL_DIRECTION_COUNT> even_column_delta = {
  MC(0, 0), // NONE
  // Radius 1
  epos.DirectionDelta(HexDirection::UP) - epos,
  epos.DirectionDelta(HexDirection::UR) - epos,
  epos.DirectionDelta(HexDirection::DR) - epos,
  epos.DirectionDelta(HexDirection::DN) - epos,
  epos.DirectionDelta(HexDirection::DL) - epos,
  epos.DirectionDelta(HexDirection::UL) - epos,
  // Radius 2
  epos.DirectionDelta(HexDirection::UP).DirectionDelta(HexDirection::UP) - epos,
  epos.DirectionDelta(HexDirection::UP).DirectionDelta(HexDirection::UL) - epos,
  epos.DirectionDelta(HexDirection::UP).DirectionDelta(HexDirection::UR) - epos,
  epos.DirectionDelta(HexDirection::DN).DirectionDelta(HexDirection::DN) - epos,
  epos.DirectionDelta(HexDirection::DN).DirectionDelta(HexDirection::DL) - epos,
  epos.DirectionDelta(HexDirection::DN).DirectionDelta(HexDirection::DR) - epos,
  epos.DirectionDelta(HexDirection::UL).DirectionDelta(HexDirection::UL) - epos,
  epos.DirectionDelta(HexDirection::UL).DirectionDelta(HexDirection::DL) - epos,
  epos.DirectionDelta(HexDirection::UR).DirectionDelta(HexDirection::UR) - epos,
  epos.DirectionDelta(HexDirection::UR).DirectionDelta(HexDirection::DR) - epos,
  epos.DirectionDelta(HexDirection::DL).DirectionDelta(HexDirection::DL) - epos,
  epos.DirectionDelta(HexDirection::DR).DirectionDelta(HexDirection::DR) - epos
};

static constexpr MC opos(10, 11);
static constexpr std::array<MC, HEX_TOTAL_DIRECTION_COUNT> odd_column_delta = {
  MC(0, 0), // NONE
  // Radius 1
  opos.DirectionDelta(HexDirection::UP) - opos,
  opos.DirectionDelta(HexDirection::UR) - opos,
  opos.DirectionDelta(HexDirection::DR) - opos,
  opos.DirectionDelta(HexDirection::DN) - opos,
  opos.DirectionDelta(HexDirection::DL) - opos,
  opos.DirectionDelta(HexDirection::UL) - opos,
  // Radius 2
  opos.DirectionDelta(HexDirection::UP).DirectionDelta(HexDirection::UP) - opos,
  opos.DirectionDelta(HexDirection::UP).DirectionDelta(HexDirection::UL) - opos,
  opos.DirectionDelta(HexDirection::UP).DirectionDelta(HexDirection::UR) - opos,
  opos.DirectionDelta(HexDirection::DN).DirectionDelta(HexDirection::DN) - opos,
  opos.DirectionDelta(HexDirection::DN).DirectionDelta(HexDirection::DL) - opos,
  opos.DirectionDelta(HexDirection::DN).DirectionDelta(HexDirection::DR) - opos,
  opos.DirectionDelta(HexDirection::UL).DirectionDelta(HexDirection::UL) - opos,
  opos.DirectionDelta(HexDirection::UL).DirectionDelta(HexDirection::DL) - opos,
  opos.DirectionDelta(HexDirection::UR).DirectionDelta(HexDirection::UR) - opos,
  opos.DirectionDelta(HexDirection::UR).DirectionDelta(HexDirection::DR) - opos,
  opos.DirectionDelta(HexDirection::DL).DirectionDelta(HexDirection::DL) - opos,
  opos.DirectionDelta(HexDirection::DR).DirectionDelta(HexDirection::DR) - opos
};

} // namespace

MatrixCoordinate
HexDirectionCoordinate(const MatrixCoordinate& pos, HexDirection dir)
{
  if ((pos.Column() % 2) == 0) {
    const MC& mc = even_column_delta[size_t(dir)];
    return pos + MatrixCoordinate(mc.row_, mc.col_);
  }

  const MC& mc = odd_column_delta[size_t(dir)];
  return pos + MatrixCoordinate(mc.row_, mc.col_);
}


} // namespace cifre
