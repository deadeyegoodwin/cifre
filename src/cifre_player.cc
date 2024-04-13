// Copyright 2024, David Goodwin. All rights reserved.

#include <cstddef>
#include <cstdint>
#include <string>
#include <iostream>
#include <sstream>

#include "cifre_player.h"
#include "common_error.h"

namespace cifre {

//
// Player
//
Player::Player(const std::shared_ptr<common::Logger>& logger,
               const std::string& name, char ch) :
  logger_(logger), name_(name), ch_(ch)
{
}

Action
Player::NextAction(const State& state, size_t max_game_turns_remaining) const
{
  // Base Player class is not capable of choosing an action.
  return Action(Action::ActionKind::INVALID);
}

std::string
Player::ToString(common::Logger::Level filter_level) const
{
  std::stringstream sstr;
  sstr << ch_;
  return sstr.str();
}

//
// AlgoPlayer
//
AlgoPlayer::AlgoPlayer(
  const std::shared_ptr<common::Logger>& logger,
  const std::string& name, char ch,
  const std::shared_ptr<cifre::Algo>& algo) :
  Player(logger, name, ch), algo_(algo)
{
}

Action
AlgoPlayer::NextAction(const State& state, size_t max_game_turns_remaining) const
{
  logger_->LogMed(state.ToString());

  const auto& actionIds = state.ValidActionIds();
  THROWIF(actionIds.empty(), "player '" + name_ + "' has no valid action to choose from");

  uint64_t action_id = algo_->NextActionId(state, max_game_turns_remaining);
  Action action(Action::ActionKind::ACTIONID, action_id);
  THROWIF(!state.IsValidAction(action),
          "player '" + Name() + "' selected invalid action " + action.ToString());

  LOGGER_LOGMED(*logger_, Name() + " (" + Char() + "): " + state.ActionToString(action));

  return action;
}

} // namespace cifre
