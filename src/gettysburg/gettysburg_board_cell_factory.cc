// Copyright 2025, David Goodwin. All rights reserved.

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <iostream>

#include "common_error.h"
#include "common_util.h"
#include "gettysburg_board_cell_factory.h"

namespace cifre { namespace gettysburg {

//
// BoardCellFactory
//
BoardCellFactory::BoardCellFactory(
  const OOB::Scenario scenario, size_t board_row_cnt, size_t board_column_cnt,
  size_t plane_row_cnt, size_t plane_column_cnt) :
  scenario_(scenario), board_row_cnt_(board_row_cnt), board_column_cnt_(board_column_cnt),
  plane_row_cnt_(plane_row_cnt), plane_column_cnt_(plane_column_cnt)
{
  // Make sure the plane and board size are what we expect...
  assert(plane_row_cnt_ == ((board_row_cnt_ - 1) * 2) + 1);
  assert(plane_column_cnt_ == ((board_column_cnt_ - 1) * 2) + 1);

  flat_vector_byte_count_ =
    plane_row_cnt_ * plane_column_cnt_ * PLANE_COUNT * sizeof(float);

  // Make a set of planes for each position on the board...
  for (size_t br = 0; br < board_row_cnt_; ++br) {
    for (size_t bc = 0; bc < board_column_cnt_; ++bc) {
      // The set of planes for [br, bc] is represented as a flat
      // vector as that is what is most efficient for feeding to the
      // model.
      FlattenedCells flat_vector(std::pmr::new_delete_resource() /* arena */);
      flat_vector.resize(flat_vector_byte_count_);
      float* flat = reinterpret_cast<float*>(flat_vector.data());

      // The planes for a board position [br, bc] includes a border
      // so that [br, bc] is the center hex in the [plane_row_cnt_,
      // plane_column_cnt_] planes.
      const size_t start_br = board_row_cnt_ - 1 - br;
      const size_t start_bc = board_column_cnt_ - 1 - bc;

      // Apply board features to the planes...
      for (auto& bd : board_cell_data) {
        if ((bd.row_ < board_row_cnt_) && (bd.column_ < board_column_cnt_)) {
          const size_t offset =
            ((start_br + bd.row_) * plane_column_cnt_ * PLANE_COUNT) +
            ((start_bc + bd.column_) * PLANE_COUNT);
          if (bd.forest_) {
            flat[offset + PLANE_IDX_FOREST] = CELL_ACTIVE;
          }
          if (bd.hill_ || bd.rocky_hill_) {
            flat[offset + PLANE_IDX_HILL] = CELL_ACTIVE;
          }

          // For small scenario we just have a single victory point
          // in the center of the board, which is added below.
          if (scenario_ != OOB::Scenario::SMALL) {
            flat[offset + PLANE_IDX_VICTORY_POINT] = bd.victory_points_;
          }
        }
      }

      // For small scenario add a single victory point in the center
      // of the board.
      if (scenario_ == OOB::Scenario::SMALL) {
        const size_t offset =
          ((start_br + (board_row_cnt_ / 2)) * plane_column_cnt_ * PLANE_COUNT) +
          ((start_bc + (board_column_cnt_ / 2)) * PLANE_COUNT);
        flat[offset + PLANE_IDX_VICTORY_POINT] = 1.0;
      }

#ifndef NDEBUG
      auto [_, ok] =
#endif
      planes_map_.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(std::make_pair(br, bc)),
        std::forward_as_tuple(std::move(flat_vector)));
      assert(ok);
    }
  }

  // Go through all board cell data again and initialize
  // 'cell_features_' and 'cell_features_offsets_.
  cell_features_.resize(board_row_cnt_ * board_column_cnt_);

  for (auto& bd : board_cell_data) {
    if ((bd.row_ < board_row_cnt_) && (bd.column_ < board_column_cnt_)) {
      const size_t offset = (bd.row_ * board_column_cnt_) + bd.column_;
      cell_features_[offset] = bd;

      // Small scenario only has victory point in center of board...
      if (scenario_ == OOB::Scenario::SMALL) {
        cell_features_[offset].victory_points_ = 0.0;
      }

      // Add cell offset feature list corresponding to a feature that
      // is present in the cell.
      if (bd.no_access_) {
        cell_features_offsets_[size_t(CellFeatureKind::NO_ACCESS)].push_back(offset);
      }
      if (bd.forest_) {
        cell_features_offsets_[size_t(CellFeatureKind::FOREST)].push_back(offset);
      }
      if (bd.hill_) {
        cell_features_offsets_[size_t(CellFeatureKind::HILL)].push_back(offset);
      }
      if (bd.rocky_hill_) {
        cell_features_offsets_[size_t(CellFeatureKind::ROCKY_HILL)].push_back(offset);
      }
      if ((scenario_ != OOB::Scenario::SMALL) && (bd.victory_points_ > 0.0)) {
        cell_features_offsets_[size_t(CellFeatureKind::VICTORY_POINT)].push_back(offset);
        assert(cell_features_offsets_[
                 size_t(CellFeatureKind::VICTORY_POINT)].size() < MAX_VICTORY_POINT_HEX_COUNT);
      }
    }
  }

  // Small scenario only has victory point in center of board...
  if (scenario_ == OOB::Scenario::SMALL) {
    const size_t offset =
      ((board_row_cnt_ / 2) * board_column_cnt_) + (board_column_cnt_ / 2);
    cell_features_[offset].victory_points_ = 1.0;
    cell_features_offsets_[size_t(CellFeatureKind::VICTORY_POINT)].push_back(offset);
    assert(cell_features_offsets_[
             size_t(CellFeatureKind::VICTORY_POINT)].size() < MAX_VICTORY_POINT_HEX_COUNT);
  }
}

const BoardCellFactory::CellFeatures&
BoardCellFactory::CellFeaturesFromIndex(CellFeatureKind kind, size_t n) const
{
  assert(n < FeaturesCount(kind));
  assert(cell_features_offsets_[size_t(kind)][n] < cell_features_.size());
  return cell_features_[cell_features_offsets_[size_t(kind)][n]];
}

const BoardCellFactory::CellFeatures&
BoardCellFactory::CellFeaturesFromCoordinates(size_t row, size_t column) const
{
  assert((row < board_row_cnt_) && (column < board_column_cnt_));
  const size_t offset = (row * board_column_cnt_) + column;
  return cell_features_[offset];
}

const BoardCellFactory::EdgeFeatures&
BoardCellFactory::EdgeFeaturesFromCoordinates(
  size_t a_row, size_t a_column, size_t b_row, size_t b_column) const
{
  assert((a_row < board_row_cnt_) && (a_column < board_column_cnt_));
  assert((b_row < board_row_cnt_) && (b_column < board_column_cnt_));

  static EdgeFeatures ee; // FIXME
  return ee;
}

const FlattenedCells&
BoardCellFactory::PlaneSet(const CellsKey key)
{
  const auto iter = planes_map_.find(key);
  THROWIF(iter == planes_map_.end(), "expected pre-initialized plane set");

  return iter->second;
}

std::string
BoardCellFactory::ToString() const
{
  std::stringstream ss;

  const size_t cplane_bytes = flat_vector_byte_count_ * planes_map_.size();
  ss
    << "constant plane sets:\n"
    << "  created " << planes_map_.size() << ", " << "memory usage " << cplane_bytes;

  return ss.str();
}

}} // namespace cifre::gettysburg
