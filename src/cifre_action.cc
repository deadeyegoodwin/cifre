// Copyright 2025, David Goodwin. All rights reserved.

#include "cifre_action.h"
#include "common_error.h"

namespace cifre {

//
// Action
//

std::string
Action::ToString() const
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
  case Action::ActionKind::CUSTOM_0:
    return "CUSTOM-0";
  case Action::ActionKind::CUSTOM_1:
    return "CUSTOM-1";
  case Action::ActionKind::CUSTOM_2:
    return "CUSTOM-2";
  case Action::ActionKind::CUSTOM_3:
    return "CUSTOM-3";
  case ActionKind::ACTIONID:
    return "ACTIONID(" + std::to_string(action_id_) + ")";
  }

  return "INVALID";
}

} // namespace cifre
