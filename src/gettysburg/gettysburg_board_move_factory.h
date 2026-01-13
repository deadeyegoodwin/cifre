// Copyright 2025, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "cifre_std.h"
#include "gettysburg_oob.h"

namespace cifre { namespace gettysburg {

class BoardCellFactory;

//
// BoardMoveFactory
//
// The move factory builds a directed graph representation of the
// board where graph nodes represent cells and graph edges represent
// borders between cells. Thus a node has a most 6 outgoing and 6
// incoming edges. Each edge carries a value indicating the move cost
// to traverse between hexes along the border represented by the edge.
//
class BoardMoveFactory {
public:
  static constexpr float MOVE_CLEAR_COST = 1.0;
  static constexpr float MOVE_FOREST_COST = 1.0;
  static constexpr float MOVE_ROCKYHILL_COST = 1.0;
  static constexpr float MOVE_STREAM_COST = 1.0;
  static constexpr float MOVE_ROAD_RAILROAD_TOWN_COST = 0.5;
  static constexpr float MOVE_ENEMY_ZOC_COST = 1.0;
  static constexpr float MOVE_EXIT_BOARD_COST = 1.0;

  static constexpr size_t NO_ADJACENT = std::numeric_limits<size_t>::max();

public:
  // Constructor
  BoardMoveFactory(
    const BoardCellFactory* cell_factory, const OOB::Scenario scenario,
    size_t row_cnt, size_t column_cnt);

  // Return true if there is an in-bounds hex adjacent to 'pos' in
  // direction 'dir'. 'dir' must be one of the radius-1 directions or
  // NONE. 'pos' must be a valid board position.
  bool IsInBoundsAdjacentHex(const MatrixCoordinate& pos, HexDirection dir) const {
    assert((size_t(dir) >= 0) && (size_t(dir) <= HEX_RADIUS1_LAST_DIRECTION_IDX));
    return nodes_[(pos.Row() * column_cnt_) + pos.Column()].adjacent_idx_[size_t(dir)] != NO_ADJACENT; }

  // For board position 'pos', return the cost to move out of the hex
  // in every radius-1 direction, assuming neither 'pos' or the
  // adjacent hex is in enemy ZOC (NONE is also included in the
  // returned array and its value is always 0.0). If a direction moves
  // off the board then maximum cost is returned for that direction.
  // 'pos' must be a valid board position.
  const cifre::array<float, 1 + HEX_RADIUS1_LAST_DIRECTION_IDX>&
  AdjacentMoveCost(const MatrixCoordinate& pos) const {
    return nodes_[(pos.Row() * column_cnt_) + pos.Column()].adj_move_costs_; }

  // For board position 'pos', return the cost to move out of the hex
  // in every radius-1 direction, assuming either 'pos' or the
  // adjacent hex is in enemy ZOC (NONE is also included in the
  // returned array and its value is always 0.0). If a direction moves
  // off the board then maximum cost is returned for that direction.
  // 'pos' must be a valid board position.
  const cifre::array<float, 1 + HEX_RADIUS1_LAST_DIRECTION_IDX>&
  AdjacentZOCMoveCost(const MatrixCoordinate& pos) const {
    return nodes_[(pos.Row() * column_cnt_) + pos.Column()].adj_zoc_move_costs_; }

private:
  // Node represents a graph node. Edges are not represented
  // explicitly, but instead as arrays of adjacent nodes and move cost
  // for outgoing along the edge.  'adjacent_idx_' is the index into
  // 'nodes_' for the node in each of the six directions represented
  // by HexDirection + NONE. An index value of NO_ADJACENT indicates
  // that there is no adjacent node in that direction (i.e. edge of
  // board).
  //
  struct Node {
    cifre::array<size_t, 1 + HEX_RADIUS1_LAST_DIRECTION_IDX> adjacent_idx_;
    cifre::array<float, 1 + HEX_RADIUS1_LAST_DIRECTION_IDX> adj_move_costs_;
    cifre::array<float, 1 + HEX_RADIUS1_LAST_DIRECTION_IDX> adj_zoc_move_costs_;
  };

private:
  BoardMoveFactory(const BoardMoveFactory&) = delete;
  BoardMoveFactory& operator=(const BoardMoveFactory&) = delete;

  size_t RCToIndex(const MatrixCoordinate& pr) const;
  cifre::array<float, 1 + HEX_RADIUS1_LAST_DIRECTION_IDX> CalculatePositionMoveCosts(
    const MatrixCoordinate& pos, bool zoc) const;
  float CalculateMoveCost(
    const MatrixCoordinate& pos, HexDirection dir, bool zoc) const;

  const BoardCellFactory* cell_factory_;
  const OOB::Scenario scenario_;
  size_t row_cnt_;
  size_t column_cnt_;

  std::vector<Node> nodes_;
};

}} // namespace cifre::gettysburg
