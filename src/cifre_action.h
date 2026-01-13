// Copyright 2026, David Goodwin. All rights reserved.
#pragma once

#include <cstdint>
#include "common_error.h"

namespace cifre {

//
// Action
//
// An action. Represents both out-of-game actions, like "quit" and
// in-game actions represented by action IDs.
//
class Action {
public:
  enum class ActionKind {
    INVALID, HELP, QUIT, SHOW_STATE,
    CUSTOM_0, CUSTOM_1, CUSTOM_2, CUSTOM_3,
    ACTIONID };

public:
  // Create action that doesn't require an action identifier.
  Action(ActionKind kind) : kind_(kind), action_id_(0) {
    THROWIF(RequiresActionId(), "action " + ToString() + " requires action-id");
  }

  // Create action that requires an action identifier. 'action_id' is
  // a numeric identifier of type uint64_t in the range [0,
  // total-action-id-count).
  Action(ActionKind kind, uint64_t action_id) : kind_(kind), action_id_(action_id) {
    THROWIF(!RequiresActionId(), "action " + ToString() + " must not specify action-id");
  }

  ActionKind Kind() const { return kind_; }
  bool RequiresActionId() const { return (kind_ == ActionKind::ACTIONID); }
  uint64_t ActionId() const {
    THROWIF(!RequiresActionId(), "action " + ToString() + " does not have action-id");
    return action_id_;
  }

  // Return string representation of the player.
  std::string ToString() const;

private:
  ActionKind kind_;
  uint64_t action_id_;
};

} // namespace cifre
