// Copyright 2025, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <tuple>

#include "cifre_board2d.h"
#include "gettysburg_oob.h"

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
// cell, so a total of (row-cnt * column-cnt) precomputed sets of
// planes. For FOREST and HILL planes, a value of CELL_ACTIVE is used
// to indicate that the position contains a forest or hill, and a
// value of CELL_EMPTY indicates it does not.  For the VICTORY_POINT
// plane each cell is initialized with the victory-points associated
// with the position.
//
//   VICTORY_POINT: victory-points for each hex
//   FOREST: forest hexes
//   HILL: hill or rocky hill hexes
//
// The non-constant planes are initialized to CELL_EMPTY. Their values
// are managed by the Board. These planes' values are positive numbers
// indicating the number of associated victory-points or units in the
// corresponding hex. The non-constant planes are:
//
//   UNION_VP: victory-points captured by union
//   CONFEDERATE_VP: victory-points captured by confederate
//   CONFEDERATE_HEADQUARTERS: hexes containing confederate headquarters unit
//   CONFEDERATE_INFANTRY_c_m: hexes containing confederate infantry unit with
//                             combat-factor 'c' and move-factor 'm'
//   CONFEDERATE_CAVALRY_c_m: hexes containing confederate cavalry unit with
//                            combat-factor 'c' and move-factor 'm'
//   CONFEDERATE_ARTILLERY_c_m: hexes containing confederate artillery unit with
//                              combat-factor 'c' and move-factor 'm'
//   UNION_HEADQUARTERS: hexes containing union headquarters unit
//   UNION_INFANTRY_c_m: hexes containing union infantry unit with
//                       combat-factor 'c' and move-factor 'm'
//   UNION_CAVALRY_c_m: hexes containing union cavalry unit with
//                            combat-factor 'c' and move-factor 'm'
//   UNION_ARTILLERY_c_m: hexes containing union artillery unit with
//                        combat-factor 'c' and move-factor 'm'
//
class BoardCellFactory {
public:
  static constexpr size_t PLANE_COUNT = 26;

  static constexpr size_t PLANE_IDX_VICTORY_POINT = 0;
  static constexpr size_t PLANE_IDX_FOREST = 1;
  static constexpr size_t PLANE_IDX_HILL = 2;
  static constexpr size_t PLANE_IDX_UNION_VP = 3;
  static constexpr size_t PLANE_IDX_CONFEDERATE_VP = 4;
  static constexpr size_t PLANE_IDX_CONFEDERATE_HEADQUARTERS = 5;
  static constexpr size_t PLANE_IDX_CONFEDERATE_INFANTRY_2_5 = 6;
  static constexpr size_t PLANE_IDX_CONFEDERATE_INFANTRY_3_5 = 7;
  static constexpr size_t PLANE_IDX_CONFEDERATE_INFANTRY_4_5 = 8;
  static constexpr size_t PLANE_IDX_CONFEDERATE_INFANTRY_5_5 = 9;
  static constexpr size_t PLANE_IDX_CONFEDERATE_INFANTRY_6_5 = 10;
  static constexpr size_t PLANE_IDX_CONFEDERATE_CAVALRY_1_7 = 11;
  static constexpr size_t PLANE_IDX_CONFEDERATE_ARTILLERY_1_5 = 12;
  static constexpr size_t PLANE_IDX_CONFEDERATE_ARTILLERY_2_5 = 13;
  static constexpr size_t PLANE_IDX_CONFEDERATE_ARTILLERY_1_7 = 14;
  static constexpr size_t PLANE_IDX_UNION_HEADQUARTERS = 15;
  static constexpr size_t PLANE_IDX_UNION_INFANTRY_1_5 = 16;
  static constexpr size_t PLANE_IDX_UNION_INFANTRY_2_5 = 17;
  static constexpr size_t PLANE_IDX_UNION_INFANTRY_3_5 = 18;
  static constexpr size_t PLANE_IDX_UNION_INFANTRY_4_5 = 19;
  static constexpr size_t PLANE_IDX_UNION_CAVALRY_1_7 = 20;
  static constexpr size_t PLANE_IDX_UNION_CAVALRY_2_7 = 21;
  static constexpr size_t PLANE_IDX_UNION_ARTILLERY_1_5 = 22;
  static constexpr size_t PLANE_IDX_UNION_ARTILLERY_2_5 = 23;
  static constexpr size_t PLANE_IDX_UNION_ARTILLERY_1_7 = 24;
  static constexpr size_t PLANE_IDX_UNION_ARTILLERY_2_7 = 25;

  static constexpr float CELL_EMPTY = 0.0;
  static constexpr float CELL_ACTIVE = 1.0;

  // CellsKey is <row-idx, column-idx>
  using CellsKey = std::tuple<size_t, size_t>;

public:
  BoardCellFactory(
    const OOB::Scenario scenario,
    size_t board_row_cnt, size_t board_column_cnt,
    size_t plane_row_cnt, size_t plane_column_cnt);

  // Return the cells for a given row, column, and
  // move-factor. Thread-safe. The caller may modify the returned
  // vector, but must restore it to its original values before calling
  // ReleaseCells.
  Board2D::FlattenedCells AcquireCells(const CellsKey key);

  // "Restock" the cells for a given row, column, and
  // move-factor. Thread-safe.
  void ReleaseCells(const CellsKey key, Board2D::FlattenedCells&& cells);

  // String representation.
  std::string ToString() const;

private:
  BoardCellFactory(const BoardCellFactory&) = delete;
  BoardCellFactory& operator=(const BoardCellFactory&) = delete;

  const OOB::Scenario scenario_;
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
