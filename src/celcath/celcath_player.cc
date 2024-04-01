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
Player::Player(const std::shared_ptr<common::Logger>& logger,
               const std::string& name, char board_char, int32_t neighbor_value) :
  logger_(logger), name_(name), board_char_(board_char), neighbor_value_(neighbor_value)
{
}

cifre::Action
Player::NextAction(const Board& board, size_t max_game_turns_remaining) const
{
  // Base Player class is not capable of choosing an action.
  return cifre::Action(cifre::Action::ActionKind::INVALID);
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
HumanPlayer::HumanPlayer(const std::shared_ptr<common::Logger>& logger,
                         const std::string& name, char board_char, int32_t neighbor_value) :
  Player(logger, name, board_char, neighbor_value)
{
}

std::string
HumanPlayer::Usage() const
{
  std::stringstream sstr;
  sstr << "'h' : help\n";
  sstr << "'q' : quit\n";
  sstr << "'b' : show board\n";
  sstr << "'s' : step board\n";
  sstr << "'e' : end turn\n";
  sstr << "'<r><c>' : toggle cell [r, c]";
  return sstr.str();
}

cifre::Action
HumanPlayer::NextAction(const Board& board, size_t max_game_turns_remaining) const
{
  const auto& actionIds = board.ValidActionIds();
  THROWIF(actionIds.empty(), "player '" + name_ + "' has no valid action to choose from");

  while (true) {
    logger_->LogMed(
      Name() + " [" + std::to_string(board.CompletedActionsThisTurn() + 1) +
      " of " + std::to_string(board.MaxActionsThisTurn()) + "]: ",
      0 /* indent */, false /* endline */);

    std::string line;
    std::getline(std::cin, line);
    cifre::Action action = board.StringToAction(line);
    if (!board.IsValidAction(action)) {
      action = cifre::Action(cifre::Action::ActionKind::INVALID);
    }

    switch (action.Kind()) {
    case cifre::Action::ActionKind::INVALID:
      logger_->LogMed("invalid action");
      logger_->LogMed(Usage());
      break;

    case cifre::Action::ActionKind::HELP:
      logger_->LogMed(Usage());
      break;

    case cifre::Action::ActionKind::SHOW_BOARD:
      logger_->LogMed(board.ToString());
      break;

    case cifre::Action::ActionKind::QUIT:
    case cifre::Action::ActionKind::ACTIONID:
      return action;
    }
  }

  return cifre::Action(cifre::Action::ActionKind::INVALID);
}

//
// AlphaZeroPlayer
//
AlphaZeroPlayer::AlphaZeroPlayer(
  const std::shared_ptr<common::Logger>& logger,
  const std::string& name, char board_char, int32_t neighbor_value,
  const std::shared_ptr<cifre::AlphaZero>& alphazero) :
  Player(logger, name, board_char, neighbor_value), alphazero_(alphazero)
{
}

void
AlphaZeroPlayer::GameStart()
{
  alphazero_->GameStart();
}

void
AlphaZeroPlayer::GameEnd(const cifre::State* terminal_state, cifre::State::TerminalStatusKind status)
{
  THROWIF(status == cifre::State::TerminalStatusKind::NON_TERMINAL,
          "game ended with NON-TERMINAL status");
  alphazero_->GameEnd(terminal_state, status);
}

cifre::Action
AlphaZeroPlayer::NextAction(const Board& board, size_t max_game_turns_remaining) const
{
  const auto& actionIds = board.ValidActionIds();
  THROWIF(actionIds.empty(), "player '" + name_ + "' has no valid action to choose from");

  uint64_t action_id = alphazero_->NextActionId(board, max_game_turns_remaining);
  cifre::Action action = board.ActionIdToAction(action_id);
  THROWIF(!board.IsValidAction(action),
          "alpha-zero selected invalid action " + action.ToString());

  LOGGER_LOGMED(*logger_,
                Name() + " [" + std::to_string(board.CompletedActionsThisTurn() + 1) +
                " of " + std::to_string(board.MaxActionsThisTurn()) + "]: " +
                board.ActionToString(action));

  return action;
}

//
// RandomPlayer
//
RandomPlayer::RandomPlayer(const std::shared_ptr<common::Logger>& logger,
                           const std::string& name, char board_char, int32_t neighbor_value):
  Player(logger, name, board_char, neighbor_value)
{
}

cifre::Action
RandomPlayer::NextAction(const Board& board, size_t max_game_turns_remaining) const
{
  // Pick a random action from the valid actions. At least one action
  // must be valid.
  const auto& actionIds = board.ValidActionIds();
  THROWIF(actionIds.empty(), "player '" + name_ + "' has no valid action to choose from");

  uint64_t action_id =
    *std::next(actionIds.begin(), rnd_(uint64_t(0), uint64_t(actionIds.size() - 1)));
  cifre::Action action = board.ActionIdToAction(action_id);

  LOGGER_LOGMED(*logger_,
                Name() + " [" + std::to_string(board.CompletedActionsThisTurn() + 1) +
                " of " + std::to_string(board.MaxActionsThisTurn()) + "]: " +
                board.ActionToString(action));

  return action;
}

}} // namespace cifre::celcath
