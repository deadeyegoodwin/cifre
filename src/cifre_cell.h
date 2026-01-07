// Copyright 2026, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

namespace cifre {

// Hex directions.
enum class HexDirection { UP=0, UP_RT=1, DN_RT=2, DN=3, DN_LT=4, UP_LT=5, NONE=6 };
static constexpr size_t HEX_DIRECTION_COUNT = 6;

// Return the string representation of a hex direction.
std::string HexDirectionToString(const HexDirection d);

//
// FlattenedCells
//
// Triton-friendly cell contents. FlattenedCells is just a
// std:vector<uint8_t> protected so that it can only be copied via a
// memory arena.
//
class FlattenedCells : public std::pmr::vector<uint8_t> {
public:
  FlattenedCells(std::pmr::memory_resource* arena) :
    std::pmr::vector<uint8_t>(arena) { }

  // Construct as a copy of 'other' using 'arena' for the newly
  // created copy.
  FlattenedCells(const FlattenedCells& other, std::pmr::memory_resource* arena) :
    std::pmr::vector<uint8_t>(other.begin(), other.end(), arena) { }

  // Allow move constructor but not operator because the move operator
  // will not transfer the arena.
  FlattenedCells(FlattenedCells&&) = default;
  FlattenedCells& operator=(FlattenedCells&&) = delete;

  // Delete default copy constructor and operator because they do not
  // copy the arena, must use arena version above.
  FlattenedCells(FlattenedCells&) = delete;
  FlattenedCells& operator=(FlattenedCells&) = delete;
  FlattenedCells(const FlattenedCells&) = delete;
  FlattenedCells& operator=(const FlattenedCells&) = delete;
};

//
// FlattenedCellsHash
//
// Functor for hashing FlattenedCells. CNT indicates how many values
// from the FlattenedCells are hashed. CNT = 0 indicates that all
// values are hashed.
//
template <size_t CNT>
struct FlattenedCellsHash {
  size_t operator()(const FlattenedCells* a) const { return operator()(*a); }
  size_t operator()(const FlattenedCells& a) const {
    const size_t uint64_size = a.size() * sizeof(FlattenedCells::value_type) / sizeof(uint64_t);
    const uint64_t* uint64_ptr = reinterpret_cast<const uint64_t*>(a.data());
    const size_t uint64_stride =
      ((CNT == 0) ? size_t(1) : std::max(size_t(1), uint64_size / CNT));

    constexpr uint64_t mulp = 2654435789;
    uint64_t hash = 104395301;
    for (size_t i = 0; i < uint64_size; i += uint64_stride) {
      hash += (uint64_ptr[i] * mulp) ^ (hash >> 23);
    }

    return hash ^ (hash << 37);
  }
};

//
// FlattenedCellsEqual
//
// Functor for comparing FlattenedCells.
//
struct FlattenedCellsEqual {
  bool operator()(const FlattenedCells* a, const FlattenedCells* b) const {
    return operator()(*a, *b); }
  bool operator()(const FlattenedCells& a, const FlattenedCells& b) const {
    return a == b; }
};

} // namespace cifre
