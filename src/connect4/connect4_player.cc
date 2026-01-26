// Copyright 2026, David Goodwin. All rights reserved.

#include <cstddef>
#include <cstdint>
#include <string>
#include <iostream>
#include <sstream>

#include "connect4_board.h"
#include "connect4_player.h"
#include "common_error.h"

namespace cifre { namespace connect4 {

//
// HumanPlayer
//
std::string
HumanPlayer::Usage() const
{
  std::stringstream sstr;
  sstr << Name() << "- Drop a piece into any non-full column. Get 4 in a row to win.\n";
  sstr << "'h' : help\n";
  sstr << "'q' : quit\n";
  sstr << "'s' : show board\n";
  sstr << "'z' : suggest action with alphazero\n";
  sstr << "'<column>' : the column identifier";
  return sstr.str();
}

Action
HumanPlayer::NextAction(const cifre::State& state, size_t max_game_sideturns_remaining) const
{
  const Board& board = dynamic_cast<const Board&>(state);

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

    case Action::ActionKind::QUERY_ALPHAZERO:
      if (algo_ == nullptr) {
        LOG_GAME(*logger_, "no alphazero model specified, use --alphazero-model");
      }
      else {
        AlphaZeroQuery(state, max_game_sideturns_remaining);
      }
      break;

    case Action::ActionKind::CUSTOM_0:
    case Action::ActionKind::CUSTOM_1:
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

}} // namespace cifre::connect4
