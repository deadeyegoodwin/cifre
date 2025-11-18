// Copyright 2024, David Goodwin. All rights reserved.

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>

#include "gettysburg_board.h"
#include "common_error.h"
#include "common_util.h"

namespace cifre { namespace gettysburg {

// Gettysburg required 3 input planes.
#define PLANE_COUNT 3

//
// Board
//
Board::Board(const cifre::BoardSize2D& size, const std::vector<Player*>& players,
             size_t starting_player_id) :
  cifre::Board2DHex(PLANE_COUNT, size, players, starting_player_id)
{
  // Initialize 'valid_action_ids_' and 'terminal_status_'.
//  UpdateValidActions();
//  UpdateTerminalStatus();
}

size_t
Board::TotalActionIdCount() const
{
  return 1;
}

std::string
Board::ActionToString(const Action& action) const
{
  if (action.Kind() != Action::ActionKind::ACTIONID) {
    return action.ToString();
  }

  //const uint64_t action_id = action.ActionId();
  THROW("FIXME");
  return "<unreachable>";
}

Action
Board::StringToAction(const std::string& str) const
{
  std::string posstr(str);
  common::Trim(posstr);

  // Parse 1 character strings
  if (posstr.size() == 1) {
    switch (posstr[0]) {
    case 'h':
    case '?': return Action(Action::ActionKind::HELP);
    case 'q': return Action(Action::ActionKind::QUIT);
    case 's': return Action(Action::ActionKind::SHOW_STATE);
    default:
      break;
    }
  }

  return Action(Action::ActionKind::INVALID);
}

void
Board::Update(uint64_t action_id)
{
  UpdateValidActions();
}

void
Board::UpdateValidActions()
{
  valid_action_ids_.clear();
}

void
Board::UpdateTerminalStatus()
{
}

}} // namespace cifre::gettysburg
