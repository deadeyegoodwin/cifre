// Copyright 2025, David Goodwin. All rights reserved.

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <iostream>

#include "common_error.h"
#include "common_util.h"
#include "gettysburg_board_cell_factory.h"
#include "gettysburg_board_cell_data.h"

namespace cifre { namespace gettysburg {

//
// BoardCellFactoryStatistics
//
std::string
BoardCellFactoryStatistics::ToString() const
{
  std::stringstream ss;
  ss
    << "plane sets: created " << created_plane_sets_ << ", "
    << "acquired " << acquired_plane_sets_ << ", "
    << "released " << released_plane_sets_;

  return ss.str();
}

//
// BoardCellFactory
//
BoardCellFactory::BoardCellFactory(
  size_t board_row_cnt, size_t board_column_cnt, size_t plane_row_cnt, size_t plane_column_cnt) :
  board_row_cnt_(board_row_cnt), board_column_cnt_(board_column_cnt),
  plane_row_cnt_(plane_row_cnt), plane_column_cnt_(plane_column_cnt)
{
  // Make sure the plane and board size are what we expect...
  assert(plane_row_cnt_ == ((board_row_cnt_ - 1) * 2) + 1);
  assert(plane_column_cnt_ == ((board_column_cnt_ - 1) * 2) + 1);

  // Make a set of planes for each position and possible move-factor
  // on the board...
  for (size_t br = 0; br < board_row_cnt_; ++br) {
    for (size_t bc = 0; bc < board_column_cnt_; ++bc) {
      for (size_t mf = 0; mf < 1; ++mf) {  // FIXME
        // The set of planes for [br, bc] is represented as a flat
        // vector as that is what is most efficient for feeding to the
        // model.
        Board2D::FlattenedCells flat_vector;
        flat_vector.resize(plane_row_cnt_ * plane_column_cnt_ * PLANE_COUNT * sizeof(float));
        float* flat = reinterpret_cast<float*>(flat_vector.data());

        // The planes for a board position [br, bc] includes a border
        // so that [br, bc] is the center hex in the [plane_row_cnt_,
        // plane_column_cnt_] planes.
        const size_t start_br = board_row_cnt_ - 1 - br;
        const size_t start_bc = board_column_cnt_ - 1 - bc;

        // Apply board features to the planes...
        for (auto& bd : board_cell_data) {
          if ((bd.row_idx_ < board_row_cnt_) && (bd.column_idx_ < board_column_cnt_)) {
            const size_t offset =
              ((start_br + bd.row_idx_) * plane_column_cnt_ * PLANE_COUNT) +
              ((start_bc + bd.column_idx_) * PLANE_COUNT);
            if (bd.forest_) {
              flat[offset + PLANE_IDX_FOREST] = CELL_ACTIVE;
            }
            if (bd.hill_) {
              flat[offset + PLANE_IDX_HILL] = CELL_ACTIVE;
            }
            flat[offset + PLANE_IDX_GOAL] = bd.victory_points_;
          }
        }

        planes_map_[std::make_tuple(br, bc, mf)] = std::move(flat_vector);
      }
    }
  }
}

Board2D::FlattenedCells
BoardCellFactory::AcquireCells(CellsKey key)
{
  key = std::make_tuple(std::get<0>(key), std::get<1>(key), 0); // move-factor FIXME

  auto& stats = Statistics();
  stats.acquired_plane_sets_ += 1;

  // First check if there is a plane-set already available for
  // immediate use.
  {
    std::unique_lock<std::mutex> lk(alloc_planes_mu_);
    const auto iter = alloc_planes_map_.find(key);
    if ((iter != alloc_planes_map_.end()) && !iter->second.empty()) {
      Board2D::FlattenedCells ps = std::move(iter->second.back());
      iter->second.pop_back();
      return ps;
    }
  }

  // No existing plane-set so copy from the pre-initialized original
  // (which must exist).
  const auto iter = planes_map_.find(key);
  THROWIF(iter == planes_map_.end(), "expected pre-initialized plane set");
  Board2D::FlattenedCells ps;
  ps.CopyFrom(iter->second);
  stats.created_plane_sets_ += 1;

  return ps;
}

void
BoardCellFactory::ReleaseCells(CellsKey key, Board2D::FlattenedCells&& cells)
{
  key = std::make_tuple(std::get<0>(key), std::get<1>(key), 0); // move-factor FIXME

  auto& stats = Statistics();
  stats.released_plane_sets_ += 1;

  // When debugging, check that the returned cells match what was
  // originally acquired.
#ifndef NDEBUG
  const auto iter = planes_map_.find(key);
  assert(iter != planes_map_.end());
  assert(cells == iter->second);
#endif

  // Save 'cells' so that it is available for reuse.
  {
    std::unique_lock<std::mutex> lk(alloc_planes_mu_);
    auto pr = alloc_planes_map_.try_emplace(key);
    pr.first->second.emplace_back(std::move(cells));
  }
}

}} // namespace cifre::gettysburg
