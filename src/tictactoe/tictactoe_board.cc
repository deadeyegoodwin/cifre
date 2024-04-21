// Copyright 2024, David Goodwin. All rights reserved.

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
Board::Board(const cifre::BoardSize2D& size, const std::vector<Player*>& players,
             size_t starting_player_id) :
  cifre::Board2DBasic(size, players, starting_player_id)
{
  // Initialize 'valid_action_ids_' and 'terminal_status_'.
  UpdateValidActions();
  UpdateTerminalStatus();
}

void
Board::Update(uint64_t action_id)
{
  // Occupy the cell corresponding to 'action_id'.
  const cifre::Player* current = PlayerAtFlattened(action_id);
  if (current == nullptr) {
    SetPlayerIdAtFlattened(active_player_id_, action_id);
  }
  else {
    THROW("player " + ActivePlayer()->Name() + " cannot occupy cell owned by " +
          current->Name());
  }

  // Board was changed, so check to see if there is a change in the
  // terminal status of the board.
  UpdateTerminalStatus();

  // Switch board state to represent the "other" player.
  SwitchActivePlayer();
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
      size_t pid = PlayerIdAt(ridx, cidx);
      if (pid == ActivePlayerId()) {
        active_count++;
      }
      else if (pid == OpponentPlayerId()) {
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
      size_t pid = PlayerIdAt(ridx, cidx);
      if (pid == ActivePlayerId()) {
        active_count++;
      }
      else if (pid == OpponentPlayerId()) {
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
    size_t pid = PlayerIdAt(didx, didx);
    if (pid == ActivePlayerId()) {
      active_count++;
    }
    else if (pid == OpponentPlayerId()) {
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
    size_t pid = PlayerIdAt(size_.RowCount() - 1 - didx, didx);
    if (pid == ActivePlayerId()) {
      active_count++;
    }
    else if (pid == OpponentPlayerId()) {
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

}} // namespace cifre::tictactoe
