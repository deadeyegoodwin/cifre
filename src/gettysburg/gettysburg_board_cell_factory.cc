// Copyright 2026, David Goodwin. All rights reserved.

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
  const OOB::Scenario scenario, cifre::MatrixSize board_size) :
  scenario_(scenario), board_size_(board_size)
{
  // The input planes are based on the size of the board. Because the
  // model is trained/used by always having the unit currently being
  // controlled at the center of the plane, there must be a border
  // around the board to allow any actual map hex to be at the center
  // of the plane (the width of the border on any size will vary
  // depending on the center hex, from 0 to 'plane_border_size_').
  // Also, to make sure even/odd hex position differences don't
  // interfere with the board display, the width of the border must be
  // even number of columns.
  plane_border_size_ =
    cifre::MatrixSize(board_size_.RowCount() - 1, board_size_.ColumnCount() - 1);
  if ((plane_border_size_.ColumnCount() % 2) == 1) {
    plane_border_size_ =
      cifre::MatrixSize(plane_border_size_.RowCount(), plane_border_size_.ColumnCount() + 1);
  }
  plane_size_ =
    cifre::MatrixSize(plane_border_size_.RowCount() + board_size_.RowCount(),
                      plane_border_size_.ColumnCount() + board_size_.ColumnCount());

  flat_vector_byte_count_ =
    plane_size_.ElementCount() * PLANE_COUNT * sizeof(float);

  // Make a set of planes for each position on the board...
  for (size_t br = 0; br < board_size_.RowCount(); ++br) {
    for (size_t bc = 0; bc < board_size_.ColumnCount(); ++bc) {
      // The set of planes for [br, bc] is represented as a flat
      // vector as that is what is most efficient for feeding to the
      // model.
      FlattenedCells flat_vector(std::pmr::new_delete_resource() /* arena */);
      flat_vector.resize(flat_vector_byte_count_);
      float* flat = reinterpret_cast<float*>(flat_vector.data());

      // The planes for a board position [br, bc] includes a border so
      // that [br, bc] is the center hex in the 'plane_size_' planes.
      const size_t start_br = plane_border_size_.RowCount() - br;
      const size_t start_bc = plane_border_size_.ColumnCount() - bc;

      // Apply board features to the planes...
      for (auto& bd : board_cell_data) {
        if ((bd.row_ < board_size_.RowCount()) && (bd.column_ < board_size_.ColumnCount())) {
          const size_t offset =
            ((start_br + bd.row_) * plane_size_.ColumnCount() * PLANE_COUNT) +
            ((start_bc + bd.column_) * PLANE_COUNT);
          if (bd.forest_) {
            flat[offset + PLANE_IDX_FOREST] = CELL_ACTIVE;
          }
          if (bd.hill_) {
            flat[offset + PLANE_IDX_HILL] = CELL_ACTIVE;
          }
          if (bd.rocky_hill_) {
            flat[offset + PLANE_IDX_ROCKY_HILL] = CELL_ACTIVE;
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
          ((start_br + (board_size_.RowCount() / 2)) * plane_size_.ColumnCount() * PLANE_COUNT) +
          ((start_bc + (board_size_.ColumnCount() / 2)) * PLANE_COUNT);
        flat[offset + PLANE_IDX_VICTORY_POINT] = 1.0;
      }

#ifndef NDEBUG
      auto [_, ok] =
#endif
      planes_map_.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(br, bc),
        std::forward_as_tuple(std::move(flat_vector)));
      assert(ok);
    }
  }

  // Go through all board cell data again and initialize
  // 'cell_features_' and 'cell_features_offsets_.
  cell_features_.resize(board_size_.ElementCount());

  for (auto& bd : board_cell_data) {
    if ((bd.row_ < board_size_.RowCount()) && (bd.column_ < board_size_.ColumnCount())) {
      const size_t offset = (bd.row_ * board_size_.ColumnCount()) + bd.column_;
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
                 size_t(CellFeatureKind::VICTORY_POINT)].size() <= MAX_VICTORY_POINT_HEX_COUNT);
      }
    }
  }

  // Small scenario only has victory point in center of board...
  if (scenario_ == OOB::Scenario::SMALL) {
    const size_t offset =
      ((board_size_.RowCount() / 2) * board_size_.ColumnCount()) + (board_size_.ColumnCount() / 2);
    cell_features_[offset].victory_points_ = 1.0;
    cell_features_offsets_[size_t(CellFeatureKind::VICTORY_POINT)].push_back(offset);
    assert(cell_features_offsets_[
             size_t(CellFeatureKind::VICTORY_POINT)].size() <= MAX_VICTORY_POINT_HEX_COUNT);
  }

  // Go through board edge data and initialize edge features.
  for (auto& be : board_edge_data) {
    if ((be.row_ < board_size_.RowCount()) && (be.column_ < board_size_.ColumnCount())) {
      // Initialize in cell explicitly listed in 'be'.
      const size_t offset = (be.row_ * board_size_.ColumnCount()) + be.column_;
      auto& cf = cell_features_[offset];
      assert(!cf.edges_[size_t(be.dir_)].road_ && !cf.edges_[size_t(be.dir_)].stream_ &&
             !cf.edges_[size_t(be.dir_)].railroad_); // make sure not already inited
      cf.edges_[size_t(be.dir_)] = be;

      // Initialize in cell on other side of edge.
      auto other_coord = HexDirectionToPosition(MatrixCoordinate(be.row_, be.column_), be.dir_);
      if ((other_coord.Row() >= 0) && (other_coord.Column() >= 0) &&
          (other_coord.Row() < int64_t(board_size_.RowCount())) &&
          (other_coord.Column() < int64_t(board_size_.ColumnCount()))) {
        const size_t other_offset =
          (other_coord.Row() * board_size_.ColumnCount()) + other_coord.Column();
        HexDirection other_dir = HexOppositeDirection(be.dir_);
        auto& ocf = cell_features_[other_offset];
        assert(!ocf.edges_[size_t(other_dir)].road_ && !ocf.edges_[size_t(other_dir)].stream_ &&
               !ocf.edges_[size_t(other_dir)].railroad_); // make sure not already inited
        ocf.edges_[size_t(other_dir)] = be;
      }
    }
  }

  // Go through all cells and mark inaccessible board edges as those
  // that are adjacent to inaccessible cells on both sides.
  for (size_t br = 0; br < board_size_.RowCount(); ++br) {
    for (size_t bc = 0; bc < board_size_.ColumnCount(); ++bc) {
      const size_t offset = (br * board_size_.ColumnCount()) + bc;
      auto& cf = cell_features_[offset];
      if (cf.not_accessible_) {
        for (size_t idx = HEX_RADIUS1_FIRST_DIRECTION_IDX; idx <= HEX_RADIUS1_LAST_DIRECTION_IDX; ++idx) {
          auto other_pos = HexDirectionToPosition(MatrixCoordinate(br, bc), HexDirection(idx));
          if ((other_pos.Row() >= 0) && (other_pos.Column() >= 0) &&
              (other_pos.Row() < int64_t(board_size_.RowCount())) &&
              (other_pos.Column() < int64_t(board_size_.ColumnCount()))) {
            const size_t other_offset = (other_pos.Row() * board_size_.ColumnCount()) + other_pos.Column();
            auto& ocf = cell_features_[other_offset];
            if (ocf.not_accessible_) {
              HexDirection other_dir = HexOppositeDirection(HexDirection(idx));
              cf.edges_[size_t(idx)].not_accessible_ = true;
              ocf.edges_[size_t(other_dir)].not_accessible_ = true;
            }
          }
          else {
            cf.edges_[size_t(idx)].not_accessible_ = true;
          }
        }
      }
    }
  }
}

const BoardCellFactory::CellFeatures&
BoardCellFactory::IndexToCellFeatures(CellFeatureKind kind, size_t n) const
{
  assert(n < CellFeaturesCount(kind));
  assert(cell_features_offsets_[size_t(kind)][n] < cell_features_.size());
  return cell_features_[cell_features_offsets_[size_t(kind)][n]];
}

const BoardCellFactory::CellFeatures&
BoardCellFactory::PositionToCellFeatures(const MatrixCoordinate& pos) const
{
  assert((pos.Row() >= 0) && (pos.Row() < int64_t(board_size_.RowCount())) &&
         (pos.Column() >= 0) && (pos.Column() < int64_t(board_size_.ColumnCount())));
  const size_t offset = (pos.Row() * board_size_.ColumnCount()) + pos.Column();
  return cell_features_[offset];
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
