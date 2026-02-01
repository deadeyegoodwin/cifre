// Copyright 2026, David Goodwin. All rights reserved.

#include <set>
#include "cifre_state.h"

namespace cifre {

bool
State::ValidateUniqueActionIds(
  const std::pmr::vector<uint64_t>& vaids, std::string* dup_str) const
{
  std::set<uint64_t> vaid_set;
  std::set<uint64_t> nonunique_vaids;

  for (auto aid : vaids) {
    if (vaid_set.find(aid) != vaid_set.end()) {
      nonunique_vaids.insert(aid);
    }
    else {
      vaid_set.insert(aid);
    }
  }

  if (dup_str != nullptr) {
    bool first = true;
    for (auto aid : nonunique_vaids) {
      if (!first) {
        *dup_str += ", ";
      }

      *dup_str += ActionToString(Action(Action::ActionKind::ACTIONID, aid));
      first = false;
    }
  }

  return nonunique_vaids.empty();
}

} // namespace cifre
