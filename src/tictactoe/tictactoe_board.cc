// Copyright 2025, David Goodwin. All rights reserved.

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>

#include "tictactoe_board.h"
#include "common_error.h"
#include "common_util.h"

namespace cifre { namespace tictactoe {

//
// Board
//
Board::Board(const cifre::Size2D& size, const std::vector<Player*>& players,
             size_t starting_player_id) :
  cifre::Board2D(size, players, starting_player_id)
{
  // Initialize 'valid_action_ids_' and 'terminal_status_'.
  UpdateValidActions();
  UpdateTerminalStatus();
}

void
Board::Update(uint64_t action_id)
{
  // Occupy the cell corresponding to 'action_id'.
  const float cv = CellValueAtFlattened(action_id);
  if (cv == CELL_EMPTY) {
    SetCellValueAtFlattened(CELL_ACTIVE_PLAYER, action_id);
  }
  else {
    THROW("player " + ActivePlayer()->Name() + " cannot occupy cell owned by " +
          PlayerAtFlattened(action_id)->Name());
  }

  // Check terminal status and if not at terminal then advance to next
  // active player.
  UpdateTerminalStatus();
  if (terminal_status_.Kind() == cifre::StateTerminalStatus::TerminalKind::NON_TERMINAL) {
    // Switch board state to represent the "other" player. This also
    // calls UpdateValidActions() to get updated actions.
    SwitchActivePlayer();
  }
}

void
Board::UpdateValidActions()
{
  // Valid action for any empty cell.
  valid_action_ids_.clear();

  for (size_t fidx = 0; fidx < size_.CellCount(); ++fidx) {
    if (CellValueAtFlattened(fidx) == CELL_EMPTY) {
      valid_action_ids_.insert(fidx);
    }
  }
}

void
Board::UpdateTerminalStatus()
{
  // Check winning conditions and set 'terminal_status_'.

  // Check each row for complete ownership.
  for (size_t ridx = 0; ridx < size_.RowCount(); ++ridx) {
    size_t active_count = 0;
    size_t opponent_count = 0;
    for (size_t cidx = 0; cidx < size_.ColumnCount(); ++cidx) {
      const float cv = CellValueAt(ridx, cidx);
      if (cv == CELL_ACTIVE_PLAYER) {
        active_count++;
      }
      else if (cv == CELL_OPPONENT_PLAYER) {
        opponent_count++;
      }
    }

    if (active_count == size_.ColumnCount()) {
      terminal_status_ =
        cifre::StateTerminalStatus(
          cifre::StateTerminalStatus::TerminalKind::WINNER, ActivePlayerId());
      return;
    }
    if (opponent_count == size_.ColumnCount()) {
      terminal_status_ =
        cifre::StateTerminalStatus(
          cifre::StateTerminalStatus::TerminalKind::WINNER, OpponentPlayerId());
      return;
    }
  }

  // Check each row for column ownership.
  for (size_t cidx = 0; cidx < size_.ColumnCount(); ++cidx) {
    size_t active_count = 0;
    size_t opponent_count = 0;
    for (size_t ridx = 0; ridx < size_.RowCount(); ++ridx) {
      const float cv = CellValueAt(ridx, cidx);
      if (cv == CELL_ACTIVE_PLAYER) {
        active_count++;
      }
      else if (cv == CELL_OPPONENT_PLAYER) {
        opponent_count++;
      }
    }

    if (active_count == size_.RowCount()) {
      terminal_status_ =
        cifre::StateTerminalStatus(
          cifre::StateTerminalStatus::TerminalKind::WINNER, ActivePlayerId());
      return;
    }
    if (opponent_count == size_.RowCount()) {
      terminal_status_ =
        cifre::StateTerminalStatus(
          cifre::StateTerminalStatus::TerminalKind::WINNER, OpponentPlayerId());
      return;
    }
  }

  // Check diagonal
  size_t active_count = 0;
  size_t opponent_count = 0;
  for (size_t didx = 0; didx < size_.RowCount(); ++didx) {
    const float cv = CellValueAt(didx, didx);
    if (cv == CELL_ACTIVE_PLAYER) {
      active_count++;
    }
    else if (cv == CELL_OPPONENT_PLAYER) {
      opponent_count++;
    }
  }

  if (active_count == size_.RowCount()) {
    terminal_status_ =
      cifre::StateTerminalStatus(
        cifre::StateTerminalStatus::TerminalKind::WINNER, ActivePlayerId());
    return;
  }
  if (opponent_count == size_.RowCount()) {
    terminal_status_ =
      cifre::StateTerminalStatus(
        cifre::StateTerminalStatus::TerminalKind::WINNER, OpponentPlayerId());
    return;
  }

  // Check other diagonal
  active_count = 0;
  opponent_count = 0;
  for (size_t didx = 0; didx < size_.RowCount(); ++didx) {
    const float cv = CellValueAt(size_.RowCount() - 1 - didx, didx);
    if (cv == CELL_ACTIVE_PLAYER) {
      active_count++;
    }
    else if (cv == CELL_OPPONENT_PLAYER) {
      opponent_count++;
    }
  }

  if (active_count == size_.RowCount()) {
    terminal_status_ =
      cifre::StateTerminalStatus(
        cifre::StateTerminalStatus::TerminalKind::WINNER, ActivePlayerId());
    return;
  }
  if (opponent_count == size_.RowCount()) {
    terminal_status_ =
      cifre::StateTerminalStatus(
        cifre::StateTerminalStatus::TerminalKind::WINNER, OpponentPlayerId());
    return;
  }

  // If all cells are occupied, then draw.
  size_t occupied_count = 0;
  for (size_t fidx = 0; fidx < size_.CellCount(); ++fidx) {
    if (CellValueAtFlattened(fidx) != CELL_EMPTY) {
      occupied_count++;
    }
  }

  if (occupied_count == size_.CellCount()) {
    terminal_status_ =
      cifre::StateTerminalStatus(cifre::StateTerminalStatus::TerminalKind::DRAW);
    return;
  }

  // Otherwise make sure the terminal status is NON_TERMINAL.
  THROWIF(
    terminal_status_.Kind() != cifre::StateTerminalStatus::TerminalKind::NON_TERMINAL,
    "expected NON_TERMINAL status");
}

void
Board::SwitchActivePlayer()
{
  // Swap cell values for players.
  for (size_t fidx = 0; fidx < size_.CellCount(); ++fidx) {
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

}} // namespace cifre::tictactoe
