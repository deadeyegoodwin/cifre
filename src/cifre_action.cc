// Copyright 2024, David Goodwin. All rights reserved.

#include "cifre_action.h"
#include "common_error.h"
#include "common_logger.h"

namespace cifre {

//
// Action
//

std::string
Action::ToString(common::Logger::Level filter_level) const
{
  switch(kind_) {
  case ActionKind::INVALID:
    break;
  case ActionKind::HELP:
    return "HELP";
  case ActionKind::QUIT:
    return "QUIT";
  case ActionKind::SHOW_STATE:
    return "SHOW-STATE";
  case ActionKind::SHOW_DEBUG_STATE:
    return "SHOW-DEBUG-STATE";
  case ActionKind::ACTIONID:
    return "ACTIONID(" + std::to_string(action_id_) + ")";
  }

  return "INVALID";
}

} // namespace cifre
