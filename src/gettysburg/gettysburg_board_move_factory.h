// Copyright 2025, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "gettysburg_board_cell_factory.h"
#include "gettysburg_oob.h"

namespace cifre { namespace gettysburg {

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
  BoardMoveFactory(
    const std::shared_ptr<BoardCellFactory>& cell_factory,
    const OOB::Scenario scenario, size_t row_cnt, size_t column_cnt);

private:
  enum Direction { UP=0, UP_RT=1, DN_RT=2, DN=3, DN_LT=4, UP_LT=5 };
  static constexpr size_t DIRECTION_COUNT = 6;

  // Node represents a graph node. Edges are not represented
  // explicitly, but instead as arrays of adjacent nodes and move cost
  // for incoming and outgoing along the edge.  'adjacent_idx_' is the
  // index into 'nodes_' for the node in each of the six directions
  // represented by Direction. An index value of NO_ADJACENT indicates
  // that there is no adjacent node in that direction (i.e. edge of
  // board).
  //
  static constexpr size_t NO_ADJACENT = std::numeric_limits<size_t>::max();
  struct Node {
    size_t row_;
    size_t column_;
    std::array<size_t, 6> adjacent_idx_ = {};
    std::array<float, 6> in_move_cost_ = {};
    std::array<float, 6> out_move_cost_ = {};
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
