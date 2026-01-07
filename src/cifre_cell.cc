// Copyright 2026, David Goodwin. All rights reserved.

#include <string>
#include "cifre_cell.h"

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

} // namespace cifre
