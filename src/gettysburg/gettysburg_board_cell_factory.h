// Copyright 2025, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <memory_resource>
#include <mutex>

#include "cifre_boardhex.h"
#include "gettysburg_board_cell_data.h"
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
  // The maximum number of hexes that can be captured for victory
  // points. The actual number will vary by scenario.
  static constexpr size_t MAX_VICTORY_POINT_HEX_COUNT = 8;

  // Kinds of edge features.
  enum class EdgeFeatureKind { ROAD, STREAM, RAILROAD };
  static constexpr size_t EDGE_FEATURE_COUNT = 3;

  //
  // EdgeFeatures
  //
  // Features of an edge between cells.
  //
  class EdgeFeatures {
  public:
    EdgeFeatures& operator=(const BoardEdgeData& bd) {
      road_ = bd.road_; stream_ = bd.stream_; railroad_ = bd.railroad_;
      return *this; }
    bool road_ {false};
    bool stream_ {false};
    bool railroad_ {false};
  };

  // Kinds of cell features.
  enum class CellFeatureKind { NO_ACCESS, FOREST, HILL, ROCKY_HILL, TOWN, VICTORY_POINT };
  static constexpr size_t CELL_FEATURE_COUNT = 6;

  //
  // CellFeatures
  //
  // Features of a hex cell and features of all edges bordering the cell.
  //
  class CellFeatures {
  public:
    CellFeatures& operator=(const BoardCellData& bd) {
      row_ = bd.row_; column_ = bd.column_; not_accessible_ = bd.no_access_;
      forest_ = bd.forest_; hill_ = bd.hill_; rocky_hill_ = bd.rocky_hill_;
      town_ = bd.town_; victory_points_ = bd.victory_points_; return *this; }
    size_t row_ {0};
    size_t column_ {0};
    bool not_accessible_ {false};
    bool forest_ {false};
    bool hill_ {false};
    bool rocky_hill_ {false};
    bool town_ {false};
    float victory_points_ {0.0};
    std::array<EdgeFeatures, HEX_DIRECTION_COUNT> edges_;
  };

  // The planes.
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

  // CellsKey is <row, column>
  using CellsKey = MatrixCoordinate;
  using CellsKeyHash = MatrixCoordinateHash;
  using CellsKeyEqual = MatrixCoordinateEqual;

  static std::string CellsKeyToString(const CellsKey& k) {
    return
      "[" + std::to_string(k.Row()) + "," + std::to_string(k.Column()) + "]";
  }

public:
  BoardCellFactory(
    const OOB::Scenario scenario,
    size_t board_row_cnt, size_t board_column_cnt,
    size_t plane_row_cnt, size_t plane_column_cnt);

  // The number of cells/hexes containing a given 'kind' of feature.
  size_t CellFeaturesCount(CellFeatureKind kind) const {
    return cell_features_offsets_[size_t(kind)].size(); }

  // Return the CellFeatures for the 'n'th cell for a given feature
  // 'kind'. 'n' must be less than FeaturesCount.
  const CellFeatures& CellFeaturesFromIndex(CellFeatureKind kind, size_t n) const;

  // Return the CellFeatures for a given 'row' / 'column'.
  const CellFeatures& CellFeaturesFromCoordinates(size_t row, size_t column) const;

  // Return the plane-set for a given board row and column.
  const FlattenedCells& PlaneSet(const CellsKey key);

  // String representation.
  std::string ToString() const;

private:
  BoardCellFactory(BoardCellFactory&) = delete;
  BoardCellFactory(const BoardCellFactory&) = delete;
  BoardCellFactory& operator=(BoardCellFactory&) = delete;
  BoardCellFactory& operator=(const BoardCellFactory&) = delete;
  BoardCellFactory(BoardCellFactory&&) = delete;
  Board& operator=(BoardCellFactory&&) = delete;

  const OOB::Scenario scenario_;
  size_t board_row_cnt_;
  size_t board_column_cnt_;
  size_t plane_row_cnt_;
  size_t plane_column_cnt_;

  // Total size, in bytes of a set of input planes.
  size_t flat_vector_byte_count_;

  // Map from the row index and column index to the pre-initialized,
  // flattened planes representing the static valued portions of the
  // sets of planes.
  std::unordered_map<CellsKey, FlattenedCells, CellsKeyHash, CellsKeyEqual> planes_map_;

  // Indexed by flattened cell index that gives features of each cell.
  std::vector<CellFeatures> cell_features_;

    // For each feature kind, a vector of offsets into cell_features_.
  std::array<std::vector<size_t>, CELL_FEATURE_COUNT> cell_features_offsets_;
};

}} // namespace cifre::gettysburg
