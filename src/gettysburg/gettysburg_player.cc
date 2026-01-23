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
  sstr << "'v' : toggle board version\n";
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
    if (logger_->IsEnabled(cifre::Logger::Kind::GAME)) {
      LOG_GAME_IE(*logger_, "Game actions: ", 0 /* indent */, false /* endline */);
      bool first = true;
      for (auto vid : board.ValidActionIds()) {
        LOG_GAME_IE(
          *logger_,
          ((first) ? "" : ", ") + board.ActionToString(Action(Action::ActionKind::ACTIONID, vid)),
          0 /* indent */, false /* endline */);
        first = false;
      }
      LOG_GAME_IE(
        *logger_, "\n" + Name()  + " (" + Char() + "): ", 0 /* indent */, false /* endline */);
    }

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
      if (board.DisplayKind() == Board::BoardDisplayKind::SMALL) {
        board.SetDisplay(Board::BoardDisplayKind::LARGE, board.DisplayVersion());
      }
      else {
        board.SetDisplay(Board::BoardDisplayKind::SMALL, board.DisplayVersion());
      }
      LOG_GAME(*logger_, board.ToString());
      break;

    case Action::ActionKind::CUSTOM_1: { // toggle board display version
      const uint32_t nv =
        (board.DisplayVersion() + 1) % Board::BOARD_DISPLAY_VERSION_COUNT;
      board.SetDisplay(board.DisplayKind(), nv);
      LOG_GAME(*logger_, board.ToString());
      break;
    }

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
