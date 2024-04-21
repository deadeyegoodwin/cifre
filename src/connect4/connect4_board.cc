// Copyright 2024, David Goodwin. All rights reserved.

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>

#include "connect4_board.h"
#include "common_error.h"
#include "common_util.h"

namespace cifre { namespace connect4 {

static constexpr size_t IN_A_ROW = 4;

//
// Board
//
Board::Board(const cifre::BoardSize2D& size, const std::vector<Player*>& players,
             size_t starting_player_id) :
  cifre::Board2DBasic(size, players, starting_player_id)
{
  // Initialize 'valid_action_ids_' and 'terminal_status_'.
  UpdateValidActions();
  UpdateTerminalStatus();
}

std::string
Board::ActionToString(const Action& action) const
{
  if (action.Kind() == Action::ActionKind::ACTIONID) {
    const uint64_t action_id = action.ActionId();
    const char col_ch = 'a' + action_id;
    return std::string("Column ") + col_ch;
  }

  return cifre::Board2DBasic::ActionToString(action);
}

Action
Board::StringToAction(const std::string& str) const
{
  std::string posstr(str);
  common::Trim(posstr);

  // Only 1-character commands recognized. We parse 'a', 'b', etc. as
  // column commands, and pass all other commands to
  // cifre::Board2DBasic.
  if (posstr.size() != 1) {
    return Action(Action::ActionKind::INVALID);
  }

  int32_t col_idx = posstr[0] - 'a';
  if ((col_idx >= 0) && ((size_t)col_idx < size_.ColumnCount())) {
    return Action(Action::ActionKind::ACTIONID, col_idx);
  }

  return cifre::Board2DBasic::StringToAction(str);
}

void
Board::Update(uint64_t action_id)
{
  // Occupy the lowest-row empty cell corresponding to 'action_id'
  // column.
  bool found = false;
  for (size_t ridx = 0; ridx < size_.RowCount(); ++ridx) {
    if (CellValueAt(ridx, action_id) == CELL_EMPTY) {
      SetPlayerIdAt(active_player_id_, ridx, action_id);
      found = true;
      break;
    }
  }

  THROWIF(!found, "player " + ActivePlayer()->Name() + " cannot play in full column");

  // Switch board state to represent the "other" player. This also
  // calls UpdateValidActions() to get updated actions.
  SwitchActivePlayer();

  UpdateTerminalStatus();
}

void
Board::UpdateValidActions()
{
  valid_action_ids_.clear();

  // Any non-full column can be played in.
  for (size_t cidx = 0; cidx < size_.ColumnCount(); ++cidx) {
    if (CellValueAt(size_.RowCount() - 1, cidx) == CELL_EMPTY) {
      valid_action_ids_.insert(cidx);
    }
  }
}

void
Board::UpdateTerminalStatus()
{
  // Check winning conditions and set 'terminal_status_'.

  // Check each row for 4 in-a-row. Also check to see if all cells are
  // occupied.
  size_t empty_count = 0;
  for (size_t ridx = 0; ridx < size_.RowCount(); ++ridx) {
    size_t adj_count = 0;
    size_t last_pid = PLAYER_COUNT;
    for (size_t cidx = 0; cidx < size_.ColumnCount(); ++cidx) {
      size_t pid = PlayerIdAt(ridx, cidx);
      if (pid == PLAYER_COUNT) {
        empty_count++;
        adj_count = 0; // empty cell
      }
      else if (pid == last_pid) {
        adj_count++;
        if (adj_count >= IN_A_ROW) {
          terminal_status_ =
            cifre::StateTerminalStatus(
              cifre::StateTerminalStatus::TerminalKind::WINNER, pid);
          return;
        }
      }
      else {
        adj_count = 1;
      }

      last_pid = pid;
    }
  }

  if (empty_count == 0) {
    terminal_status_ =
      cifre::StateTerminalStatus(cifre::StateTerminalStatus::TerminalKind::DRAW);
    return;
  }

  // Check each column for 4 in-a-row.
  for (size_t cidx = 0; cidx < size_.ColumnCount(); ++cidx) {
    size_t adj_count = 0;
    size_t last_pid = PLAYER_COUNT;
    for (size_t ridx = 0; ridx < size_.RowCount(); ++ridx) {
      size_t pid = PlayerIdAt(ridx, cidx);
      if (pid == PLAYER_COUNT) {
        adj_count = 0; // empty cell
      }
      else if (pid == last_pid) {
        adj_count++;
        if (adj_count >= IN_A_ROW) {
          terminal_status_ =
            cifre::StateTerminalStatus(
              cifre::StateTerminalStatus::TerminalKind::WINNER, pid);
          return;
        }
      }
      else {
        adj_count = 1;
      }

      last_pid = pid;
    }
  }

  // Down-to-right diagonals.
  for (size_t ridx = 0; ridx <= (size_.RowCount() - IN_A_ROW); ++ridx) {
    for (size_t cidx = 0; cidx <= (size_.ColumnCount() - IN_A_ROW); ++cidx) {
      size_t adj_count = 0;
      size_t last_pid = PLAYER_COUNT;
      for (size_t idx = 0; idx < IN_A_ROW; ++idx) {
        size_t pid = PlayerIdAt(ridx + idx, cidx + idx);
        if (pid == PLAYER_COUNT) {
          adj_count = 0; // empty cell
        }
        else if (pid == last_pid) {
          adj_count++;
          if (adj_count >= IN_A_ROW) {
            terminal_status_ =
              cifre::StateTerminalStatus(
                cifre::StateTerminalStatus::TerminalKind::WINNER, pid);
            return;
          }
        }
        else {
          adj_count = 1;
        }

        last_pid = pid;
      }
    }
  }

  // Up-to-right diagonals.
  for (size_t ridxp1 = size_.RowCount(); ridxp1 > (size_.RowCount() - IN_A_ROW); --ridxp1) {
    for (size_t cidx = 0; cidx <= (size_.ColumnCount() - IN_A_ROW); ++cidx) {
      size_t adj_count = 0;
      size_t last_pid = PLAYER_COUNT;
      for (size_t idx = 0; idx < IN_A_ROW; ++idx) {
        size_t pid = PlayerIdAt(ridxp1 - 1 - idx, cidx + idx);
        if (pid == PLAYER_COUNT) {
          adj_count = 0; // empty cell
        }
        else if (pid == last_pid) {
          adj_count++;
          if (adj_count >= IN_A_ROW) {
            terminal_status_ =
              cifre::StateTerminalStatus(
                cifre::StateTerminalStatus::TerminalKind::WINNER, pid);
            return;
          }
        }
        else {
          adj_count = 1;
        }

        last_pid = pid;
      }
    }
  }

  // Otherwise make sure the terminal status is NON_TERMINAL.
  THROWIF(
    terminal_status_.Kind() != cifre::StateTerminalStatus::TerminalKind::NON_TERMINAL,
    "expected NON_TERMINAL status");
}

}} // namespace cifre::connect4
