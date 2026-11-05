// Copyright 2026, David Goodwin. All rights reserved.

#include <tuple>
#include "biplane_action.h"
#include "biplane_board.h"
#include "common_util.h"

namespace cifre { namespace biplane {

#if 0  // set to 1 for debugging actions

const std::array<std::tuple<std::string, MatrixCoordinate, int64_t>,
                 TOTAL_ACTIONID_COUNT> action_data_ {{
    // name                       [y, x]  dir
  { std::string("0"),  {0, 0}, 0 },
  { std::string("N"),  {1, 0}, 0 },
  { std::string("NE"), {1, 1}, 0 },
  { std::string("E"),  {0, 1}, 0 },
  { std::string("SE"), {-1, 1}, 0 },
  { std::string("S"),  {-1, 0}, 0 },
  { std::string("SW"), {-1, -1}, 0 },
  { std::string("W"),  {0, -1}, 0 },
  { std::string("NW"), {1, -1}, 0 },

  { std::string("CW"),  {0, 0}, 1 },
  { std::string("CCW"), {0, 0}, -1 },
  { std::string(""), {1, 0}, -2 },
  { std::string(""), {1, 0}, 2 },

  { std::string(""), {2, 0}, 0 },
  { std::string(""), {2, -1}, -1 },
  { std::string(""), {2, 1}, 1 },
  { std::string(""), {2, -1}, -2 },
  { std::string(""), {2, 1}, 3 }
}};

#else

const std::array<std::tuple<std::string, MatrixCoordinate, int64_t>,
                 TOTAL_ACTIONID_COUNT> action_data_ {{
    // name                       [y, x]  dir
  { std::string("SLOW-STRAIGHT"), {1, 0}, 0 },
  { std::string("SLOW-45-LEFT"), {1, -1}, -1 },
  { std::string("SLOW-45-RIGHT"), {1, 1}, 1 },
  { std::string("SLOW-90-LEFT"), {1, -1}, -2 },
  { std::string("SLOW-90-RIGHT"), {1, 1}, 2 },
  { std::string("SLOW-135-LEFT"), {1, -1}, -3 },
  { std::string("SLOW-135-RIGHT"), {1, 1}, 3 },

  { std::string("SLOW-45-LSLIP"), {1, 0}, -1 },
  { std::string("SLOW-45-RSLIP"), {1, 0}, 1 },
  { std::string("SLOW-90-LSLIP"), {1, 0}, -2 },
  { std::string("SLOW-90-RSLIP"), {1, 0}, 2 },

  { std::string("MED-STRAIGHT"), {2, 0}, 0 },
  { std::string("MED-45-LEFT"), {2, -1}, -1 },
  { std::string("MED-45-RIGHT"), {2, 1}, 1 },
  { std::string("MED-90-LEFT"), {2, -1}, -2 },
  { std::string("MED-90-RIGHT"), {2, 1}, 2 },
  { std::string("MED-135-LEFT"), {2, -1}, -3 },
  { std::string("MED-135-RIGHT"), {2, 1}, 3 }
}};

#endif

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
  assert(aid < TOTAL_ACTIONID_COUNT);
  pos_delta = std::get<1>(action_data_[aid]);
  orientation_delta = std::get<2>(action_data_[aid]);
}

}} // namespace cifre::biplane
