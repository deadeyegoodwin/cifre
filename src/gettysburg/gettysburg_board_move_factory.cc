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

  // Move cost is determined by the features of the 'to_pos' cell and
  // the features of the edge between 'pos' and 'to_pos'
  const auto& to_pos_features = cell_factory_->CellFeaturesFromCoordinates(to_pos);
  const auto& edge_features = cell_factory_->CellFeaturesFromCoordinates(pos).edges_[size_t(dir)];

  // If the edge contains a road or railroad, or if moving into a town
  // then the cost is always 0.5.
  if (to_pos_features.town_ || edge_features.road_ || edge_features.railroad_) {
    return 0.5;
  }

  // Moving off the board cost 1 MF.
  if (to_pos_features.not_accessible_) {
    return 1.0;
  }

  // Otherwise move cost is 1.0, +1 for forest, +1 for rocky-hill, +1 for stream
  // crossing.
  return
    1.0 +
    (to_pos_features.forest_ ? 1 : 0) +
    (to_pos_features.rocky_hill_ ? 1 : 0) +
    (edge_features.stream_ ? 1 : 0);
}

}} // namespace cifre::gettysburg
