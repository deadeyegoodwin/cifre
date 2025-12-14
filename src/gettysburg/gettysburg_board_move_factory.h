// Copyright 2025, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "gettysburg_oob.h"

namespace cifre { namespace gettysburg {

//
// BoardMoveFactory
//
class BoardMoveFactory {
public:
  BoardMoveFactory(
    const OOB::Scenario scenario, size_t row_cnt, size_t column_cnt);

private:
  BoardMoveFactory(const BoardMoveFactory&) = delete;
  BoardMoveFactory& operator=(const BoardMoveFactory&) = delete;

  const OOB::Scenario scenario_;
  size_t row_cnt_;
  size_t column_cnt_;
};

}} // namespace cifre::gettysburg
