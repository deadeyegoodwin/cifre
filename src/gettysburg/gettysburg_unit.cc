// Copyright 2024, David Goodwin. All rights reserved.

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>

#include "gettysburg_unit.h"
#include "common_error.h"
#include "common_util.h"

namespace cifre { namespace gettysburg {

//
// Unit
//
Unit::Unit(ArmyKind army_kind, size_t row_idx, size_t col_idx) :
  army_kind_(army_kind), row_idx_(row_idx), column_idx_(col_idx)
{
}

}} // namespace cifre::gettysburg
