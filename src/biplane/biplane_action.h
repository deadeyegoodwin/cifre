// Copyright 2026, David Goodwin. All rights reserved.
#pragma once

#include <cstdint>
#include <string>

#include "cifre_vec.h"

namespace cifre { namespace biplane {

static constexpr size_t TOTAL_ACTIONID_COUNT = 18;

// Return string representation of an action-id.
std::string ActionIdToString(uint64_t aid);

// Return action-id corresponding to a string. Returns
// TOTAL_ACTIONID_COUNT if 'str' is not a known action.
uint64_t StringToActionId(const std::string& str);

// Return the position delta and orientation delta for the mode
// represented by action-id, 'aid'.
void ActionIdToMove(
  uint64_t aid, MatrixCoordinate& pos_delta, int64_t& orientation_delta);

}} // namespace cifre::biplane
