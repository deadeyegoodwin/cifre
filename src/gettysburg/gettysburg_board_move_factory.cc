// Copyright 2026, David Goodwin. All rights reserved.

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
      const MatrixCoordinate node_rc(br, bc);

      node.adjacent_idx_[size_t(HexDirection::UP)] =
        RCToIndex(HexAdjacentCoordinate(node_rc, HexDirection::UP));
      node.adjacent_idx_[size_t(HexDirection::UP_RT)] =
        RCToIndex(HexAdjacentCoordinate(node_rc, HexDirection::UP_RT));
      node.adjacent_idx_[size_t(HexDirection::DN_RT)] =
        RCToIndex(HexAdjacentCoordinate(node_rc, HexDirection::DN_RT));
      node.adjacent_idx_[size_t(HexDirection::DN)] =
        RCToIndex(HexAdjacentCoordinate(node_rc, HexDirection::DN));
      node.adjacent_idx_[size_t(HexDirection::DN_LT)] =
        RCToIndex(HexAdjacentCoordinate(node_rc, HexDirection::DN_LT));
      node.adjacent_idx_[size_t(HexDirection::UP_LT)] =
        RCToIndex(HexAdjacentCoordinate(node_rc, HexDirection::UP_LT));

      node.out_move_cost_[size_t(HexDirection::UP)] =
        CalculateMoveCost(node_rc, HexDirection::UP);
      node.out_move_cost_[size_t(HexDirection::UP_RT)] =
        CalculateMoveCost(node_rc, HexDirection::UP_RT);
      node.out_move_cost_[size_t(HexDirection::DN_RT)] =
        CalculateMoveCost(node_rc, HexDirection::DN_RT);
      node.out_move_cost_[size_t(HexDirection::DN)] =
        CalculateMoveCost(node_rc, HexDirection::DN);
      node.out_move_cost_[size_t(HexDirection::DN_LT)] =
        CalculateMoveCost(node_rc, HexDirection::DN_LT);
      node.out_move_cost_[size_t(HexDirection::UP_LT)] =
        CalculateMoveCost(node_rc, HexDirection::UP_LT);
    }
  }
}

size_t
BoardMoveFactory::RCToIndex(const MatrixCoordinate& pr) const
{
  // Hexes outside the bounds of the board are not adjacent.
  if ((pr.Row() < 0) || (pr.Row() >= int64_t(row_cnt_)) ||
      (pr.Column() < 0) || (pr.Column() >= int64_t(column_cnt_))) {
    return NO_ADJACENT;
  }

  // Also any cell marked as inaccessible in not adjacent.
  if (cell_factory_->CellFeaturesFromCoordinates(pr).not_accessible_) {
    return NO_ADJACENT;
  }

  return (pr.Row() * column_cnt_) + pr.Column();
}

float
BoardMoveFactory::CalculateMoveCost(
  const MatrixCoordinate& pos, HexDirection dir) const
{
  MatrixCoordinate to_pos = HexAdjacentCoordinate(pos, dir);

  // Hexes outside the bounds of the board are moving off the board.
  if ((to_pos.Row() < 0) || (to_pos.Row() >= int64_t(row_cnt_)) ||
      (to_pos.Column() < 0) || (to_pos.Column() >= int64_t(column_cnt_))) {
    return 1.0;
  }

  // Move cost associated with features of the 'to_pos' cell. Moving
  // off the board cost 1 MF. Moving into a town costs 0.5. Forest
  // costs +1, rocky-hill costs +1.
  const auto& cell_features = cell_factory_->CellFeaturesFromCoordinates(to_pos);
  if (cell_features.not_accessible_) {
    return 1.0;
  }
  else if (cell_features.town_) {
    return 0.5;
  }

  float move_cost = 1.0 + (cell_features.forest_ ? 1 : 0) + (cell_features.rocky_hill_ ? 1 : 0);

  // Move cost associated with features of the edge between 'from' and
  // 'to' cells.
#if 0
  const auto& edge_features =
    cell_factory_->EdgeFeaturesFromCoordinates(from_row, from_column, to_row, to_column);
  if (edge_features.road_ || edge_features.railroad_) {
    return 0.5;
  }

  if (edge_features.stream_) {
    move_cost += 1;
  }
#endif
  return move_cost;
}

}} // namespace cifre::gettysburg
