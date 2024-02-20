// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include "common_error.h"

namespace cifre { namespace celcath {

//
// Action
//
// An action.
//
class Action {
public:
  enum class ActionKind { INVALID, HELP, QUIT, SHOW_BOARD, ACTIONID };

  // Create action that doesn't require an action identifier.
  Action(ActionKind kind) : kind_(kind), action_id_(0) {
    THROWIF(RequiresActionId(), "action kind requires action-id");
  }

  // Create action that requires an action identifier. 'action_id' is
  // a numeric identifier of type uint64_t in the range [0,
  // total-action-id-count).
  Action(ActionKind kind, uint64_t action_id) : kind_(kind), action_id_(action_id) {
    THROWIF(!RequiresActionId(), "action kind must not specify action-id");
  }

  ActionKind Kind() const { return kind_; }
  bool RequiresActionId() const { return (kind_ == ActionKind::ACTIONID); }
  uint64_t ActionId() const {
    THROWIF(!RequiresActionId(), "action does not have action-id");
    return action_id_;
  }

private:
  ActionKind kind_;
  uint64_t action_id_;
};

}} // namespace cifre::celcath
