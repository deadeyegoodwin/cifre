// Copyright 2026, David Goodwin. All rights reserved.
#pragma once

#include <cstdint>
#include <sstream>
#include <string>
#include <tuple>

#include "cifre_std.h"

namespace cifre {

//
// FGColor
//
// Foreground colors for characters.
//
enum class FGColor {
  DEFAULT        = 0,
  BLACK          = 30,
  RED            = 31,
  GREEN          = 32,
  YELLOW         = 33,
  BLUE           = 34,
  MAGENTA        = 35,
  CYAN           = 36,
  WHITE          = 37,
  BRIGHT_BLACK   = 90,
  BRIGHT_RED     = 91,
  BRIGHT_GREEN   = 92,
  BRIGHT_YELLOW  = 93,
  BRIGHT_BLUE    = 94,
  BRIGHT_MAGENTA = 95,
  BRIGHT_CYAN    = 96,
  BRIGHT_WHITE   = 97
};

//
// BGColor
//
// Background colors for characters.
//
enum class BGColor {
  DEFAULT        = 0,
  BLACK          = 40,
  RED            = 41,
  GREEN          = 42,
  YELLOW         = 43,
  BLUE           = 44,
  MAGENTA        = 45,
  CYAN           = 46,
  WHITE          = 47,
  BRIGHT_BLACK   = 100,
  BRIGHT_RED     = 101,
  BRIGHT_GREEN   = 102,
  BRIGHT_YELLOW  = 103,
  BRIGHT_BLUE    = 104,
  BRIGHT_MAGENTA = 105,
  BRIGHT_CYAN    = 106,
  BRIGHT_WHITE   = 107
};

// Type representing a single character and foreground and background
// colors for that character.
class ColorChar : public std::tuple<char, FGColor, BGColor> {
public:
  ColorChar() : ColorChar(' ') { }
  ColorChar(char ch, FGColor fg = FGColor::DEFAULT, BGColor bg = BGColor::DEFAULT) :
    std::tuple<char, FGColor, BGColor>(ch, fg, bg) { }

  char Char() const { return std::get<0>(*this); }
  FGColor ForegroundColor() const { return std::get<1>(*this); }
  BGColor BackgroundColor() const { return std::get<2>(*this); }

  // Overlay 'other' onto 'this' if possible. 'this' has priority so
  // 'this' will not be changed if overlaying is not possible.
  ColorChar& Overlay(const ColorChar& other) {
    if (std::get<0>(*this) == ' ') { std::get<0>(*this) = std::get<0>(other); }
    return *this;
  }

  std::string ToString() const {
    return
      std::string("\033[") + std::to_string(uint32_t(ForegroundColor())) + ";" +
      std::to_string(uint32_t(BackgroundColor())) + "m" + Char() +
      std::string("\033[") + std::to_string(uint32_t(FGColor::DEFAULT)) + ";" +
      std::to_string(uint32_t(BGColor::DEFAULT)) + "m"; }
};

// Return std::string for concatenation of ColorChars.
template<typename... Args>
std::string ColorCharsToString(const ColorChar& c, Args... args) {
  std::stringstream sstr;
  ConcatenateColorCharsToString(sstr, c, args...);
  return sstr.str();
}

template<size_t S>
std::string ColorCharsToString(const cifre::array<ColorChar, S>& ar) {
  std::stringstream sstr;
  for (const auto& cc : ar) {
    ConcatenateColorCharsToString(sstr, cc);
  }
  return sstr.str();
}

// Support functions for ColorCharsToString, don't call directly.
void ConcatenateColorCharsToString(std::stringstream& sstr);
template<typename... Args>
void ConcatenateColorCharsToString(
  std::stringstream& sstr, const ColorChar& c, Args... args) {
  sstr << c.ToString();
  ConcatenateColorCharsToString(sstr, args...);
}

} // namespace cifre
