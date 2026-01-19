// Copyright 2026, David Goodwin. All rights reserved.

#include <charconv> // Required header for std::to_chars

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

std::string
RealToMinimalString(double f)
{
  cifre::array<char, 64> buf;

  auto [ptr, err] = std::to_chars(buf.data(), buf.data() + buf.size(), f);
  if (err == std::errc{}) {
    return std::string(buf.data(), ptr);
  }

  return { };
}

void
ConcatenateColorCharsToString(std::stringstream& sstr)
{
  // does nothing...
  // needed to support varargs for ColorCharsToString()
}

} // namespace cifre
