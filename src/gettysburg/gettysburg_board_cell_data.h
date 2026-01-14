// Copyright 2026, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include "cifre_boardhex.h"

namespace cifre { namespace gettysburg {

//
// BoardCellData
//
// Static information about cells of the board.
//
class BoardCellData {
public:
  BoardCellData(
    size_t r, size_t c, bool na, bool f, bool h, bool rh = false, bool t = false, float vp = 0) :
    row_(r), column_(c), no_access_(na), forest_(f), hill_(h), rocky_hill_(rh),
    town_(t), victory_points_(vp) { }

  const size_t row_;
  const size_t column_;
  const bool no_access_;
  const bool forest_;
  const bool hill_;
  const bool rocky_hill_;
  const bool town_;
  const float victory_points_;
};

static std::vector<BoardCellData> board_cell_data {
  // column 0
  // row col noaccess forest hill   rocky  town   vp
  {  0,  0,  false,   true,  true },
  {  3,  0,  false,   true,  false },
  {  4,  0,  false,   true,  false },
  {  5,  0,  false,   true,  false },
  {  6,  0,  false,   true,  false },
  {  7,  0,  false,   true,  false },
  {  8,  0,  true,    false, false },
  {  9,  0,  true,    false, false },
  { 10,  0,  true,    false, false },
  // column 1
  // row col noaccess forest hill   rocky  town   vp
  {  0,  1,  false,   true,  true },
  {  2,  1,  false,   true,  false },
  {  3,  1,  false,   true,  false },
  {  4,  1,  false,   true,  false },
  {  5,  1,  false,   true,  false },
  {  6,  1,  false,   true,  false },
  {  7,  1,  false,   true,  false },
  {  8,  1,  true,    false, false },
  {  9,  1,  true,    false, false },
  { 10,  1,  true,    false, false },
  // column 2
  // row col noaccess forest hill   rocky  town   vp
  {  0,  2,  false,   true,  false },
  {  1,  2,  false,   true,  false },
  {  3,  2,  false,   true,  false },
  {  4,  2,  false,   true,  false },
  {  5,  2,  false,   true,  false },
  {  6,  2,  false,   true,  false },
  {  7,  2,  false,   true,  false },
  {  8,  2,  true,    false, false },
  {  9,  2,  true,    false, false },
  { 10,  2,  true,    false, false },
  // column 3
  // row col noaccess forest hill   rocky  town   vp
  {  0,  3,  false,   true,  false },
  {  2,  3,  false,   true,  true },
  {  3,  3,  false,   true,  false },
  {  4,  3,  false,   true,  false },
  {  5,  3,  false,   true,  false },
  {  8,  3,  true,    false, false },
  {  9,  3,  true,    false, false },
  { 10,  3,  true,    false, false },
  // column 4
  // row col noaccess forest hill   rocky  town   vp
  {  2,  4,  false,   true,  false },
  {  3,  4,  false,   true,  true },
  {  4,  4,  false,   true,  false },
  {  5,  4,  false,   true,  false, true },
  {  6,  4,  false,   true,  false },
  { 10,  4,  false,   true,  false },
  // column 5
  // row col noaccess forest hill   rocky  town   vp
  {  1,  5,  false,   true,  false },
  {  2,  5,  false,   true,  true },
  {  3,  5,  false,   true,  false },
  {  4,  5,  false,   true,  false, true },
  {  5,  5,  false,   false, false, true,  false, 5.0 },
  { 10,  5,  true,    false, false },
  // column 6
  // row col noaccess forest hill   rocky  town   vp
  {  2,  6,  false,   true,  true },
  {  4,  6,  false,   true,  false },
  {  5,  6,  false,   true,  false },
  {  6,  6,  false,   true,  false },
  {  7,  6,  false,   true,  false },
  {  8,  6,  false,   true,  false },
  {  9,  6,  false,   true,  false },
  // column 7
  // row col noaccess forest hill   rocky  town   vp
  {  1,  7,  false,   true,  false },
  {  2,  7,  false,   true,  true },
  {  3,  7,  false,   true,  true },
  {  4,  7,  false,   true,  false },
  {  5,  7,  false,   true,  false },
  {  6,  7,  false,   true,  true },
  {  8,  7,  false,   true,  false },
  { 10,  7,  true,    false, false },
  // column 8
  // row col noaccess forest hill   rocky  town   vp
  {  0,  8,  false,   true,  false },
  {  1,  8,  false,   true,  false },
  {  2,  8,  false,   true,  true },
  {  5,  8,  false,   true,  false },
  {  6,  8,  false,   true,  false },
  {  7,  8,  false,   true,  false },
  {  8,  8,  false,   true,  false },
  {  9,  8,  false,   true,  false },
  { 10,  8,  false,   true,  false, false, false, 3.0 },
  // column 9
  // row col noaccess forest hill   rocky  town   vp
  {  0,  9,  false,   true,  false },
  {  1,  9,  false,   true,  false },
  {  2,  9,  false,   true,  false },
  {  6,  9,  false,   true,  false },
  {  7,  9,  false,   true,  false },
  {  8,  9,  false,   true,  false },
  {  9,  9,  false,   true,  false },
  { 10,  9,  true,    false, false },
  // column 10
  // row col noaccess forest hill   rocky  town   vp
  {  2, 10,  false,   true,  false },
  {  3, 10,  false,   true,  false },
  {  5, 10,  false,   false, true,  false, false, 1.0 },
  {  6, 10,  false,   false, true,  false, false, 1.0 },
  {  7, 10,  false,   true,  false },
  {  8, 10,  false,   true,  false },
  {  9, 10,  false,   true,  false },
  { 10, 10,  false,   true,  false },
  // column 11
  // row col noaccess forest hill   rocky  town   vp
  {  1, 11,  false,   true,  false },
  {  3, 11,  false,   true,  false },
  {  5, 11,  false,   false, true,  false, false, 1.0 },
  {  6, 11,  false,   false, true,  false, false, 1.0 },
  {  7, 11,  false,   true,  false },
  {  8, 11,  false,   true,  false },
  {  9, 11,  false,   true,  false },
  { 10, 11,  true,    false, false },
  // column 12
  // row col noaccess forest hill   rocky  town   vp
  {  1, 12,  false,   true,  false },
  {  3, 12,  false,   true,  false },
  {  4, 12,  false,   true,  false },
  {  6, 12,  false,   false, true,  false, false, 1.0 },
  {  8, 12,  false,   true,  true,  false, false, 3.0 },
  {  9, 12,  false,   true,  false },
  { 10, 12,  false,   true,  false },
  // column 13
  // row col noaccess forest hill   rocky  town   vp
  {  0, 13,  false,   true,  false },
  {  1, 13,  false,   true,  false },
  {  8, 13,  false,   true,  false },
  { 10, 13,  true,    false, false },
  // column 14
  // row col noaccess forest hill   rocky  town   vp
  {  1, 14,  false,   true,  false },
  {  4, 14,  false,   true,  false },
  {  6, 14,  false,  false,  false, false, true },
  // column 15
  // row col noaccess forest hill   rocky  town   vp
  {  0, 15,  false,   true,  true },
  {  1, 15,  false,   true,  false },
  {  2, 15,  false,   true,  true },
  { 10, 15,  true,    false, false },
  // column 16
  // row col noaccess forest hill   rocky  town   vp
  {  0, 16,  false,   true,  true },
  {  1, 16,  false,   true,  true },
  {  4, 16,  false,   true,  false },
  {  8, 16,  false,   true,  false },
  // column 17
  // row col noaccess forest hill   rocky  town   vp
  {  0, 17,  false,   false,  true },
  {  1, 17,  false,   false,  true },
  {  4, 17,  false,   false,  true },
  { 10, 17,  true,    false,  false },
  // column 18
  // row col noaccess forest hill   rocky  town   vp
  {  1, 18,  false,   false, true },
  {  3, 18,  false,   true,  false },
  {  4, 18,  false,   false, true },
  {  7, 18,  false,   true,  true },
  // column 19
  // row col noaccess forest hill   rocky  town   vp
  {  0, 19,  true,    false, false },
  {  1, 19,  true,    false, false },
  {  2, 19,  true,    false, false },
  {  3, 19,  false,   true,  true },
  {  4, 19,  false,   true,  true },
  {  7, 19,  false,   true,  false },
  {  9, 19,  false,   true,  false },
  { 10, 19,  true,    false, false },
  // column 20
  // row col noaccess forest hill   rocky  town   vp
  {  0, 20,  true,    false, false },
  {  1, 20,  true,    false, false },
  {  2, 20,  true,    false, false },
  {  3, 20,  true,    false, false },
  {  4, 20,  false,   true,  true },
  {  9, 20,  false,   true,  false },
  { 10, 20,  false,   true,  false },
};

//
// BoardEdgeData
//
// Static information about edges of the board.
//
class BoardEdgeData {
public:
  BoardEdgeData(
    size_t r, size_t c, HexDirection d, bool rd, bool s = false, bool rr = false) :
    row_(r), column_(c), dir_(d), road_(rd), stream_(s), railroad_(rr) { }

  const size_t row_;
  const size_t column_;
  const HexDirection dir_;
  const bool road_;
  const bool stream_;
  const bool railroad_;
};

namespace {

static constexpr HexDirection UP = HexDirection::UP;
static constexpr HexDirection UR = HexDirection::UR;
static constexpr HexDirection DR = HexDirection::DR;
static constexpr HexDirection DN = HexDirection::DN;
static constexpr HexDirection DL = HexDirection::DL;
static constexpr HexDirection UL = HexDirection::UL;

} // namespace

static std::vector<BoardEdgeData> board_edge_data {
  // column 0 adjacent
  // row col dir road   stream  railroad
  {  0,  0,  DN, true  },
  {  0,  0,  DL, true  },
  {  1,  0,  UR, true  },
  {  1,  0,  DR, true  },
  {  3,  0,  DR, true  },
  {  3,  0,  DN, true  },
  {  3,  0,  DL, true  },
  {  4,  0,  DR, true  },
  {  7,  0,  UR, true  },
  {  7,  0,  UL, true  },
  // column 1 adjacent
  // row col dir road   stream  railroad
  {  0,  1,  UR, true  },
  {  1,  1,  DR, true  },
  {  2,  1,  DR, true  },
  {  2,  1,  DN, true  },
  {  4,  1,  DN, true  },
  {  6,  1,  DR, true  },
  // column 2 adjacent
  // row col dir road   stream  railroad
  {  0,  2,  UR, true  },
  {  0,  2,  DN, true  },
  {  1,  2,  DN, true  },
  {  2,  2,  DR, true  },
  {  2,  2,  DN, true  },
  {  6,  2,  DN, true  },
  {  7,  2,  UR, true  },
  {  7,  2,  DN, true  },
  // column 3 adjacent
  // row col dir road   stream  railroad
  {  2,  3,  UR, true  },
  {  3,  3,  UR, true  },
  {  5,  3,  DN, true  },
  {  6,  3,  UR, true  },
  {  6,  3,  DN, true  },
  // column 4 adjacent
  // row col dir road   stream  railroad
  {  0,  4,  UR, true  },
  {  0,  4,  UL, true  },
  {  2,  4,  DR, true  },
  {  3,  4,  UR, true  },
  {  3,  4,  DN, true  },
  {  6,  4,  DR, true  },
  {  7,  4,  UR, true  },
  {  7,  4,  DN, true  },
  {  8,  4,  DL, true  },
  {  9,  4,  UR, true  },
  {  9,  4,  DN, false, true  },
  {  9,  4,  DL, true  },
  { 10,  4,  UR, false, true  },
  // column 5 adjacent
  // row col dir road   stream  railroad
  {  0,  5,  UP, true  },
  {  0,  5,  UR, false, true  },
  {  0,  5,  DN, true  },
  {  2,  5,  UR, true  },
  {  2,  5,  DR, true  },
  {  3,  5,  DR, true  },
  {  6,  5,  UR, true  },
  {  6,  5,  DR, true  },
  {  8,  5,  DR, true  },
  {  8,  5,  DN, true  },
  {  9,  5,  DR, false, true  },
  {  9,  5,  DN, false, true  },
  // column 6 adjacent
  // row col dir road   stream  railroad
  {  0,  6,  UL, true  },
  {  0,  6,  DR, true,  true  },
  {  0,  6,  DN, false, true  },
  {  1,  6,  UR, true  },
  {  1,  6,  DR, true  },
  {  2,  6,  UR, true  },
  {  3,  6,  DR, true  },
  {  3,  6,  DN, true  },
  {  5,  6,  UR, true  },
  {  5,  6,  DR, true  },
  {  5,  6,  DN, true  },
  {  6,  6,  DR, true  },
  {  7,  6,  DN, true  },
  {  8,  6,  DR, true  },
  {  8,  6,  DN, true  },
  {  9,  6,  DN, false, true  },
  { 10,  6,  UR, false, true  },
  // column 7 adjacent
  // row col dir road   stream  railroad
  {  0,  7,  UP, false, true  },
  {  0,  7,  UR, true,  true  },
  {  1,  7,  DN, true  },
  {  2,  7,  DN, true  },
  {  3,  7,  DR, true  },
  {  3,  7,  DN, true  },
  {  4,  7,  UR, true  },
  {  5,  7,  DR, true  },
  {  6,  7,  UR, true  },
  {  8,  7,  UR, true  },
  {  9,  7,  DR, false, true  },
  {  9,  7,  DN, false, true  },
  // column 8 adjacent
  // row col dir road   stream  railroad
  {  0,  8,  DR, true,  true  },
  {  0,  8,  DN, false, true  },
  {  4,  8,  UR, true  },
  {  4,  8,  DR, true  },
  {  6,  8,  DR, true  },
  {  6,  8,  DN, true  },
  {  7,  8,  DN, true  },
  {  8,  8,  DN, true  },
  {  9,  8,  UR, true  },
  {  9,  8,  DR, false, true  },
  {  9,  8,  DN, true,  true  },
  { 10,  8,  DL, true  },
  // column 9 adjacent
  // row col dir road   stream  railroad
  {  0,  9,  UP, false, true  },
  {  0,  9,  UR, true,  true  },
  {  4,  9,  DR, true  },
  {  6,  9,  UR, true  },
  {  8,  9,  UR, true  },
  {  8,  9,  DR, false, true  },
  {  8,  9,  DN, true,  true  },
  {  9,  9,  DR, true  },
  // column 10 adjacent
  // row col dir road   stream  railroad
  {  0,  10, DR, true  },
  {  0,  10, DN, false, true  },
  {  1,  10, UR, false, true  },
  {  5,  10, DR, true  },
  {  6,  10, UR, true  },
  {  7,  10, UR, true  },
  {  7,  10, DN, true  },
  {  8,  10, DN, false, true  },
  {  9,  10, UR, false, true  },
  { 10,  10, DN, true  },
  // column 11 adjacent
  // row col dir road   stream  railroad
  {  0,  11, DR, true  },
  {  0,  11, DN, false, true  },
  {  1,  11, UR, false, true  },
  {  1,  11, DR, true  },
  {  1,  11, DN, true  },
  {  4,  11, DR, true  },
  {  5,  11, DR, true  },
  {  6,  11, DR, true  },
  {  8,  11, DN, false, true  },
  {  8,  11, DR, false, true  },
  // column 12 adjacent
  // row col dir road   stream  railroad
  {  0,  12, UL, true  },
  {  0,  12, DN, true  },
  {  1,  12, UR, true  },
  {  1,  12, DR, true  },
  {  1,  12, DN, false, true  },
  {  2,  12, UR, false, true  },
  {  2,  12, DR, true  },
  {  5,  12, DR, true  },
  {  6,  12, UR, true  },
  {  6,  12, DR, true  },
  {  6,  12, DN, true  },
  {  7,  12, UR, true  },
  {  8,  12, DR, false, true  },
  {  8,  12, DN, false, true  },
  // column 13 adjacent
  // row col dir road   stream  railroad
  {  0,  13, DR, true  },
  {  1,  13, DN, true,  true  },
  {  2,  13, UR, true,  true  },
  {  2,  13, DR, true  },
  {  2,  13, DN, true  },
  {  3,  13, DR, true  },
  {  4,  13, DR, true  },
  {  5,  13, DR, true  },
  {  5,  13, DN, true  },
  {  6,  13, UR, true  },
  {  7,  13, UR, true  },
  {  7,  13, DN, false, true  },
  {  8,  13, UR, false, true  },
  // column 14 adjacent
  // row col dir road   stream  railroad
  {  0,  14, UR, true  },
  {  0,  14, DR, true  },
  {  0,  14, DN, true  },
  {  1,  14, DN, true  },
  {  2,  14, DR, false, true  },
  {  2,  14, DN, false, true  },
  {  4,  14, UR, true  },
  {  4,  14, DN, true  },
  {  5,  14, UR, true  },
  {  5,  14, DN, true  },
  {  6,  14, UR, false, false, true  },
  {  6,  14, DR, true,  false, true  },
  {  6,  14, DN, true  },
  {  7,  14, DR, true  },
  {  7,  14, DN, true  },
  {  8,  14, DR, false, true  },
  {  8,  14, DN, true,  true  },
  {  9,  14, DN, true  },
  { 10,  14, DL, true  },
  // column 15 adjacent
  // row col dir road   stream  railroad
  {  0,  15, DR, true  },
  {  1,  15, DN, false, true  },
  {  2,  15, UR, false, true  },
  {  3,  15, UR, true  },
  {  3,  15, DN, true  },
  {  4,  15, UR, false, false, true  },
  {  4,  15, DN, false, false, true  },
  {  5,  15, UR, true  },
  {  5,  15, DN, true  },
  {  6,  15, UR, true  },
  {  6,  15, DR, true  },
  {  6,  15, DN, false, false, true  },
  {  7,  15, DR, false, true,  true  },
  {  7,  15, DN, true,  true  },
  {  8,  15, UR, false, false, true  },
  {  8,  15, DR, true,  false, true  },
  {  9,  15, UR, true  },
  // column 16 adjacent
  // row col dir road   stream  railroad
  {  1,  16, DR, true  },
  {  2,  16, UR, true  },
  {  2,  16, DR, false, true  },
  {  2,  16, DN, true,  true  },
  {  3,  16, UR, false, false,  true  },
  {  3,  16, DN, false, false,  true  },
  {  5,  16, UR, true  },
  {  6,  16, UR, true  },
  {  6,  16, DR, true  },
  {  7,  16, UR, true  },
  {  7,  16, DR, true  },
  {  7,  16, DN, false, true  },
  {  8,  16, UR, false, true  },
  {  9,  16, DR, false, false, true  },
  {  9,  16, DN, true  },
  { 10,  16, UR, true  },
  // column 17 adjacent
  // row col dir road   stream  railroad
  {  0,  17, UR, true  },
  {  0,  17, DN, true  },
  {  1,  17, UR, false, false, true  },
  {  1,  17, DR, true,  true  },
  {  1,  17, DN, false, true,  true  },
  {  2,  17, UR, true  },
  {  4,  17, UR, true  },
  {  4,  17, DR, true  },
  {  6,  17, UR, true  },
  {  7,  17, UR, true  },
  {  7,  17, DR, false, true  },
  {  7,  17, DN, false, true  },
  {  9,  17, DR, true,  false, true  },
  // column 18 adjacent
  // row col dir road   stream  railroad
  {  0,  18, UP, true  },
  {  0,  18, UR, false, false, true  },
  {  0,  18, DN, false, false, true  },
  {  1,  18, DN, false, true  },
  {  2,  18, UR, true  },
  {  4,  18, UR, true  },
  {  5,  18, UR, true  },
  {  6,  18, UR, true  },
  {  6,  18, DR, true  },
  {  7,  18, DR, false, true  },
  {  7,  18, DN, true,  true  },
  {  8,  18, DR, true  },
  { 10,  18, UR, false, false, true  },
  { 10,  18, DR, true  },
  // column 19 adjacent
  // row col dir road   stream  railroad
  {  3,  19, UR, true  },
  {  4,  19, DR, true  },
  {  5,  19, UR, true  },
  {  6,  19, UR, true  },
  {  6,  19, DN, false, true  },
  {  7,  19, UR, true,  true  },
  {  7,  19, DN, true  },
  {  8,  19, DR, true  },
  {  9,  19, UR, true  },
  {  9,  19, DR, false, false, true  },
  {  9,  19, DN, true  },
  // column 20 adjacent
  // row col dir road   stream  railroad
  {  5,  20, DR, true  },
  {  6,  20, DR, true  },
  {  7,  20, DR, true  },
  {  7,  20, DN, false, true  },
  {  8,  20, UR, true  },
  {  8,  20, DN, true  },
  {  9,  20, UR, true  },
  { 10,  19, DN, false, false, true  },
};

}} // namespace cifre::gettysburg
