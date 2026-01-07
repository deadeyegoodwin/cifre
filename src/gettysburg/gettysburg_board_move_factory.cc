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
  const BoardCellFactory* cell_factory,
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

      node.adjacent_idx_[size_t(HexDirection::UP)] = RCToIndex(up);
      node.adjacent_idx_[size_t(HexDirection::UP_RT)] = RCToIndex(up_rt);
      node.adjacent_idx_[size_t(HexDirection::DN_RT)] = RCToIndex(dn_rt);
      node.adjacent_idx_[size_t(HexDirection::DN)] = RCToIndex(dn);
      node.adjacent_idx_[size_t(HexDirection::DN_LT)] = RCToIndex(dn_lt);
      node.adjacent_idx_[size_t(HexDirection::UP_LT)] = RCToIndex(up_lt);

      node.out_move_cost_[size_t(HexDirection::UP)] = CalculateMoveCost(node_rc, up);
      node.out_move_cost_[size_t(HexDirection::UP_RT)] = CalculateMoveCost(node_rc, up_rt);
      node.out_move_cost_[size_t(HexDirection::DN_RT)] = CalculateMoveCost(node_rc, dn_rt);
      node.out_move_cost_[size_t(HexDirection::DN)] = CalculateMoveCost(node_rc, dn);
      node.out_move_cost_[size_t(HexDirection::DN_LT)] = CalculateMoveCost(node_rc, dn_lt);
      node.out_move_cost_[size_t(HexDirection::UP_LT)] = CalculateMoveCost(node_rc, up_lt);
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
  if (cell_factory_->CellFeaturesFromCoordinates(row, column).not_accessible_) {
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

  // Hexes outside the bounds of the board are moving off the board.
  if ((to_row < 0) || (to_row >= int64_t(row_cnt_)) ||
      (to_column < 0) || (to_column >= int64_t(column_cnt_))) {
    return 1.0;
  }

  // Move cost associated with features of the 'to' cell. Moving off
  // the board cost 1 MF. Moving into a town costs 0.5.
  const auto& cell_features = cell_factory_->CellFeaturesFromCoordinates(to_row, to_column);
  if (cell_features.not_accessible_) {
    return 1.0;
  }
  else if (cell_features.town_) {
    return 0.5;
  }

  float move_cost = 1.0 + (cell_features.forest_ ? 1 : 0) + (cell_features.rocky_hill_ ? 1 : 0);

  // Move cost associated with features of the edge between 'from' and
  // 'to' cells.
  const auto& edge_features =
    cell_factory_->EdgeFeaturesFromCoordinates(from_row, from_column, to_row, to_column);
  if (edge_features.road_ || edge_features.railroad_) {
    return 0.5;
  }

  if (edge_features.stream_) {
    move_cost += 1;
  }

  return move_cost;
}

}} // namespace cifre::gettysburg
