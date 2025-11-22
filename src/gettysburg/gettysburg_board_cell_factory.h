// Copyright 2025, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

namespace cifre { namespace gettysburg {

//
// BoardCellFactory
//
// Gettysburg board state is contained in multiple 2D planes of
// "cells" representing board hexes. Many of these planes contain
// constant data and so this factory pre-computes board state so that
// it can be quickly provided to Boards. This class is a singleton
// that also shares and caches these pre-computed board states across
// all Board instances.
//
class BoardCellFactory {
public:
  static constexpr size_t PLANE_COUNT = 6;

  static constexpr size_t PLANE_IDX_FOREST = 0;
  static constexpr size_t PLANE_IDX_HILL = 1;
  static constexpr size_t PLANE_IDX_MOVE = 2;
  static constexpr size_t PLANE_IDX_UNION = 3;
  static constexpr size_t PLANE_IDX_CONFEDERATE = 4;
  static constexpr size_t PLANE_IDX_GOAL = 5;

  static constexpr float CELL_EMPTY = 0.0;
  static constexpr float CELL_ACTIVE = 1.0;

public:
  static BoardCellFactory* Instance(size_t row_cnt, size_t column_cnt) {
    static BoardCellFactory instance(row_cnt, column_cnt);
    assert(row_cnt == instance.row_cnt_);
    assert(column_cnt == instance.column_cnt_);
    return &instance;
  }

private:
  BoardCellFactory(size_t row_cnt, size_t column_cnt) :
    row_cnt_(row_cnt), column_cnt_(column_cnt) {}
  BoardCellFactory(const BoardCellFactory&) = delete;
  BoardCellFactory& operator=(const BoardCellFactory&) = delete;

  size_t row_cnt_;
  size_t column_cnt_;
};

}} // namespace cifre::gettysburg
