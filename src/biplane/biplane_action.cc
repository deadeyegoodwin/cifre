// Copyright 2026, David Goodwin. All rights reserved.

#include <tuple>
#include "biplane_action.h"
#include "biplane_board.h"
#include "common_util.h"

namespace cifre { namespace biplane {

const std::array<std::tuple<std::string, MatrixCoordinate, int64_t>,
                 TOTAL_ACTIONID_COUNT> action_data_ {{
  { std::string("SLOW-STRAIGHT"), {0, 1}, 0 },
  { std::string("SLOW-45-LEFT"), {-1, 1}, -1 },
  { std::string("SLOW-45-RIGHT"), {1, 1}, 1 },
  { std::string("SLOW-135-LEFT"), {-1, 1}, -3 },
  { std::string("SLOW-135-RIGHT"), {1, 1}, 3 },

  { std::string("MED-STRAIGHT"), {0, 2}, 0 },
  { std::string("MED-45-LEFT"), {-1, 2}, -1 },
  { std::string("MED-45-RIGHT"), {1, 2}, 1 },
  { std::string("MED-135-LEFT"), {-1, 2}, -3 },
  { std::string("MED-135-RIGHT"), {1, 2}, 3 }
}};

std::string
ActionIdToString(uint64_t aid)
{
  assert(aid < TOTAL_ACTIONID_COUNT);
  return std::get<0>(action_data_[aid]);
}

uint64_t
StringToActionId(const std::string& str)
{
  std::string lstr(str);
  common::ToUpper(lstr);
  for (size_t idx = 0; idx < action_data_.size(); ++idx) {
    if (lstr == std::get<0>(action_data_[idx])) {
      return idx;
    }
  }

  return TOTAL_ACTIONID_COUNT;
}

void
ActionIdToMove(
  uint64_t aid, MatrixCoordinate& pos_delta, int64_t& orientation_delta)
{
}

}} // namespace cifre::biplane
