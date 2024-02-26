// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include "common_error.h"
#include "common_logger.h"

namespace cifre { namespace celcath {

//
// Action
//
// An action.
//
class Action {
public:
  enum class ActionKind { INVALID, HELP, QUIT, SHOW_BOARD, ACTIONID };

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
  std::string ToString(common::Logger::Level filter_level = common::Logger::Level::LOW) const {
    switch(kind_) {
    case ActionKind::INVALID:
      break;
    case ActionKind::HELP:
      return "HELP";
    case ActionKind::QUIT:
      return "QUIT";
    case ActionKind::SHOW_BOARD:
      return "SHOW_BOARD";
    case ActionKind::ACTIONID:
      return "ACTIONID(" + std::to_string(action_id_) + ")";
    }

    return "INVALID";
  }

private:
  ActionKind kind_;
  uint64_t action_id_;
};

}} // namespace cifre::celcath
