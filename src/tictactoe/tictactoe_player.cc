// Copyright 2024, David Goodwin. All rights reserved.

#include <cstddef>
#include <cstdint>
#include <string>
#include <iostream>
#include <sstream>

#include "tictactoe_board.h"
#include "tictactoe_player.h"
#include "common_error.h"

namespace cifre { namespace tictactoe {

//
// HumanPlayer
//
HumanPlayer::HumanPlayer(const std::shared_ptr<common::Logger>& logger,
                         const std::string& name, char ch, int32_t neighbor_value) :
  cifre::Player(logger, name, ch, neighbor_value)
{
}

std::string
HumanPlayer::Usage() const
{
  std::stringstream sstr;
  sstr << "tic-tac-toe - Occupy one cell each turn. Get three in a row before your opponent.\n";
  sstr << "'h' : help\n";
  sstr << "'q' : quit\n";
  sstr << "'s' : show board\n";
  sstr << "'<row><column>' : occupy cell [row, column]";
  return sstr.str();
}

Action
HumanPlayer::NextAction(const cifre::State& state, size_t max_game_turns_remaining) const
{
  const Board& board = dynamic_cast<const Board&>(state);

  logger_->LogMed(board.ToString());

  const auto& actionIds = board.ValidActionIds();
  THROWIF(actionIds.empty(), "player '" + name_ + "' has no valid action to choose from");

  while (true) {
    logger_->LogMed(Name()  + " (" + Char() + "): ", 0 /* indent */, false /* endline */);

    std::string line;
    std::getline(std::cin, line);
    Action action = board.StringToAction(line);
    if (!board.IsValidAction(action)) {
      action = Action(Action::ActionKind::INVALID);
    }

    switch (action.Kind()) {
    case Action::ActionKind::INVALID:
      logger_->LogMed("invalid action");
      logger_->LogMed(Usage());
      break;

    case Action::ActionKind::HELP:
      logger_->LogMed(Usage());
      break;

    case Action::ActionKind::SHOW_STATE:
      logger_->LogMed(board.ToString());
      break;

    case Action::ActionKind::QUIT:
    case Action::ActionKind::ACTIONID:
      return action;
    }
  }

  return Action(Action::ActionKind::INVALID);
}

}} // namespace cifre::tictactoe
