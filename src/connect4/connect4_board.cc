// Copyright 2026, David Goodwin. All rights reserved.

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
Board::Board(const cifre::MatrixSize& size, const std::vector<Player*>& players,
             size_t starting_player_id, std::pmr::memory_resource* arena) :
  cifre::Board2D(size, players, starting_player_id, arena)
{
  valid_action_ids_.reserve(TotalActionIdCount());

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

  return cifre::Board2D::ActionToString(action);
}

Action
Board::StringToAction(const std::string& str) const
{
  std::string posstr(str);
  common::Trim(posstr);

  // Only 1-character commands recognized. We parse 'a', 'b', etc. as
  // column commands, and pass all other commands to
  // cifre::Board2D.
  if (posstr.size() != 1) {
    return Action(Action::ActionKind::INVALID);
  }

  int32_t col_idx = posstr[0] - 'a';
  if ((col_idx >= 0) && ((size_t)col_idx < size_.ColumnCount())) {
    return Action(Action::ActionKind::ACTIONID, col_idx);
  }

  return cifre::Board2D::StringToAction(str);
}

StateUniquePtr
Board::Copy(std::pmr::memory_resource* arena) const
{
  void* buffer = arena->allocate(sizeof(Board), alignof(Board));
  Board* nb = new(buffer) Board(*this, arena);
  return StateUniquePtr(nb, StateDeleter<Board>(arena));
}

void
Board::Update(uint64_t action_id)
{
  // Occupy the lowest-row empty cell corresponding to 'action_id'
  // column.
  bool found = false;
  for (size_t ridx = 0; ridx < size_.RowCount(); ++ridx) {
    if (CellValueAt(ridx, action_id) == CELL_EMPTY) {
      SetCellValueAt(CELL_ACTIVE_PLAYER, ridx, action_id);
      found = true;
      break;
    }
  }

  THROWIF(!found, "player " + ActivePlayer()->Name() + " cannot play in full column");

  // Must switch active player so that the board player state is
  // correct when determining terminal status. Otherwise a winning
  // position will be incorrectly attributed.
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
      valid_action_ids_.push_back(cidx);
    }
  }

#ifndef NDEBUG
  {
    std::string dups;
    THROWIF(!ValidateUniqueActionIds(valid_action_ids_, &dups),
            "duplicate valid action IDs: " + dups);
  }
#endif
}

void
Board::SwitchActivePlayer()
{
  // Swap cell values for players.
  for (size_t fidx = 0; fidx < size_.ElementCount(); ++fidx) {
    const float cv = CellValueAtFlattened(fidx);
    if (cv != CELL_EMPTY) {
      const float ncv =
        ((cv == CELL_ACTIVE_PLAYER) ? CELL_OPPONENT_PLAYER : CELL_ACTIVE_PLAYER);
      SetCellValueAtFlattened(ncv, fidx);
    }
  }

  // Update the valid actions for the now-active player.
  active_player_id_ = (active_player_id_ + 1) % players_.size();
  UpdateValidActions();
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
    float last_cv = CELL_EMPTY;
    for (size_t cidx = 0; cidx < size_.ColumnCount(); ++cidx) {
      const float cv = CellValueAt(ridx, cidx);
      if (cv == CELL_EMPTY) {
        empty_count++;
        adj_count = 0; // empty cell
      }
      else if (cv == last_cv) {
        adj_count++;
        if (adj_count >= IN_A_ROW) {
          terminal_status_ =
            cifre::StateTerminalStatus(
              cifre::StateTerminalStatus::TerminalKind::WINNER, CellValueToPlayer(cv)->Id());
          return;
        }
      }
      else {
        adj_count = 1;
      }

      last_cv = cv;
    }
  }

  // Check each column for 4 in-a-row.
  for (size_t cidx = 0; cidx < size_.ColumnCount(); ++cidx) {
    size_t adj_count = 0;
    float last_cv = CELL_EMPTY;
    for (size_t ridx = 0; ridx < size_.RowCount(); ++ridx) {
      const float cv = CellValueAt(ridx, cidx);
      if (cv == CELL_EMPTY) {
        adj_count = 0; // empty cell
      }
      else if (cv == last_cv) {
        adj_count++;
        if (adj_count >= IN_A_ROW) {
          terminal_status_ =
            cifre::StateTerminalStatus(
              cifre::StateTerminalStatus::TerminalKind::WINNER, CellValueToPlayer(cv)->Id());
          return;
        }
      }
      else {
        adj_count = 1;
      }

      last_cv = cv;
    }
  }

  // Up-to-right diagonals.
  for (size_t ridx = 0; ridx <= (size_.RowCount() - IN_A_ROW); ++ridx) {
    for (size_t cidx = 0; cidx <= (size_.ColumnCount() - IN_A_ROW); ++cidx) {
      const float cv = CellValueAt(ridx, cidx);
      if (cv != CELL_EMPTY) {
        bool found = true;
        for (size_t idx = 1; idx < IN_A_ROW; ++idx) {
          if (cv != CellValueAt(ridx + idx, cidx + idx)) {
            found = false;
            break;
          }
        }
        if (found) {
          terminal_status_ =
            cifre::StateTerminalStatus(
              cifre::StateTerminalStatus::TerminalKind::WINNER, CellValueToPlayer(cv)->Id());
          return;
        }
      }
    }
  }

  // Up-to-left diagonals.
  for (size_t ridx = 0; ridx <= (size_.RowCount() - IN_A_ROW); ++ridx) {
    for (size_t cidx = (size_.ColumnCount() - IN_A_ROW); cidx < size_.ColumnCount(); ++cidx) {
      const float cv = CellValueAt(ridx, cidx);
      if (cv != CELL_EMPTY) {
        bool found = true;
        for (size_t idx = 1; idx < IN_A_ROW; ++idx) {
          if (cv != CellValueAt(ridx + idx, cidx - idx)) {
            found = false;
            break;
          }
        }
        if (found) {
          terminal_status_ =
            cifre::StateTerminalStatus(
              cifre::StateTerminalStatus::TerminalKind::WINNER, CellValueToPlayer(cv)->Id());
          return;
        }
      }
    }
  }

  if (empty_count == 0) {
    terminal_status_ =
      cifre::StateTerminalStatus(cifre::StateTerminalStatus::TerminalKind::DRAW);
    return;
  }

  // Otherwise make sure the terminal status is NON_TERMINAL.
  THROWIF(
    terminal_status_.Kind() != cifre::StateTerminalStatus::TerminalKind::NON_TERMINAL,
    "expected NON_TERMINAL status");
}

}} // namespace cifre::connect4
