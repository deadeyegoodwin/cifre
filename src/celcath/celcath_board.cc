// Copyright 2024, David Goodwin. All rights reserved.

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <regex>
#include <string>
#include <sstream>

#include "celcath_board.h"
#include "common_error.h"
#include "common_util.h"

namespace cifre { namespace celcath {

//
// Board
//
Board::Board(const Settings& settings, const Stepper& stepper,
             const std::vector<Player*>& players) :
  cifre::Board2D<float>(settings.BoardSize()),
  settings_(settings), stepper_(stepper), players_(players),
  status_(false /* is_terminal */), active_player_id_(settings.StartingPlayerId()),
  complete_actions_this_turn_(0)
{
  THROWIF(players_.size() != 2, "exactly 2 players are required");
  UpdateValidActions();
}

bool
Board::IsValidAction(const Action& action) const
{
  switch (action.Kind()) {
  case Action::ActionKind::INVALID:
    return false;

  case Action::ActionKind::HELP:
  case Action::ActionKind::QUIT:
  case Action::ActionKind::SHOW_BOARD:
    return true;

  case Action::ActionKind::ACTIONID:
    return valid_action_ids_.find(action.ActionId()) != valid_action_ids_.end();
  }

  return false;
}

Action
Board::ActionIdToAction(uint64_t action_id) const
{
  THROWIF(action_id >= settings_.TotalActionIdCount(),
          "unsupported action identifier, " + std::to_string(action_id));

  return Action(Action::ActionKind::ACTIONID, action_id);
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
    case 'b': return Action(Action::ActionKind::SHOW_BOARD);
    case 'e': return Action(Action::ActionKind::ACTIONID, settings_.EndTurnActionId());
    default:
      break;
    }
  }
  // Parse 2 character strings. Cell specifier must be a column letter
  // ('a' based) followed by a row number (1 based).
  else if (posstr.size() == 2) {
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

std::string
Board::ActionToString(const Action& action, common::Logger::Level filter_level) const
{
  if (action.Kind() != Action::ActionKind::ACTIONID) {
    return action.ToString(filter_level);
  }

  const uint64_t action_id = action.ActionId();

  if (action_id == settings_.EndTurnActionId()) {
    return "END-TURN";
  }

  // Get cell specifier for action. Cell specifier must be a column
  // letter ('a' based) and a row number (1 based).
  const size_t row_idx = action_id / size_.ColumnCount();
  const size_t col_idx = action_id % size_.ColumnCount();
  const char row_ch = '1' + row_idx;
  const char col_ch = 'a' + col_idx;

  const float current = CellValueAtFlattened(action_id);
  const float active_player_cell_value = PlayerIdToCellValue(active_player_id_);
  if (current == active_player_cell_value) {
    return std::string("REMOVE [") + row_ch + "," + col_ch + "]";
  }
  else if (current == CELL_EMPTY) {
    return std::string("ADD [") + row_ch + "," + col_ch + "]";
  }

  THROW("cannot convert action ID to string, " + action.ToString(filter_level));
}

size_t
Board::PlayerIdAt(size_t row_idx, size_t col_idx) const
{
  return CellValueToPlayerId(CellValueAt(row_idx, col_idx));
}

size_t
Board::PlayerIdAtFlattened(size_t flat_idx) const
{
  return CellValueToPlayerId(CellValueAtFlattened(flat_idx));
}

Player*
Board::PlayerAt(size_t row_idx, size_t col_idx) const
{
  const size_t id = PlayerIdAt(row_idx, col_idx);
  if (id == players_.size()) {
    return nullptr;
  }

  return players_[id];
}

void
Board::SetPlayerAt(Player* player, size_t row_idx, size_t col_idx)
{
  SetCellValueAt(PlayerToCellValue(player), row_idx, col_idx);
}

Player*
Board::PlayerAtFlattened(size_t flat_idx) const
{
  const size_t id = PlayerIdAtFlattened(flat_idx);
  if (id == players_.size()) {
    return nullptr;
  }

  return players_[id];
}

void
Board::SetPlayerAtFlattened(Player* player, size_t flat_idx)
{
  SetCellValueAtFlattened(PlayerToCellValue(player), flat_idx);
}

void
Board::Update(uint64_t action_id)
{
  bool next_player = false;
  bool run_stepper = false;

  // End turn?
  if (action_id == settings_.EndTurnActionId()) {
    next_player = run_stepper = true;
  }
  // All other action IDs toggle the liveness of the appropriate cell.
  else {
    const float active_player_cell_value = PlayerIdToCellValue(active_player_id_);
    const float current = CellValueAtFlattened(action_id);
    if (current == active_player_cell_value) {
      SetCellValueAtFlattened(CELL_EMPTY, action_id);
    }
    else if (current == CELL_EMPTY) {
      SetCellValueAtFlattened(active_player_cell_value, action_id);
    }
    else {
      THROW("unexpected cell value " + std::to_string(current));
    }

    // Show that an action has been completed for this turn, if max
    // number of allowed actions has been completed then the active
    // player's turn is over.
    complete_actions_this_turn_ += 1;
    if (complete_actions_this_turn_ >= settings_.MaxActionsPerTurn()) {
      next_player = run_stepper = true;
    }
    // Active player's turn is not complete, so only need to update
    // validity of the end-turn action.
    else if (complete_actions_this_turn_ >= settings_.MinActionsPerTurn()) {
      valid_action_ids_.insert(settings_.EndTurnActionId());
    }

    // Board was changed, so check to see if there is a change in the
    // status of the board.
    UpdateStatus();
  }

  // If necessary run the stepper and check to see if there is a
  // change in the status of the board.
  if (run_stepper) {
    stepper_.Step(this, players_);
    UpdateStatus();
  }

  if (next_player) {
    active_player_id_ = (active_player_id_ + 1) % players_.size();
    complete_actions_this_turn_ = 0;

    // If the new active player is the starting player, then this is
    // the start of a new turn.
    if (active_player_id_ == settings_.StartingPlayerId()) {
      status_.turn_count_ += 1;
    }

    // Update the valid actions for the now-active player.
    UpdateValidActions();
  }
}

void
Board::UpdateValidActions()
{
  valid_action_ids_.clear();

  const float active_player_cell_value = PlayerIdToCellValue(active_player_id_);

  // FIXME for now can play on any cell except home rows.
  for (size_t ridx = 1; ridx < (size_.RowCount() - 1); ++ridx) {
    for (size_t cidx = 0; cidx < size_.ColumnCount(); ++cidx) {
      const float current = CellValueAt(ridx, cidx);
      if ((current == active_player_cell_value) || (current == CELL_EMPTY)) {
        valid_action_ids_.insert((ridx * size_.ColumnCount()) + cidx);
      }
    }
  }

  if (complete_actions_this_turn_ >= settings_.MinActionsPerTurn()) {
    valid_action_ids_.insert(settings_.EndTurnActionId());
  }
}

void
Board::UpdateStatus()
{
  // Check winning conditions.

  // FIXME for now must have cell on opponents first row.

  // Row 0 is player zero's home row, check it for player 1.
  bool player1_winner = false;
  for (size_t cidx = 0; cidx < size_.ColumnCount(); ++cidx) {
    if (PlayerAt(0 /* row_idx */, cidx) == players_[1]) {
      player1_winner = true;
      break;
    }
  }

  // Row n is player one's home row, check it for player 0.
  bool player0_winner = false;
  for (size_t cidx = 0; cidx < size_.ColumnCount(); ++cidx) {
    if (PlayerAt(size_.RowCount() - 1 /* row_idx */, cidx) == players_[0]) {
      player0_winner = true;
      break;
    }
  }

  if (player0_winner || player1_winner) {
    status_.is_terminal_ = true;
    if (!player0_winner) {
      status_.is_draw_ = false;
      status_.winner_id_ = 1;
    }
    else if (!player1_winner) {
      status_.is_draw_ = false;
      status_.winner_id_ = 0;
    }
    else {
      status_.is_draw_ = true;
    }
  }
}

std::string
Board::ToString(common::Logger::Level filter_level) const
{
  std::stringstream sstr;
  const size_t rh_width = 4;
  const size_t ch_width = (size_.ColumnCount() + 2) * 2;
  const std::string& player0_name = players_[0]->Name() + " - " + players_[0]->BoardCharacter();
  const std::string& player1_name = players_[1]->Name() + " - " + players_[1]->BoardCharacter();

  // Player 0 home row is the "top" of the board (row 0).
  sstr << std::string(rh_width + ((ch_width - player0_name.size()) / 2), ' ') << player0_name << '\n';

  // Column letters across the top.
  for (size_t col_idx = 0; col_idx < (size_.ColumnCount() + 1); ++col_idx) {
    if (col_idx == 0) {
      sstr << "       ";
    }
    else {
      sstr << char('a' + col_idx - 1) << ' ';
    }
  }
  sstr << "\n    " << std::string(ch_width, '-') << '\n';

  // Board with each row number on the left.
  for (size_t row_idx = 0; row_idx < (size_.RowCount() + 2); ++row_idx) {
    if (row_idx == 0) {
      sstr << "   |\n";
    }
    else {
      if (row_idx < (size_.RowCount() + 1)) {
        sstr << ' ' << row_idx << ' ';
      }
      else {
        sstr << "   ";
      }

      sstr << "|  ";
      if (row_idx < (size_.RowCount() + 1)) {
        for (size_t col_idx = 0; col_idx < size_.ColumnCount(); ++col_idx) {
          Player* player = PlayerAt(row_idx - 1, col_idx);
          if (player == nullptr) {
            player = &Player::DEAD;
          }
          sstr << ' ' << player->ToString(filter_level);
        }
      }
      sstr << '\n';
    }
  }

  // Player 1 home row is the "bottom" of the board (row n).
  sstr << std::string(rh_width + ((ch_width - player1_name.size()) / 2), ' ') << player1_name;

  return sstr.str();
}

}} // namespace cifre::celcath
