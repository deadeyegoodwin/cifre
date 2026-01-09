// Copyright 2026, David Goodwin. All rights reserved.

#include "gettysburg_action.h"

namespace cifre { namespace gettysburg {

std::string
ActionIdToString(uint64_t aid)
{
  assert(aid < TOTAL_ACTIONID_COUNT);
  return HexDirectionToString(HexDirection(aid));
}

}} // namespace cifre::gettysburg
