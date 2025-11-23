// Copyright 2025, David Goodwin. All rights reserved.

#include <cctype>
#include <cstddef>
#include <cstdint>

#include "common_error.h"
#include "common_util.h"
#include "gettysburg_board_cell_factory.h"

namespace cifre { namespace gettysburg {

//
// BoardCellFactory
//
std::vector<uint8_t>
BoardCellFactory::Cells(size_t row_idx, size_t col_idx, size_t move_factor)
{
  std::vector<uint8_t> flat;
  flat.resize(row_cnt_ * column_cnt_ * PLANE_COUNT * sizeof(float));
  return flat;
}

}} // namespace cifre::gettysburg
