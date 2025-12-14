// Copyright 2025, David Goodwin. All rights reserved.

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <iostream>

#include "common_error.h"
#include "gettysburg_board_move_factory.h"

namespace cifre { namespace gettysburg {

//
// BoardMoveFactory
//
BoardMoveFactory::BoardMoveFactory(
  const OOB::Scenario scenario, size_t row_cnt, size_t column_cnt) :
  scenario_(scenario), row_cnt_(row_cnt), column_cnt_(column_cnt)
{
}

}} // namespace cifre::gettysburg
