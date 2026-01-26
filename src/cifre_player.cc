// Copyright 2026, David Goodwin. All rights reserved.

#include <cstddef>
#include <cstdint>
#include <string>
#include <iostream>
#include <sstream>

#include "cifre_algo_alphazero.h"
#include "cifre_player.h"
#include "common_error.h"

namespace cifre {

//
// Player
//
Player::Player(const std::shared_ptr<Logger>& logger,
               size_t id, const std::string& name) :
  logger_(logger), id_(id), name_(name), ch_('-')
{
}

Action
Player::NextAction(const State& state, size_t max_game_sideturns_remaining) const
{
  // Base Player class is not capable of choosing an action.
  return Action(Action::ActionKind::INVALID);
}

std::string
Player::ToString() const
{
  std::stringstream sstr;
  sstr << ch_;
  return sstr.str();
}

//
// HumanPlayer
//
HumanPlayer::HumanPlayer(
  const std::shared_ptr<Logger>& logger, size_t id, const std::string& name,
  const std::shared_ptr<cifre::Algo>& algo) :
  cifre::Player(logger, id, name), algo_(algo)
{
}

void
HumanPlayer::AlphaZeroQuery(const State& state, size_t max_game_sideturns_remaining) const
{
  assert(algo_ != nullptr);

  std::shared_ptr<AlphaZeroAlgo> az_algo =
    std::dynamic_pointer_cast<AlphaZeroAlgo>(algo_);

  if (az_algo != nullptr) {
    // Turn on logging to see more details about alpha-zero decision.
    bool algo_enabled = az_algo->MutableLogger()->IsEnabled(Logger::Kind::ALGO);
    if (!algo_enabled) {
      az_algo->MutableLogger()->Enable(Logger::Kind::ALGO);
    }

    uint64_t action_id = az_algo->NextActionId(state, max_game_sideturns_remaining);
    Action action(Action::ActionKind::ACTIONID, action_id);

    if (!algo_enabled) {
      az_algo->MutableLogger()->Disable(Logger::Kind::ALGO);
    }

    THROWIF(
      !state.IsValidAction(action), "player '" + Name() +
      "', alphazero selected invalid action " + action.ToString());

    LOG_GAME(*logger_, "alphazero action: " + state.ActionToString(action));
  }
}

//
// AlgoPlayer
//
AlgoPlayer::AlgoPlayer(
  const std::shared_ptr<Logger>& logger,
  size_t id, const std::string& name, const std::shared_ptr<cifre::Algo>& algo) :
  Player(logger, id, name), algo_(algo)
{
}

Action
AlgoPlayer::NextAction(const State& state, size_t max_game_sideturns_remaining) const
{
  LOG_GAME(*logger_, state.ToString());

  THROWIF(state.ValidActionIds().empty(),
          "player '" + name_ + "' has no valid action to choose from");

  uint64_t action_id = algo_->NextActionId(state, max_game_sideturns_remaining);
  Action action(Action::ActionKind::ACTIONID, action_id);
  THROWIF(!state.IsValidAction(action),
          "player '" + Name() + "' selected invalid action " + action.ToString());

  LOG_GAME(*logger_, Name() + " (" + Char() + "): " + state.ActionToString(action));

  return action;
}

} // namespace cifre
