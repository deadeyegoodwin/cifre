// Copyright 2025, David Goodwin. All rights reserved.

#include <limits>

#include "common_error.h"
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

  // First go through and assign each node it's row and column. Not
  // strictly needed as row/column is known from the Node's position
  // in 'nodes_', but having these values helps with some correctness
  // checking and other steps.
  //
  // FIXME do we use these???
  {
    size_t idx = 0;
    for (size_t br = 0; br < row_cnt_; ++br) {
      for (size_t bc = 0; bc < column_cnt_; ++bc) {
        nodes_[idx].row_ = br;
        nodes_[idx].column_ = bc;
        idx++;
      }
    }
  }

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

      node.adjacent_idx_[UP] = RCToIndex(up);
      node.adjacent_idx_[UP_RT] = RCToIndex(up_rt);
      node.adjacent_idx_[DN_RT] = RCToIndex(dn_rt);
      node.adjacent_idx_[DN] = RCToIndex(dn);
      node.adjacent_idx_[DN_LT] = RCToIndex(dn_lt);
      node.adjacent_idx_[UP_LT] = RCToIndex(up_lt);

      node.out_move_cost_[UP] = CalculateMoveCost(node_rc, up);
      node.in_move_cost_[UP] = CalculateMoveCost(up, node_rc);
      node.out_move_cost_[UP_RT] = CalculateMoveCost(node_rc, up_rt);
      node.in_move_cost_[UP_RT] = CalculateMoveCost(up_rt, node_rc);
      node.out_move_cost_[DN_RT] = CalculateMoveCost(node_rc, dn_rt);
      node.in_move_cost_[DN_RT] = CalculateMoveCost(dn_rt, node_rc);
      node.out_move_cost_[DN] = CalculateMoveCost(node_rc, dn);
      node.in_move_cost_[DN] = CalculateMoveCost(dn, node_rc);
      node.out_move_cost_[DN_LT] = CalculateMoveCost(node_rc, dn_lt);
      node.in_move_cost_[DN_LT] = CalculateMoveCost(dn_lt, node_rc);
      node.out_move_cost_[UP_LT] = CalculateMoveCost(node_rc, up_lt);
      node.in_move_cost_[UP_LT] = CalculateMoveCost(up_lt, node_rc);
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
  if (!cell_factory_->IsAccessible(row, column)) {
    return NO_ADJACENT;
  }

  return (row * column_cnt_) + column;
}

float
BoardMoveFactory::CalculateMoveCost(
  const std::pair<int64_t, int64_t>& from, const std::pair<int64_t, int64_t>& to) const
{
  const int64_t from_row = from.first;
  const int64_t from_column = from.second;
  const int64_t to_row = to.first;
  const int64_t to_column = to.second;

  // Moving off the board cost 1 MF but the logic for that is handled
  // elsewhere by recognizing that attempting a NO_ADJACENT transition
  // moves the unit off the board.
  if ((from_row < 0) || (from_row >= int64_t(row_cnt_)) ||
      (from_column < 0) || (from_column >= int64_t(column_cnt_)) ||
      (to_row < 0) || (to_row >= int64_t(row_cnt_)) ||
      (to_column < 0) || (to_column >= int64_t(column_cnt_))) {
    return 0.0;
  }

  // Use the cell factory to determine what is in the 'to'
  // hexes and from that determine the move cost.
  const bool forest = cell_factory_->IsForest(to_row, to_column);
  const bool rocky_hill = cell_factory_->IsRockyHill(to_row, to_column);

  return 1.0 + (forest ? 1 : 0) + (rocky_hill ? 1 : 0);
}

}} // namespace cifre::gettysburg
