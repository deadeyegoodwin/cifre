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

  // Make a set of planes for each position and possible move-factor
  // on the board...
  for (size_t br = 0; br < board_row_cnt_; ++br) {
    for (size_t bc = 0; bc < board_column_cnt_; ++bc) {
      for (size_t mf = 0; mf < 1; ++mf) {  // FIXME
        // The set of planes for [br, bc] is represented as a flat
        // vector as that is what is most efficient for feeding to the
        // model.
        Board2D::FlattenedCells flat_vector;
        flat_vector.resize(flat_vector_byte_count_);
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

        planes_map_[std::make_tuple(br, bc, mf)] = std::move(flat_vector);
      }
    }
  }
}

Board2D::FlattenedCells
BoardCellFactory::AcquireCells(CellsKey key)
{
  key = std::make_tuple(std::get<0>(key), std::get<1>(key), 0); // move-factor FIXME

  acquired_plane_sets_ += 1;

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
  created_plane_sets_ += 1;

  return ps;
}

void
BoardCellFactory::ReleaseCells(CellsKey key, Board2D::FlattenedCells&& cells)
{
  key = std::make_tuple(std::get<0>(key), std::get<1>(key), 0); // move-factor FIXME

  released_plane_sets_ += 1;

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

std::string
BoardCellFactory::ToString() const
{
  std::stringstream ss;

  const size_t cplane_bytes = flat_vector_byte_count_ * planes_map_.size();
  ss
    << "constant plane sets:\n"
    << "    created " << planes_map_.size() << ", " << "memory usage " << cplane_bytes << "\n";

  size_t aplane_bytes = 0;
  for (const auto& pr : alloc_planes_map_) {
    aplane_bytes += pr.second.size();
  }
  aplane_bytes *= flat_vector_byte_count_;
  ss
    << "allocated plane sets:\n"
    << "    created " << created_plane_sets_ << ", " << "memory usage " << aplane_bytes << "\n"
    << "    acquired " << acquired_plane_sets_ << ", " << "released " << released_plane_sets_;

  size_t max_per = 0;
  CellsKey max_key;
  for (const auto& pr : alloc_planes_map_) {
    if (pr.second.size() > max_per) {
      max_per = pr.second.size();
      max_key = pr.first;
    }
  }

  if (max_per > 0) {
    ss
      << "\n    per-state maximum " << max_per << " for ["
      << std::get<0>(max_key) << ","
      << std::get<1>(max_key) << ","
      << std::get<2>(max_key) << "]";
  }

  return ss.str();
}

}} // namespace cifre::gettysburg
