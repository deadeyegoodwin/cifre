// Copyright 2024, David Goodwin. All rights reserved.

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>

#include "celcath_board.h"
#include "common_error.h"
#include "common_util.h"

namespace cifre { namespace celcath {

static constexpr size_t IN_A_ROW = 4;

//
// Board
//
Board::Board(
  const cifre::BoardSize2D& size, const std::vector<Player*>& players,
  size_t starting_player_id, const std::shared_ptr<Stepper>& stepper) :
  cifre::Board2D(size, players, starting_player_id),
  step_action_id_(size_.CellCount()), stepper_(stepper)
{
  // Initialize 'valid_action_ids_' and 'terminal_status_'.
  UpdateValidActions();
  UpdateTerminalStatus();
}

std::string
Board::ActionToString(const Action& action) const
{
  if ((action.Kind() == Action::ActionKind::ACTIONID) &&
      (action.ActionId() == step_action_id_)) {
    return "Evolve";
  }

  return cifre::Board2D::ActionToString(action);
}

Action
Board::StringToAction(const std::string& str) const
{
  std::string posstr(str);
  common::Trim(posstr);

  // Recognize the 'n' 1-character command. Pass all other commands to
  // cifre::Board2D.
  if ((posstr.size() == 1) && (posstr[0] == 'n')) {
    return Action(Action::ActionKind::ACTIONID, step_action_id_);
  }

  return cifre::Board2D::StringToAction(str);
}

void
Board::Update(uint64_t action_id)
{
  if (action_id == step_action_id_) {
    stepper_->Step(this, players_);
  }
  else {
  // Toggle the liveness of the appropriate cell.
    const float current = CellValueAtFlattened(action_id);
    if (current == CELL_ACTIVE_PLAYER) {
      SetCellValueAtFlattened(CELL_EMPTY, action_id);
    }
    else if (current == CELL_EMPTY) {
      SetCellValueAtFlattened(CELL_ACTIVE_PLAYER, action_id);
    }
    else {
      THROW("unexpected cell value " + std::to_string(current));
    }
  }

  // Switch board state to represent the "other" player. This also
  // calls UpdateValidActions() to get updated actions.
  SwitchActivePlayer();

  UpdateTerminalStatus();
}

void
Board::UpdateValidActions()
{
  valid_action_ids_.clear();

  // Any non-opponent cell can be toggled.
  for (size_t fidx = 0; fidx < size_.CellCount(); ++fidx) {
    if (CellValueAtFlattened(fidx) != CELL_OPPONENT_PLAYER) {
      valid_action_ids_.insert(fidx);
    }
  }

  // Can always step / evolve.
  valid_action_ids_.insert(step_action_id_);
}

void
Board::UpdateTerminalStatus()
{
  // Check winning conditions and set 'terminal_status_'.

  // FIXME for now win if have 3 more cells than opponent
  static constexpr size_t WIN_MARGIN = 3;

  size_t active_count = 0;
  size_t opponent_count = 0;
  for (size_t fidx = 0; fidx < size_.CellCount(); ++fidx) {
    const float cv = CellValueAtFlattened(fidx);
    if (cv == CELL_ACTIVE_PLAYER) {
      active_count++;
    }
    else if (cv == CELL_OPPONENT_PLAYER) {
      opponent_count++;
    }
  }

  if (active_count >= (opponent_count + WIN_MARGIN)) {
    terminal_status_ =
      cifre::StateTerminalStatus(
        cifre::StateTerminalStatus::TerminalKind::WINNER, ActivePlayerId());
  }
  else if (opponent_count >= (active_count + WIN_MARGIN)) {
    terminal_status_ =
      cifre::StateTerminalStatus(
        cifre::StateTerminalStatus::TerminalKind::WINNER, OpponentPlayerId());
  }
  else {
    // Otherwise make sure the terminal status is NON_TERMINAL.
    THROWIF(
      terminal_status_.Kind() != cifre::StateTerminalStatus::TerminalKind::NON_TERMINAL,
      "expected NON_TERMINAL status");
  }
}

float
Board::SwitchCellValue(const float v) const
{
  if (v == CELL_ACTIVE_PLAYER) {
    return CELL_OPPONENT_PLAYER;
  }
  if (v == CELL_OPPONENT_PLAYER) {
    return CELL_ACTIVE_PLAYER;
  }

  assert(v == CELL_EMPTY);
  return CELL_EMPTY;
}

void
Board::SwitchActivePlayer()
{
  // Rotate board cells 180 degrees so that opponent's home is row
  //  0. Swap cell values for players.
  size_t ridx_rot = size_.RowCount();
  for (size_t ridx = 0; ridx < (size_.RowCount() / 2); ++ridx) {
    ridx_rot -= 1;

    size_t cidx_rot = size_.ColumnCount();
    for (size_t cidx = 0; cidx < size_.ColumnCount(); ++cidx) {
      cidx_rot -= 1;
      const float t0 = CellValueAt(ridx, cidx);
      const float t1 = CellValueAt(ridx_rot, cidx_rot);
      SetCellValueAt(SwitchCellValue(t0), ridx_rot, cidx_rot);
      SetCellValueAt(SwitchCellValue(t1), ridx, cidx);
    }
  }

  // If there is an odd number of rows, then swap the middle row that
  // was not handled by the above.
  if ((size_.RowCount() % 2) != 0) {
    const size_t ridx = size_.RowCount() / 2;
    size_t cidx_rot = size_.ColumnCount();
    for (size_t cidx = 0; cidx < (size_.ColumnCount() / 2); ++cidx) {
      cidx_rot -= 1;
      const float t0 = CellValueAt(ridx, cidx);
      const float t1 = CellValueAt(ridx, cidx_rot);
      SetCellValueAt(SwitchCellValue(t0), ridx, cidx_rot);
      SetCellValueAt(SwitchCellValue(t1), ridx, cidx);
    }

    // If also an odd number of columns then need to toggle the player
    // in the very center of the board.
    if ((size_.ColumnCount() % 2) != 0) {
      const size_t ridx = size_.RowCount() / 2;
      const size_t cidx = size_.ColumnCount() / 2;
      const float t0 = CellValueAt(ridx, cidx);
      SetCellValueAt(SwitchCellValue(t0), ridx, cidx);
    }
  }

  // Update the valid actions for the now-active player.
  active_player_id_ = (active_player_id_ + 1) % players_.size();
  UpdateValidActions();
}

}} // namespace cifre::celcath
