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
// Player::PlayerKind
//
Player::PlayerKind
Player::StringToPlayerKind(const std::string& str)
{
  if (str == "human") {
    return PlayerKind::HUMAN;
  }
  if (str == "random") {
    return PlayerKind::RANDOM;
  }
  if (str == "alphazero") {
    return PlayerKind::ALPHAZERO;
  }
  if (str == "perfect") {
    return PlayerKind::PERFECT;
  }

  return PlayerKind::UNKNOWN;
}

std::string
Player::PlayerKindToString(const PlayerKind kind)
{
  switch (kind) {
  case PlayerKind::UNKNOWN: return "<unknown>";
  case PlayerKind::HUMAN: return "human";
  case PlayerKind::RANDOM: return "random";
  case PlayerKind::ALPHAZERO: return "alphazero";
  case PlayerKind::PERFECT: return "perfect";
  }

  return "<none>"; // not reached
}

//
// Player
//
Player*
Player::Create(
  const std::shared_ptr<common::Logger>& logger,
  const Player::PlayerKind kind, const std::string& name, const char ch,
  const std::shared_ptr<AlphaZero>& alphazero)
{
  switch (kind) {
  case Player::PlayerKind::RANDOM:
    assert(alphazero == nullptr);
    return new RandomPlayer(logger, name, ch);

  case Player::PlayerKind::ALPHAZERO:
    assert(alphazero != nullptr);
    return new AlphaZeroPlayer(logger, name, ch, alphazero);

  default:
    break;
  }

  return nullptr;
}

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
// AlphaZeroPlayer
//
AlphaZeroPlayer::AlphaZeroPlayer(
  const std::shared_ptr<common::Logger>& logger,
  const std::string& name, char ch,
  const std::shared_ptr<cifre::AlphaZero>& alphazero) :
  Player(logger, name, ch), alphazero_(alphazero)
{
}

Action
AlphaZeroPlayer::NextAction(const State& state, size_t max_game_turns_remaining) const
{
  logger_->LogMed(state.ToString());

  const auto& actionIds = state.ValidActionIds();
  THROWIF(actionIds.empty(), "player '" + name_ + "' has no valid action to choose from");

  uint64_t action_id = alphazero_->NextActionId(state, max_game_turns_remaining);
  Action action(Action::ActionKind::ACTIONID, action_id);
  THROWIF(!state.IsValidAction(action),
          "alpha-zero selected invalid action " + action.ToString());

  LOGGER_LOGMED(*logger_, Name() + " (" + Char() + "): " + state.ActionToString(action));

  return action;
}

//
// RandomPlayer
//
RandomPlayer::RandomPlayer(const std::shared_ptr<common::Logger>& logger,
                           const std::string& name, char ch):
  Player(logger, name, ch)
{
}

Action
RandomPlayer::NextAction(const State& state, size_t max_game_turns_remaining) const
{
  logger_->LogMed(state.ToString());

  // Pick a random action from the valid actions. At least one action
  // must be valid.
  const auto& actionIds = state.ValidActionIds();
  THROWIF(actionIds.empty(), "player '" + name_ + "' has no valid action to choose from");

  uint64_t action_id =
    *std::next(actionIds.begin(), rnd_(uint64_t(0), uint64_t(actionIds.size() - 1)));
  Action action(Action::ActionKind::ACTIONID, action_id);
  LOGGER_LOGMED(*logger_, Name() + " (" + Char() + "): " + state.ActionToString(action));
  return action;
}

} // namespace cifre
