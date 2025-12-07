// Copyright 2025, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

namespace cifre { namespace gettysburg {

//
// BoardCellData
//
// Static information about each cell of the board.
//
class BoardCellData {
public:
  BoardCellData(size_t r, size_t c, bool na, bool f, bool h, float vp = 0) :
    row_idx_(r), column_idx_(c), no_access_(na), forest_(f), hill_(h),
    victory_points_(vp) { }

  const size_t row_idx_;
  const size_t column_idx_;
  const bool no_access_;
  const bool forest_;
  const bool hill_;
  const float victory_points_;
};

static std::vector<BoardCellData> board_cell_data {
  // column 0
  // row col noaccess forest hill   vp
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
  // row col noaccess forest hill   vp
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
  // row col noaccess forest hill   vp
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
  // row col noaccess forest hill   vp
  {  0,  3,  false,   true,  false },
  {  2,  3,  false,   true,  true },
  {  3,  3,  false,   true,  false },
  {  4,  3,  false,   true,  false },
  {  5,  3,  false,   true,  false },
  {  8,  3,  true,    false, false },
  {  9,  3,  true,    false, false },
  { 10,  3,  true,    false, false },
  // column 4
  // row col noaccess forest hill   vp
  {  2,  4,  false,   true,  false },
  {  3,  4,  false,   true,  true },
  {  4,  4,  false,   true,  false },
  {  5,  4,  false,   true,  true },
  {  6,  4,  false,   true,  false },
  { 10,  4,  false,   true,  false },
  // column 5
  // row col noaccess forest hill   vp
  {  1,  5,  false,   true,  false },
  {  2,  5,  false,   true,  true },
  {  3,  5,  false,   true,  false },
  {  4,  5,  false,   true,  true },
  {  5,  5,  false,   false, true,  5.0 },
  { 10,  5,  true,    false, false },
  // column 6
  // row col noaccess forest hill   vp
  {  2,  6,  false,   true,  true },
  {  4,  6,  false,   true,  false },
  {  5,  6,  false,   true,  false },
  {  6,  6,  false,   true,  false },
  {  7,  6,  false,   true,  false },
  {  8,  6,  false,   true,  false },
  {  9,  6,  false,   true,  false },
  // column 7
  // row col noaccess forest hill   vp
  {  1,  7,  false,   true,  false },
  {  2,  7,  false,   true,  true },
  {  3,  7,  false,   true,  true },
  {  4,  7,  false,   true,  false },
  {  5,  7,  false,   true,  false },
  {  6,  7,  false,   true,  true },
  {  8,  7,  false,   true,  false },
  { 10,  7,  true,    false, false },
  // column 8
  // row col noaccess forest hill   vp
  {  0,  8,  false,   true,  false },
  {  1,  8,  false,   true,  false },
  {  2,  8,  false,   true,  true },
  {  5,  8,  false,   true,  false },
  {  6,  8,  false,   true,  false },
  {  7,  8,  false,   true,  false },
  {  8,  8,  false,   true,  false },
  {  9,  8,  false,   true,  false },
  { 10,  8,  false,   true,  false, 3.0 },
  // column 9
  // row col noaccess forest hill   vp
  {  0,  9,  false,   true,  false },
  {  1,  9,  false,   true,  false },
  {  2,  9,  false,   true,  false },
  {  6,  9,  false,   true,  false },
  {  7,  9,  false,   true,  false },
  {  8,  9,  false,   true,  false },
  {  9,  9,  false,   true,  false },
  { 10,  9,  true,    false, false },
  // column 10
  // row col noaccess forest hill   vp
  {  2, 10,  false,   true,  false },
  {  3, 10,  false,   true,  false },
  {  5, 10,  false,   false, true,  1.0 },
  {  6, 10,  false,   false, true,  1.0 },
  {  7, 10,  false,   true,  false },
  {  8, 10,  false,   true,  false },
  {  9, 10,  false,   true,  false },
  { 10, 10,  false,   true,  false },
  // column 11
  // row col noaccess forest hill   vp
  {  1, 11,  false,   true,  false },
  {  3, 11,  false,   true,  false },
  {  5, 11,  false,   false, true,  1.0 },
  {  6, 11,  false,   false, true,  1.0 },
  {  7, 11,  false,   true,  false },
  {  8, 11,  false,   true,  false },
  {  9, 11,  false,   true,  false },
  { 10, 11,  true,    false, false },
  // column 12
  // row col noaccess forest hill   vp
  {  1, 12,  false,   true,  false },
  {  3, 12,  false,   true,  false },
  {  4, 12,  false,   true,  false },
  {  6, 12,  false,   false, true,  1.0 },
  {  8, 12,  false,   true,  true,  3.0 },
  {  9, 12,  false,   true,  false },
  { 10, 12,  false,   true,  false },
  // column 13
  // row col noaccess forest hill   vp
  {  0, 13,  false,   true,  false },
  {  1, 13,  false,   true,  false },
  {  8, 13,  false,   true,  false },
  { 10, 13,  true,    false, false },
  // column 14
  // row col noaccess forest hill   vp
  {  1, 14,  false,   true,  false },
  {  4, 14,  false,   true,  false },
  // column 15
  // row col noaccess forest hill   vp
  {  0, 15,  false,   true,  true },
  {  1, 15,  false,   true,  false },
  {  2, 15,  false,   true,  true },
  { 10, 15,  true,    false, false },
  // column 16
  // row col noaccess forest hill   vp
  {  0, 16,  false,   true,  true },
  {  1, 16,  false,   true,  true },
  {  4, 16,  false,   true,  false },
  {  8, 16,  false,   true,  false },
  // column 17
  // row col noaccess forest hill   vp
  {  0, 17,  false,   false,  true },
  {  1, 17,  false,   false,  true },
  {  4, 17,  false,   false,  true },
  { 10, 17,  true,    false,  false },
  // column 18
  // row col noaccess forest hill   vp
  {  1, 18,  false,   false, true },
  {  3, 18,  false,   true,  false },
  {  4, 18,  false,   false, true },
  {  7, 18,  false,   true,  true },
  // column 19
  // row col noaccess forest hill   vp
  {  0, 19,  true,    false, false },
  {  1, 19,  true,    false, false },
  {  2, 19,  true,    false, false },
  {  3, 19,  false,   true,  true },
  {  4, 19,  false,   true,  true },
  {  7, 19,  false,   true,  false },
  {  9, 19,  false,   true,  false },
  { 10, 19,  true,    false, false },
  // column 20
  // row col noaccess forest hill   vp
  {  0, 20,  true,    false, false },
  {  1, 20,  true,    false, false },
  {  2, 20,  true,    false, false },
  {  3, 20,  true,    false, false },
  {  4, 20,  false,   true,  true },
  {  9, 20,  false,   true,  false },
  { 10, 20,  false,   true,  false },
};

}} // namespace cifre::gettysburg
