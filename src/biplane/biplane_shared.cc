// Copyright 2026, David Goodwin. All rights reserved.

#include <cassert>
#include <tuple>
#include "biplane_shared.h"
#include "cifre_std.h"

namespace cifre { namespace biplane {

class BiplaneActionInfo {
public:
  BiplaneActionInfo(
    const std::string n, const MatrixCoordinate pdelta, const int64_t odelta) :
    name(n), position_delta(pdelta), orientation_delta(odelta) { }

  const std::string name;
  const MatrixCoordinate position_delta;
  const int64_t orientation_delta;
};


const cifre::array<BiplaneActionInfo, TOTAL_ACTIONID_COUNT> action_info_ {
    // name                                          [y, x]  dir
  BiplaneActionInfo { std::string("SLOW-STRAIGHT"), {1, 0}, 0 },
  BiplaneActionInfo { std::string("SLOW-45-LEFT"), {1, -1}, -1 },
  BiplaneActionInfo { std::string("SLOW-45-RIGHT"), {1, 1}, 1 },
  BiplaneActionInfo { std::string("SLOW-90-LEFT"), {1, -1}, -2 },
  BiplaneActionInfo { std::string("SLOW-90-RIGHT"), {1, 1}, 2 },
  BiplaneActionInfo { std::string("SLOW-135-LEFT"), {1, -1}, -3 },
  BiplaneActionInfo { std::string("SLOW-135-RIGHT"), {1, 1}, 3 },

  BiplaneActionInfo { std::string("SLOW-45-LSLIP"), {1, 0}, -1 },
  BiplaneActionInfo { std::string("SLOW-45-RSLIP"), {1, 0}, 1 },
  BiplaneActionInfo { std::string("SLOW-90-LSLIP"), {1, 0}, -2 },
  BiplaneActionInfo { std::string("SLOW-90-RSLIP"), {1, 0}, 2 },

  BiplaneActionInfo { std::string("MED-STRAIGHT"), {2, 0}, 0 },
  BiplaneActionInfo { std::string("MED-45-LEFT"), {2, -1}, -1 },
  BiplaneActionInfo { std::string("MED-45-RIGHT"), {2, 1}, 1 },
  BiplaneActionInfo { std::string("MED-90-LEFT"), {2, -1}, -2 },
  BiplaneActionInfo { std::string("MED-90-RIGHT"), {2, 1}, 2 },
  BiplaneActionInfo { std::string("MED-135-LEFT"), {2, -1}, -3 },
  BiplaneActionInfo { std::string("MED-135-RIGHT"), {2, 1}, 3 }
};

std::string
ActionIdToString(uint64_t aid)
{
  assert(aid < TOTAL_ACTIONID_COUNT);
  return action_info_[aid].name;
}

uint64_t
StringToActionId(const std::string& str)
{
  for (size_t idx = 0; idx < action_info_.size(); ++idx) {
    if (str == action_info_[idx].name) {
      return idx;
    }
  }

  return TOTAL_ACTIONID_COUNT;
}

void
ActionIdToDelta(
  uint64_t aid, MatrixCoordinate& pos_delta, int64_t& orientation_delta)
{
  assert(aid < TOTAL_ACTIONID_COUNT);
  pos_delta = action_info_[aid].position_delta;
  orientation_delta = action_info_[aid].orientation_delta;
}

}} // namespace cifre::biplane
