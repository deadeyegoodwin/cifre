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

//
// Board
//
Board::Board(
  const cifre::BoardSize2D& size, const std::vector<Player*>& players,
  size_t starting_player_id, const std::shared_ptr<Stepper>& stepper) :
  cifre::Board2D(size, players, starting_player_id),
  step_action_id_(size_.CellCount()), stepper_(stepper)
{
  cells_flattened_.resize(size_.CellCount() * sizeof(float), CELL_EMPTY);
  cells_ = reinterpret_cast<float*>(cells_flattened_.data());

  // Initialize 'valid_action_ids_' and 'terminal_status_'.
  UpdateValidActions();
  UpdateTerminalStatus();
}

Player*
Board::PlayerAt(size_t row_idx, size_t col_idx) const
{
  // For now we assume 2 player so that we can determine the player
  // from the cell value.
  assert(players_.size() == 2);

  const float cv = CellValueAt(row_idx, col_idx);
  if (cv > 0.0) {
    return ActivePlayer();
  }
  else if (cv < 0.0) {
    return players_[(active_player_id_ == 0) ? 1 : 0];
  }

  return nullptr;
}

Player*
Board::PlayerAtFlattened(size_t flat_idx) const
{
  // For now we assume 2 player so that we can determine the player
  // from the cell value.
  assert(players_.size() == 2);

  const float cv = CellValueAtFlattened(flat_idx);
  if (cv > 0.0) {
    return ActivePlayer();
  }
  else if (cv < 0.0) {
    return players_[(active_player_id_ == 0) ? 1 : 0];
  }

  return nullptr;
}

std::string
Board::ActionToString(const Action& action) const
{
  if (action.Kind() != Action::ActionKind::ACTIONID) {
    return action.ToString();
  }

  const uint64_t action_id = action.ActionId();

  if ((action.Kind() == Action::ActionKind::ACTIONID) &&
      (action_id == step_action_id_)) {
    return "Step";
  }

  // Get cell specifier for action. Cell specifier must be a row
  // number (1 based) followed by a column letter ('a' based).
  const size_t row_idx = action_id / size_.ColumnCount();
  const size_t col_idx = action_id % size_.ColumnCount();
  const char row_ch = '1' + row_idx;
  const char col_ch = 'a' + col_idx;
  return std::string("BUILD [") + row_ch + "," + col_ch + "]";
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
    case 'n': return Action(Action::ActionKind::ACTIONID, step_action_id_);
    default:
      break;
    }
  }

  // Parse 2 character strings. Cell specifier must be a row number (1
  // based) followed by a column letter ('a' based).
  if (posstr.size() == 2) {
    int32_t row_idx = posstr[0] - '1';
    int32_t col_idx = posstr[1] - 'a';
    if ((col_idx >= 0) && ((size_t)col_idx < size_.ColumnCount()) &&
        (row_idx >= 0) && ((size_t)row_idx < size_.RowCount())) {
      return Action(Action::ActionKind::ACTIONID,
                    (row_idx * size_.ColumnCount()) + col_idx);
    }
  }

  return Action(Action::ActionKind::INVALID);
}

void
Board::Update(uint64_t action_id)
{
  if (action_id == step_action_id_) {
    stepper_->Step(this, players_);
  }
  else {
    // Increment selected cell value.
    const float cv = CellValueAtFlattened(action_id);
    if ((cv >= CELL_EMPTY) && (cv < CELL_LG)) {
      SetCellValueAtFlattened(cv + 1.0, action_id);
    }
    else {
      THROW("unexpected cell value " + std::to_string(cv));
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

  // Empty cells that are manhattan adjacent to currently occupied
  // cells can be played.
  for (size_t ridx = 0; ridx < size_.RowCount(); ++ridx) {
    for (size_t cidx = 0; cidx < size_.ColumnCount(); ++cidx) {
      const size_t fidx = (ridx * size_.ColumnCount()) + cidx;
      const float cv = CellValueAtFlattened(fidx);
      if (cv > CELL_EMPTY) {
        if (cv < CELL_LG) {
          valid_action_ids_.insert(fidx);
        }
        // Left
        if ((cidx > 0) && (CellValueAtFlattened(fidx - 1) == CELL_EMPTY)) {
          valid_action_ids_.insert(fidx - 1);
        }
        // Right
        if ((cidx < (size_.ColumnCount() - 1)) && (CellValueAtFlattened(fidx + 1) == CELL_EMPTY)) {
          valid_action_ids_.insert(fidx + 1);
        }
        // Up
        if ((ridx > 0) && (CellValueAtFlattened(fidx - size_.RowCount()) == CELL_EMPTY)) {
          valid_action_ids_.insert(fidx - size_.RowCount());
        }
        // Down
        if ((ridx < (size_.RowCount() - 1)) &&
            (CellValueAtFlattened(fidx + size_.RowCount()) == CELL_EMPTY)) {
          valid_action_ids_.insert(fidx + size_.RowCount());
        }
      }
    }
  }

  // If there are no valid actions (nothing yet built), then can play
  // on home row only.
  if (valid_action_ids_.empty()) {
    for (size_t fidx = 0; fidx < size_.ColumnCount(); ++fidx) {
      const float cv = CellValueAtFlattened(fidx);
      if (cv == CELL_EMPTY) {
        valid_action_ids_.insert(fidx);
      }
    }
  }

  // Step action is always available
  valid_action_ids_.insert(step_action_id_);
}

void
Board::UpdateTerminalStatus()
{
  // Check winning conditions and set 'terminal_status_'. Win if have
  // more building on opponent's half of board then the opponent does.
  // For now logic is 2-player specific.
  assert(players_.size() == 2);

  // Check if opponent player wins by having more on active side of
  // board.
  bool opponent_win = false;
  {
    float active_count = 0.0;
    float opponent_count = 0.0;
    for (size_t ridx = 0; ridx < (size_.RowCount() / 2); ++ridx) {
      for (size_t cidx = 0; cidx < size_.ColumnCount(); ++cidx) {
        const float cv = CellValueAt(ridx, cidx);
        if (cv > 0.0) {
          active_count += cv;
        }
        else if (cv < 0.0) {
          opponent_count -= cv;
        }
      }
    }

    opponent_win = (active_count < opponent_count);
  }

  // Check if active player wins by having more on opponent side of
  // board.
  bool active_win = false;
  {
    float active_count = 0.0;
    float opponent_count = 0.0;
    for (size_t ridx = (size_.RowCount() - 1); ridx >= (size_.RowCount() - (size_.RowCount() / 2)); --ridx) {
      for (size_t cidx = 0; cidx < size_.ColumnCount(); ++cidx) {
        const float cv = CellValueAt(ridx, cidx);
        if (cv > 0.0) {
          active_count += cv;
        }
        else if (cv < 0.0) {
          opponent_count -= cv;
        }
      }
    }

    active_win = (active_count > opponent_count);
  }

  if (active_win && !opponent_win) {
    terminal_status_ =
      cifre::StateTerminalStatus(
        cifre::StateTerminalStatus::TerminalKind::WINNER, active_player_id_);
  }
  else if (opponent_win && !active_win) {
    terminal_status_ =
      cifre::StateTerminalStatus(
        cifre::StateTerminalStatus::TerminalKind::WINNER,
        (active_player_id_ == 0) ? 1 : 0);
  }
  else {
    // Otherwise make sure the terminal status is NON_TERMINAL.
    THROWIF(
      terminal_status_.Kind() != cifre::StateTerminalStatus::TerminalKind::NON_TERMINAL,
      "expected NON_TERMINAL status");
  }
}

void
Board::SwitchActivePlayer()
{
  // Rotate board cells 180 degrees so that opponent's home is row
  // zero. Swap cell values for players.
  size_t ridx_rot = size_.RowCount();
  for (size_t ridx = 0; ridx < (size_.RowCount() / 2); ++ridx) {
    ridx_rot -= 1;

    size_t cidx_rot = size_.ColumnCount();
    for (size_t cidx = 0; cidx < size_.ColumnCount(); ++cidx) {
      cidx_rot -= 1;
      const float t0 = CellValueAt(ridx, cidx);
      const float t1 = CellValueAt(ridx_rot, cidx_rot);
      SetCellValueAt(-t1, ridx, cidx);
      SetCellValueAt(-t0, ridx_rot, cidx_rot);
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
      SetCellValueAt(-t1, ridx, cidx);
      SetCellValueAt(-t0, ridx, cidx_rot);
    }

    // If also an odd number of columns then need to toggle the player
    // in the very center of the board.
    if ((size_.ColumnCount() % 2) != 0) {
      const size_t ridx = size_.RowCount() / 2;
      const size_t cidx = size_.ColumnCount() / 2;
      const float t0 = CellValueAt(ridx, cidx);
      SetCellValueAt(-t0, ridx, cidx);
    }
  }

  // Update the valid actions for the now-active player.
  active_player_id_ = (active_player_id_ + 1) % players_.size();
  UpdateValidActions();
}

std::string
Board::ToString() const
{
  std::stringstream sstr;
  const size_t ch_width = (size_.ColumnCount() + 2) * 3;

  // Column letters across the top.
  for (size_t col_idx = 0; col_idx < (size_.ColumnCount() + 1); ++col_idx) {
    if (col_idx == 0) {
      sstr << "       ";
    }
    else {
      sstr << char('a' + col_idx - 1) << "  ";
    }
  }
  sstr << "\n    " << std::string(ch_width, '-') << '\n';

  // Board with each row number on the left.
  sstr << "   |\n";
  for (size_t row_idx = size_.RowCount(); row_idx > 0; --row_idx) {
    sstr << ' ' << row_idx << " | ";
    for (size_t col_idx = 0; col_idx < size_.ColumnCount(); ++col_idx) {
      const float cv = CellValueAt(row_idx - 1, col_idx);
      if (cv == CELL_EMPTY) {
        sstr << "   ";
      }
      else {
        sstr << ((cv < 0) ? " " : "  ") << int(cv);
      }
    }
    sstr << '\n';
  }
  sstr << "   |";

  return sstr.str();
}

}} // namespace cifre::celcath
