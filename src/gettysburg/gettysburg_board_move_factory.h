// Copyright 2025, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

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
  static constexpr size_t NO_ADJACENT = std::numeric_limits<size_t>::max();

public:
  // Constructor
  BoardMoveFactory(
    const BoardCellFactory* cell_factory, const OOB::Scenario scenario,
    size_t row_cnt, size_t column_cnt);

  // Return true if there is an in-bounds hex adjacent to 'pos' in
  // direction 'dir'.
  bool IsInBoundsAdjacentHex(const MatrixCoordinate& pos, HexDirection dir) const {
    return nodes_[(pos.Row() * column_cnt_) + pos.Column()].adjacent_idx_[size_t(dir)] != NO_ADJACENT; }

  // Return the cost to move out of 'row' / 'column' in every
  // direction.
  const std::array<float, HEX_DIRECTION_COUNT>& MoveCost(const MatrixCoordinate& pos) const {
    return nodes_[(pos.Row() * column_cnt_) + pos.Column()].out_move_cost_; }

private:
  // Node represents a graph node. Edges are not represented
  // explicitly, but instead as arrays of adjacent nodes and move cost
  // for outgoing along the edge.  'adjacent_idx_' is the index into
  // 'nodes_' for the node in each of the six directions represented
  // by HexDirection. An index value of NO_ADJACENT indicates that
  // there is no adjacent node in that direction (i.e. edge of board).
  //
  struct Node {
    std::array<size_t, HEX_DIRECTION_COUNT> adjacent_idx_ = {};
    std::array<float, HEX_DIRECTION_COUNT> out_move_cost_ = {};
  };

private:
  BoardMoveFactory(const BoardMoveFactory&) = delete;
  BoardMoveFactory& operator=(const BoardMoveFactory&) = delete;

  size_t RCToIndex(const MatrixCoordinate& pr) const;
  float CalculateMoveCost(const MatrixCoordinate& pos, HexDirection dir) const;

  const BoardCellFactory* cell_factory_;
  const OOB::Scenario scenario_;
  size_t row_cnt_;
  size_t column_cnt_;

  std::vector<Node> nodes_;
};

}} // namespace cifre::gettysburg
