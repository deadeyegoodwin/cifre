// Copyright 2026, David Goodwin. All rights reserved.

#include <cstddef>
#include <cstdint>
#include <string>
#include <iostream>
#include <sstream>

#include "gettysburg_board.h"
#include "gettysburg_player.h"
#include "common_error.h"

namespace cifre { namespace gettysburg {

//
// HumanPlayer
//
HumanPlayer::HumanPlayer(const std::shared_ptr<Logger>& logger,
                         size_t id, const std::string& name) :
  cifre::Player(logger, id, name)
{
}

std::string
HumanPlayer::Usage() const
{
  std::stringstream sstr;
  sstr << Name() << "\n";
  sstr << "'h' : help\n";
  sstr << "'q' : quit\n";
  sstr << "'s' : show board\n";
  sstr << "'t' : toggle board size\n";
  sstr << "'v' : toggle verbose board\n";
  return sstr.str();
}

Action
HumanPlayer::NextAction(const cifre::State& state, size_t max_game_turns_remaining) const
{
  const Board& board = dynamic_cast<const Board&>(state);

  LOG_GAME(*logger_, "");
  LOG_GAME(*logger_, board.ToString());

  THROWIF(board.ValidActionIds().empty(),
          "player '" + name_ + "' has no valid action to choose from");

  while (true) {
    LOG_GAME_IE(*logger_, Name()  + " (" + Char() + "): ", 0 /* indent */, false /* endline */);

    std::string line;
    std::getline(std::cin, line);
    Action action = board.StringToAction(line);
    if (!board.IsValidAction(action)) {
      action = Action(Action::ActionKind::INVALID);
    }

    switch (action.Kind()) {
    case Action::ActionKind::INVALID:
      LOG_GAME(*logger_, "invalid action");
      LOG_GAME(*logger_, Usage());
      break;

    case Action::ActionKind::HELP:
      LOG_GAME(*logger_, Usage());
      break;

    case Action::ActionKind::SHOW_STATE:
      LOG_GAME(*logger_, board.ToString());
      break;

    case Action::ActionKind::CUSTOM_0: // toggle board display size
      if (board.Display() == Board::DisplayKind::SMALL) {
        board.SetDisplay(Board::DisplayKind::LARGE);
      }
      else {
        board.SetDisplay(Board::DisplayKind::SMALL);
      }
      LOG_GAME(*logger_, board.ToString());
      break;

    case Action::ActionKind::CUSTOM_1: // toggle verbose board display
      if (board.Display() == Board::DisplayKind::LARGE) {
        board.SetDisplay(Board::DisplayKind::LARGE_VERBOSE);
      }
      else if (board.Display() == Board::DisplayKind::LARGE_VERBOSE) {
        board.SetDisplay(Board::DisplayKind::LARGE);
      }
      LOG_GAME(*logger_, board.ToString());
     break;

    case Action::ActionKind::CUSTOM_2:
    case Action::ActionKind::CUSTOM_3:
      break;

    case Action::ActionKind::QUIT:
    case Action::ActionKind::ACTIONID:
      return action;
    }
  }

  return Action(Action::ActionKind::INVALID);
}

}} // namespace cifre::gettysburg
