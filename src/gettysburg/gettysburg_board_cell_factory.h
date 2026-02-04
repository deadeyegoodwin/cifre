// Copyright 2026, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <memory_resource>
#include <mutex>

#include "cifre_boardhex.h"
#include "cifre_std.h"
#include "gettysburg_board_cell_data.h"
#include "gettysburg_oob.h"

namespace cifre { namespace gettysburg {

//
// BoardCellFactory
//
// Gettysburg board state is contained in multiple 2D planes of
// "cells" representing board hexes. Some of these planes contain
// constant data and so this factory pre-computes board state so that
// it can be quickly provided to Boards.
//
// The following planes are pre-computed, one instance for each board
// cell, so a total of (row-cnt * column-cnt) precomputed sets of
// planes. For FOREST, HILL and ROCKY_HILL planes, a value of
// CELL_ACTIVE is used to indicate that the position contains a forest
// or hill, and a value of CELL_EMPTY indicates it does not. For the
// VICTORY_POINT plane each cell is initialized with the
// victory-points associated with the position.
//
//   VICTORY_POINT: victory-points for each hex
//   FOREST: forest hexes
//   HILL: hill hexes
//   ROCKY_HILL: rocky hill hexes
//
// The non-constant planes are initialized to CELL_EMPTY. Their values
// are managed by the Board. For UNION_VP and CONFEDERATE_VP a cell's
// value indicates the number of victory-points captured by each army
// at that cell. For other planes a value of CELL_ACTIVE is used to
// indicate that the position has the property corresponding to the
// cell, and a value of CELL_EMPTY indicates it does not. The
// non-constant planes are:
//
//   UNION_VP: victory-points captured by union
//   CONFEDERATE_VP: victory-points captured by confederate
//   CONFEDERATE: position contains a confederate unit
//   UNION: position contains a union unit
//   MOVE_0, MOVE_5, MOVE_7, MOVE_10: confederate or union unit at this
//     position has the corresponding movement factor
//
// FIXME this setup has some problems. Probably would be better to
// allow move action to every hex on the board and then have per-turn
// history of union and confederate units, perhaps with move-factor
// value, for example PLANE_IDX_UNION_MOVE_5.
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

    // Edge is inaccessible if cell on either side is inaccessible
    bool not_accessible_ {false};
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
    cifre::array<EdgeFeatures, 1 + HEX_RADIUS1_LAST_DIRECTION_IDX> edges_;
  };

  // The planes.
  static constexpr size_t PLANE_COUNT = 12;

  static constexpr size_t PLANE_IDX_VICTORY_POINT = 0;
  static constexpr size_t PLANE_IDX_FOREST = 1;
  static constexpr size_t PLANE_IDX_HILL = 2;
  static constexpr size_t PLANE_IDX_ROCKY_HILL = 3;
  static constexpr size_t PLANE_IDX_UNION_VP = 4;
  static constexpr size_t PLANE_IDX_CONFEDERATE_VP = 5;
  static constexpr size_t PLANE_IDX_UNION = 6;
  static constexpr size_t PLANE_IDX_CONFEDERATE = 7;
  static constexpr size_t PLANE_IDX_MOVE_0 = 8;
  static constexpr size_t PLANE_IDX_MOVE_5 = 9;
  static constexpr size_t PLANE_IDX_MOVE_7 = 10;
  static constexpr size_t PLANE_IDX_MOVE_10 = 11;
  static_assert(PLANE_COUNT == (PLANE_IDX_MOVE_10 + 1));

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
  BoardCellFactory(const OOB::Scenario scenario, cifre::MatrixSize board_size);

  // Return the size of each plane in the plane-set.
  const cifre::MatrixSize& PlaneSize() const { return plane_size_; }

  // Return the number of border rows and border columns.
  const cifre::MatrixSize& PlaneBorderSize() const { return plane_border_size_; }

  // The number of cells/hexes containing a given 'kind' of feature.
  size_t CellFeaturesCount(CellFeatureKind kind) const {
    return cell_features_offsets_[size_t(kind)].size(); }

  // Return the CellFeatures for the 'n'th cell for a given feature
  // 'kind'. 'n' must be less than FeaturesCount.
  const CellFeatures& IndexToCellFeatures(CellFeatureKind kind, size_t n) const;

  // Return the CellFeatures for a given 'row' / 'column'.
  const CellFeatures& PositionToCellFeatures(const MatrixCoordinate& pos) const;

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
  BoardCellFactory& operator=(BoardCellFactory&&) = delete;

  const OOB::Scenario scenario_;
  const cifre::MatrixSize board_size_;

  cifre::MatrixSize plane_border_size_;
  cifre::MatrixSize plane_size_;

  // Total size, in bytes of a set of input planes.
  size_t flat_vector_byte_count_;

  // Map from the row index and column index to the pre-initialized,
  // flattened planes representing the static valued portions of the
  // sets of planes.
  std::unordered_map<CellsKey, FlattenedCells, CellsKeyHash, CellsKeyEqual> planes_map_;

  // Indexed by flattened cell index that gives features of each cell.
  std::vector<CellFeatures> cell_features_;

    // For each feature kind, a vector of offsets into cell_features_.
  cifre::array<std::vector<size_t>, CELL_FEATURE_COUNT> cell_features_offsets_;
};

}} // namespace cifre::gettysburg
