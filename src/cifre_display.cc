// Copyright 2026, David Goodwin. All rights reserved.

#include "cifre_display.h"

namespace cifre {

ColorChar&
ColorChar::Overlay(const ColorChar& other)
{
  if (std::get<0>(*this) == ' ') {
    std::get<0>(*this) = std::get<0>(other);
  }
  if (std::get<1>(*this) == FGColor::DEFAULT) {
    std::get<1>(*this) = std::get<1>(other);
  }
  if (std::get<2>(*this) == BGColor::DEFAULT) {
    std::get<2>(*this) = std::get<2>(other);
  }

  return *this;
}

void
ConcatenateColorCharsToString(std::stringstream& sstr)
{
  // does nothing...
  // needed to support varargs for ColorCharsToString()
}

} // namespace cifre
