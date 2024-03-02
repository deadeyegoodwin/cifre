// Copyright 2024, David Goodwin. All rights reserved.

#include <cstddef>
#include <cstdint>
#include <string>
#include <iostream>
#include <sstream>

#include "celcath_board.h"
#include "celcath_player.h"
#include "common_error.h"

namespace cifre { namespace celcath {

//
// Player
//
Player Player::DEAD("dead", ' ', 0);
Player Player::LIVE("live", 'O', 1);

Player::Player(const std::string& name, char board_char, int32_t neighbor_value) :
  name_(name), board_char_(board_char), neighbor_value_(neighbor_value)
{
}

Action
Player::NextAction(const Board& board) const
{
  // Base Player class is not capable of choosing an action.
  return Action(Action::ActionKind::INVALID);
}

std::string
Player::ToString(common::Logger::Level filter_level) const
{
  std::stringstream sstr;
  sstr << board_char_;
  return sstr.str();
}

//
// HumanPlayer
//
HumanPlayer::HumanPlayer(const std::string& name, char board_char, int32_t neighbor_value) :
  Player(name, board_char, neighbor_value)
{
}

std::string
HumanPlayer::Usage() const
{
  std::stringstream sstr;
  sstr << "'h' : help\n";
  sstr << "'q' : quit\n";
  sstr << "'b' : show board\n";
  sstr << "'e' : end turn\n";
  sstr << "'<r><c>' : toggle cell [r, c]";
  return sstr.str();
}

Action
HumanPlayer::NextAction(const Board& board) const
{
  const auto& actionIds = board.ValidActionIds();
  THROWIF(actionIds.empty(), "player '" + name_ + "' has no valid action to choose from");

  while (true) {
    std::cout
      << Name() << " [" << (board.CompletedActionsThisTurn() + 1)
      << " of " << board.MaxActionsThisTurn() << "]: ";

    std::string line;
    std::getline(std::cin, line);
    Action action = board.StringToAction(line);
    if (!board.IsValidAction(action)) {
      action = Action(Action::ActionKind::INVALID);
    }

    switch (action.Kind()) {
    case Action::ActionKind::INVALID:
      std::cout << "invalid action\n";
      std::cout << Usage() << std::endl;
      break;

    case Action::ActionKind::HELP:
      std::cout << Usage() << std::endl;
      break;

    case Action::ActionKind::SHOW_BOARD:
      std::cout << board.ToString() << '\n';
      break;

    case Action::ActionKind::QUIT:
    case Action::ActionKind::ACTIONID:
      return action;
    }
  }

  return Action(Action::ActionKind::INVALID);
}

//
// AlphaZeroPlayer
//
AlphaZeroPlayer::AlphaZeroPlayer(
  const std::string& name, char board_char, int32_t neighbor_value,
  std::unique_ptr<cifre::AlphaZero> alpha_zero) :
  Player(name, board_char, neighbor_value), alpha_zero_(std::move(alpha_zero))
{
}

Action
AlphaZeroPlayer::NextAction(const Board& board) const
{
  std::cout
    << Name() << " [" << (board.CompletedActionsThisTurn() + 1)
    << " of " << board.MaxActionsThisTurn() << "]: ";

  const auto& actionIds = board.ValidActionIds();
  THROWIF(actionIds.empty(), "player '" + name_ + "' has no valid action to choose from");

  uint64_t action_id = alpha_zero_->NextActionId(board.TritonRepresentation(), board.ValidActionIds());
  Action action = board.ActionIdToAction(action_id);

  // FIXME, if action is invalid just choose first one available...
  if (!board.IsValidAction(action)) {
    action_id = *actionIds.begin();
    action = board.ActionIdToAction(action_id);
  }
  // FIXME end

  std::cout << board.ActionToString(action) << '\n';
  return action;
}

//
// RandomPlayer
//
RandomPlayer::RandomPlayer(const std::string& name, char board_char, int32_t neighbor_value):
  Player(name, board_char, neighbor_value)
{
}

Action
RandomPlayer::NextAction(const Board& board) const
{
  std::cout
    << Name() << " [" << (board.CompletedActionsThisTurn() + 1)
    << " of " << board.MaxActionsThisTurn() << "]: ";

  // Pick a random action from the valid actions. At least one action
  // must be valid.
  const auto& actionIds = board.ValidActionIds();
  THROWIF(actionIds.empty(), "player '" + name_ + "' has no valid action to choose from");

  uint64_t action_id =
    *std::next(actionIds.begin(), rnd_(uint64_t(0), uint64_t(actionIds.size() - 1)));
  Action action = board.ActionIdToAction(action_id);
  std::cout << board.ActionToString(action) << '\n';

  return action;
}

}} // namespace cifre::celcath
