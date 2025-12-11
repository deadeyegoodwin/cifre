// Copyright 2025, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <tuple>

#include "cifre_board2d.h"

namespace cifre { namespace gettysburg {

//
// BoardCellFactory
//
// Gettysburg board state is contained in multiple 2D planes of
// "cells" representing board hexes. Many of these planes contain
// constant data and so this factory pre-computes board state so that
// it can be quickly provided to Boards.
//
// The following planes are pre-computed, one instance for each board
// cell, and each movement factor, so a total of (row-cnt * column-cnt
// * max-move-factory) precomputed sets of planes. A value of
// CELL_ACTIVE is used to indicate that the position contains the
// corresponding item (for example, in the FOREST plane a value of
// CELL_ACTIVE indicates that there is a forest in the corresponding
// row/column position). The exception is the VICTORY_POINT plane
// which is initialized with the victory-points associated with the
// position.
//
//   VICTORY_POINT: plane indicates victory-points for each hex
//   FOREST: plane indicates which hexes contain forest
//   HILL: plane indicates which hexes contain hill
//   MOVE: plane indicates reachable hexes from row/column for a given
//         move factor
//
//   Other planes are initialized to CELL_EMPTY.
//
class BoardCellFactory {
public:
  static constexpr size_t PLANE_COUNT = 8;

  static constexpr size_t PLANE_IDX_VICTORY_POINT = 0;
  static constexpr size_t PLANE_IDX_FOREST = 1;
  static constexpr size_t PLANE_IDX_HILL = 2;
  static constexpr size_t PLANE_IDX_MOVE = 3;
  static constexpr size_t PLANE_IDX_UNION_UNIT = 4;
  static constexpr size_t PLANE_IDX_UNION_VP = 5;
  static constexpr size_t PLANE_IDX_CONFEDERATE_UNIT = 6;
  static constexpr size_t PLANE_IDX_CONFEDERATE_VP = 7;

  static constexpr float CELL_EMPTY = 0.0;
  static constexpr float CELL_ACTIVE = 1.0;

  // CellsKey is <row-idx, column-idx, move-factory>
  using CellsKey = std::tuple<size_t, size_t, size_t>;

public:
  BoardCellFactory(
    size_t board_row_cnt, size_t board_column_cnt,
    size_t plane_row_cnt, size_t plane_column_cnt);

  // Return the cells for a given row, column, and
  // move-factor. Thread-safe. The caller may modify the returned
  // vector, but must restore it to its original values before calling
  // ReleaseCells.
  Board2D::FlattenedCells AcquireCells(CellsKey key);

  // "Restock" the cells for a given row, column, and
  // move-factor. Thread-safe.
  void ReleaseCells(CellsKey key, Board2D::FlattenedCells&& cells);

  // String representation.
  std::string ToString() const;

private:
  BoardCellFactory(const BoardCellFactory&) = delete;
  BoardCellFactory& operator=(const BoardCellFactory&) = delete;

  size_t board_row_cnt_;
  size_t board_column_cnt_;
  size_t plane_row_cnt_;
  size_t plane_column_cnt_;

  // Statistics
  std::atomic<size_t> created_plane_sets_ {0};
  std::atomic<size_t> acquired_plane_sets_ {0};
  std::atomic<size_t> released_plane_sets_ {0};

  // Total size, in bytes of a set of input planes.
  size_t flat_vector_byte_count_;

  // Map from the row index, column index, and move factor to the
  // pre-initialized, flattened planes representing the static valued
  // portions of the sets of planes.
  std::map<CellsKey, Board2D::FlattenedCells> planes_map_;

  // Map from the row index, column index, and move factor to already
  // allocated planes. This map maintains the plane-sets that can be
  // acquired and released and thus allows reuse. This map must be
  // thread safe so a mutex is needed as well.
  std::mutex alloc_planes_mu_;
  std::map<CellsKey, std::vector<Board2D::FlattenedCells>> alloc_planes_map_;
};

}} // namespace cifre::gettysburg
