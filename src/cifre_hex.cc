// Copyright 2026, David Goodwin. All rights reserved.

#include <string>

#include "cifre_hex.h"
#include "cifre_std.h"
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

HexDirection
HexOppositeDirection(const HexDirection dir)
{
  switch (dir) {
  case HexDirection::NONE: return HexDirection::NONE;
  // Radius 1
  case HexDirection::UP: return HexDirection::DN;
  case HexDirection::UR: return HexDirection::DL;
  case HexDirection::DR: return HexDirection::UL;
  case HexDirection::DN: return HexDirection::UP;
  case HexDirection::DL: return HexDirection::UR;
  case HexDirection::UL: return HexDirection::DR;
  // Radius 2
  case HexDirection::UP_UP: return HexDirection::DN_DN;
  case HexDirection::UP_UL: return HexDirection::DN_DR;
  case HexDirection::UP_UR: return HexDirection::DN_DL;
  case HexDirection::DN_DN: return HexDirection::UP_UP;
  case HexDirection::DN_DL: return HexDirection::UP_UR;
  case HexDirection::DN_DR: return HexDirection::UP_UL;
  case HexDirection::UL_UL: return HexDirection::DR_DR;
  case HexDirection::UL_DL: return HexDirection::UR_DR;
  case HexDirection::UR_UR: return HexDirection::DL_DL;
  case HexDirection::UR_DR: return HexDirection::UL_DL;
  case HexDirection::DL_DL: return HexDirection::UR_UR;
  case HexDirection::DR_DR: return HexDirection::UL_UL;
  }

  return HexDirection::NONE; // unreachable
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

constexpr MC operator+(const MC& lhs, const MC& rhs) {
  return MC(lhs.row_ + rhs.row_, lhs.col_ + rhs.col_);
}

constexpr MC operator-(const MC& lhs, const MC& rhs) {
  return MC(lhs.row_ - rhs.row_, lhs.col_ - rhs.col_);
}

static constexpr MC epos(HEX_MAX_RADIUS * 4, HEX_MAX_RADIUS * 4);
static constexpr cifre::array<MC, HEX_TOTAL_DIRECTION_COUNT> even_column_delta = {
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

static constexpr MC opos(HEX_MAX_RADIUS * 4, (HEX_MAX_RADIUS * 4) + 1);
static constexpr cifre::array<MC, HEX_TOTAL_DIRECTION_COUNT> odd_column_delta = {
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

//
// Startup
//
// Static class instantiation to initialize data at startup.
//
static struct Startup {
  Startup() {
    for (size_t idx = 0; idx < delta_size_; ++idx) {
      even_delta_to_hex_dir_[idx].fill(HexDirection::NONE);
      odd_delta_to_hex_dir_[idx].fill(HexDirection::NONE);
    }

    for (size_t dir = 0; dir < HEX_TOTAL_DIRECTION_COUNT; ++dir) {
      const MC emc = even_column_delta[dir] + MC(delta_offset_, delta_offset_);
      assert((emc.row_ >= 0) && (emc.row_ < delta_size_) &&
             (emc.col_ >= 0) && (emc.col_ < delta_size_));
      even_delta_to_hex_dir_[emc.row_][emc.col_] = HexDirection(dir);

      const MC omc = odd_column_delta[dir] + MC(delta_offset_, delta_offset_);
      assert((omc.row_ >= 0) && (omc.row_ < delta_size_) &&
             (omc.col_ >= 0) && (omc.col_ < delta_size_));
      odd_delta_to_hex_dir_[omc.row_][omc.col_] = HexDirection(dir);
    }
  }

  static constexpr int64_t delta_offset_ = HEX_MAX_RADIUS;
  static constexpr int64_t delta_size_ = 1 + (HEX_MAX_RADIUS * 2);

  cifre::array<cifre::array<HexDirection, delta_size_>, delta_size_> even_delta_to_hex_dir_;
  cifre::array<cifre::array<HexDirection, delta_size_>, delta_size_> odd_delta_to_hex_dir_;
} sui;

} // namespace

HexDirection
PositionToHexDirection(const MatrixCoordinate& from, const MatrixCoordinate& to)
{
  MatrixCoordinate delta =
    to - from + MatrixCoordinate(sui.delta_offset_, sui.delta_offset_);
  if ((delta.Row() < 0) || (delta.Column() < 0) ||
      (delta.Row() >= sui.delta_size_) || (delta.Column() >= sui.delta_size_)) {
    return HexDirection::NONE;
  }

  if ((from.Column() % 2) == 0) {
    return sui.even_delta_to_hex_dir_[delta.Row()][delta.Column()];
  }

  return sui.odd_delta_to_hex_dir_[delta.Row()][delta.Column()];
}

MatrixCoordinate
HexDirectionToPosition(const MatrixCoordinate& pos, HexDirection dir)
{
  if ((pos.Column() % 2) == 0) {
    const MC& mc = even_column_delta[size_t(dir)];
    return pos + MatrixCoordinate(mc.row_, mc.col_);
  }

  const MC& mc = odd_column_delta[size_t(dir)];
  return pos + MatrixCoordinate(mc.row_, mc.col_);
}

HexDirection
HexDirectionConcatenate(HexDirection dir0, HexDirection dir1)
{
  // Can use either even or odd starting position, will get same
  // result...
  const MatrixCoordinate pos(HEX_MAX_RADIUS * 4, HEX_MAX_RADIUS * 4);
  return
    PositionToHexDirection(
      pos, HexDirectionToPosition(HexDirectionToPosition(pos, dir0), dir1));
}

} // namespace cifre
