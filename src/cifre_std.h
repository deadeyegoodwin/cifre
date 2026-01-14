// Copyright 2026, David Goodwin. All rights reserved.
#pragma once

#include <array>
#include <cassert>

namespace cifre {

//
// Lightly customized versions of some std containers (array) that
// include error/bounds checking in debug mode. A more controllable
// alternative to defining _GLIBCXX_DEBUG or _GLIBCXX_ASSERTION across
// all compilation units.
//

//
// cifre::array
//
#ifndef NDEBUG

template <typename T, std::size_t N>
struct array : std::array<T, N> {
  T& operator[](std::size_t n) {
   assert(n < N);
   return std::array<T, N>::operator[](n);
 }
 const T& operator[](std::size_t n) const {
   assert(n < N);
   return std::array<T, N>::operator[](n);
 }
};

#else  // !NDEBUG

template <typename T, std::size_t N>
using array = std::array<T, N>;

#endif

} // namespace cifre
