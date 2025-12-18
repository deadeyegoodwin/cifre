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
    const std::shared_ptr<BoardCellFactory>& cell_factory,
    const OOB::Scenario scenario, size_t row_cnt, size_t column_cnt);

  // Return true if the is a hex adjacent to 'row' / 'column' in
  // direction 'dir'.
  bool IsAdjacentHex(size_t row, size_t column, Board2DHex::Direction dir) {
    return nodes_[(row * column_cnt_) + column].adjacent_idx_[size_t(dir)] != NO_ADJACENT; }

  // Return the cost to move out of 'row' / 'column' in every
  // direction.
  const std::array<float, Board2DHex::DIRECTION_COUNT>& MoveCost(
    size_t row, size_t column) const {
    return nodes_[(row * column_cnt_) + column].out_move_cost_; }

private:
  // Node represents a graph node. Edges are not represented
  // explicitly, but instead as arrays of adjacent nodes and move cost
  // for outgoing along the edge.  'adjacent_idx_' is the index into
  // 'nodes_' for the node in each of the six directions represented
  // by Direction. An index value of NO_ADJACENT indicates that there
  // is no adjacent node in that direction (i.e. edge of board).
  //
  struct Node {
    std::array<size_t, Board2DHex::DIRECTION_COUNT> adjacent_idx_ = {};
    std::array<float, Board2DHex::DIRECTION_COUNT> out_move_cost_ = {};
  };

private:
  BoardMoveFactory(const BoardMoveFactory&) = delete;
  BoardMoveFactory& operator=(const BoardMoveFactory&) = delete;

  size_t RCToIndex(const std::pair<int64_t, int64_t>& pr) const;
  float CalculateMoveCost(
    const std::pair<int64_t, int64_t>& from,
    const std::pair<int64_t, int64_t>& to) const;

  const std::shared_ptr<BoardCellFactory> cell_factory_;
  const OOB::Scenario scenario_;
  size_t row_cnt_;
  size_t column_cnt_;

  std::vector<Node> nodes_;
};

}} // namespace cifre::gettysburg
