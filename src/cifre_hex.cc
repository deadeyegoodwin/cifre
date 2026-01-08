// Copyright 2026, David Goodwin. All rights reserved.

#include <string>
#include "cifre_hex.h"

namespace cifre {

std::string
HexDirectionToString(const HexDirection d)
{
  switch (d) {
  case HexDirection::UP: return "UP";
  case HexDirection::UP_RT: return "UP-RT";
  case HexDirection::DN_RT: return "DN-RT";
  case HexDirection::DN: return "DN";
  case HexDirection::DN_LT: return "DN-LT";
  case HexDirection::UP_LT: return "UP-LT";
  case HexDirection::NONE: return "NONE";
  }

  return "<none>"; // unreachable
}

MatrixCoordinate
HexAdjacentCoordinate(const MatrixCoordinate& pos, HexDirection dir)
{
  int64_t br = 0, bc = 0;

  switch (dir) {
  case HexDirection::UP:
    br = pos.Row() - 1;
    bc = pos.Column();
    break;
  case HexDirection::UP_RT:
    br = pos.Row() - (((pos.Column() % 2) == 0) ? 1 : 0);
    bc = pos.Column() + 1;
    break;
  case HexDirection::DN_RT:
    br = pos.Row() + (((pos.Column() % 2) == 1) ? 1 : 0);
    bc = pos.Column() + 1;
    break;
  case HexDirection::DN:
    br = pos.Row() + 1;
    bc = pos.Column();
    break;
  case HexDirection::DN_LT:
    br = pos.Row() + (((pos.Column() % 2) == 1) ? 1 : 0);
    bc = pos.Column() - 1;
    break;
  case HexDirection::UP_LT:
    br = pos.Row() - (((pos.Column() % 2) == 0) ? 1 : 0);
    bc = pos.Column() - 1;
    break;
  case HexDirection::NONE:
    br = pos.Row();
    bc = pos.Column();
    break;
  }

  if ((br < 0) || (bc < 0)) {
    return MatrixCoordinate::UNKNOWN;
  }

  return MatrixCoordinate(br, bc);
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
      return (col_delta == 1) ? HexDirection::DN_RT : HexDirection::DN_LT;
    }
    else if (row_delta == -1) {
      return (col_delta == 1) ? HexDirection::UP_RT : HexDirection::UP_LT;
    }
  }
  else { // (from.Column() % 2) == 1
    if (row_delta == 0) {
      return (col_delta == 1) ? HexDirection::UP_RT : HexDirection::UP_LT;
    }
    else if (row_delta == 1) {
      return (col_delta == 1) ? HexDirection::DN_RT : HexDirection::DN_LT;
    }
  }

  return HexDirection::NONE;
}


} // namespace cifre
