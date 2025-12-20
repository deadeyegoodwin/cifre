// Copyright 2025, David Goodwin. All rights reserved.

#include <limits>

#include "common_error.h"
#include "gettysburg_board_cell_factory.h"
#include "gettysburg_board_move_factory.h"

namespace cifre { namespace gettysburg {

//
// BoardMoveFactory
//
BoardMoveFactory::BoardMoveFactory(
  const std::shared_ptr<BoardCellFactory>& cell_factory,
  const OOB::Scenario scenario, size_t row_cnt, size_t column_cnt) :
  cell_factory_(cell_factory), scenario_(scenario),
  row_cnt_(row_cnt), column_cnt_(column_cnt)
{
  nodes_.resize(row_cnt_ * column_cnt_);

  // For each node, initialize the adjacent indices and move costs.
  // We ignore the cutouts in the full board, those are handled by the
  // CellFactory marking those hexes as inaccessible.
  for (size_t br = 0; br < row_cnt_; ++br) {
    for (size_t bc = 0; bc < column_cnt_; ++bc) {
      Node& node = nodes_[(br * column_cnt_) + bc];
      const std::pair<int64_t, int64_t> node_rc(br, bc);

      const std::pair<int64_t, int64_t> up(br - 1, bc);
      const std::pair<int64_t, int64_t> up_rt(br - (((bc % 2) == 0) ? 1 : 0), bc + 1);
      const std::pair<int64_t, int64_t> dn_rt(br + (((bc % 2) == 1) ? 1 : 0), bc + 1);
      const std::pair<int64_t, int64_t> dn(br + 1, bc);
      const std::pair<int64_t, int64_t> dn_lt(br + (((bc % 2) == 1) ? 1 : 0), bc - 1);
      const std::pair<int64_t, int64_t> up_lt(br - (((bc % 2) == 0) ? 1 : 0), bc - 1);

      node.adjacent_idx_[size_t(Board2DHex::Direction::UP)] = RCToIndex(up);
      node.adjacent_idx_[size_t(Board2DHex::Direction::UP_RT)] = RCToIndex(up_rt);
      node.adjacent_idx_[size_t(Board2DHex::Direction::DN_RT)] = RCToIndex(dn_rt);
      node.adjacent_idx_[size_t(Board2DHex::Direction::DN)] = RCToIndex(dn);
      node.adjacent_idx_[size_t(Board2DHex::Direction::DN_LT)] = RCToIndex(dn_lt);
      node.adjacent_idx_[size_t(Board2DHex::Direction::UP_LT)] = RCToIndex(up_lt);

      node.out_move_cost_[size_t(Board2DHex::Direction::UP)] = CalculateMoveCost(node_rc, up);
      node.out_move_cost_[size_t(Board2DHex::Direction::UP_RT)] = CalculateMoveCost(node_rc, up_rt);
      node.out_move_cost_[size_t(Board2DHex::Direction::DN_RT)] = CalculateMoveCost(node_rc, dn_rt);
      node.out_move_cost_[size_t(Board2DHex::Direction::DN)] = CalculateMoveCost(node_rc, dn);
      node.out_move_cost_[size_t(Board2DHex::Direction::DN_LT)] = CalculateMoveCost(node_rc, dn_lt);
      node.out_move_cost_[size_t(Board2DHex::Direction::UP_LT)] = CalculateMoveCost(node_rc, up_lt);
    }
  }
}

size_t
BoardMoveFactory::RCToIndex(const std::pair<int64_t, int64_t>& pr) const
{
  const int64_t row = pr.first;
  const int64_t column = pr.second;

  // Hexes outside the bounds of the board are not adjacent.
  if ((row < 0) || (row >= int64_t(row_cnt_)) ||
      (column < 0) || (column >= int64_t(column_cnt_))) {
    return NO_ADJACENT;
  }

  // Also any cell marked as inaccessible in not adjacent.
  if (cell_factory_->Features(row, column).not_accessible_) {
    return NO_ADJACENT;
  }

  return (row * column_cnt_) + column;
}

float
BoardMoveFactory::CalculateMoveCost(
  const std::pair<int64_t, int64_t>& from, const std::pair<int64_t, int64_t>& to) const
{
  //const int64_t from_row = from.first;
  //const int64_t from_column = from.second;
  const int64_t to_row = to.first;
  const int64_t to_column = to.second;

  // Hexes outside the bounds of the board are moving off the board.
  if ((to_row < 0) || (to_row >= int64_t(row_cnt_)) ||
      (to_column < 0) || (to_column >= int64_t(column_cnt_))) {
    return 1.0;
  }

  const auto& features = cell_factory_->Features(to_row, to_column);

  // Moving off the board cost 1 MF.
  if (features.not_accessible_) {
    return 1.0;
  }

  return 1.0 + (features.forest_ ? 1 : 0) + (features.rocky_hill_ ? 1 : 0);
}

}} // namespace cifre::gettysburg
