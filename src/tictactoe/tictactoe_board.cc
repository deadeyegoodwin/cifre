// Copyright 2024, David Goodwin. All rights reserved.

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <regex>
#include <string>
#include <sstream>

#include "tictactoe_board.h"
#include "common_error.h"
#include "common_util.h"

namespace cifre { namespace tictactoe {

//
// Board
//
Board::Board(
  const std::shared_ptr<common::Logger>& logger, const cifre::BoardSize2D& size,
  const std::vector<Player*>& players, size_t starting_player_id) :
  cifre::Board2D<float>(size), logger_(logger), players_(players),
  active_player_id_(starting_player_id), terminal_status_(cifre::State::TerminalStatusKind::NON_TERMINAL)
{
  THROWIF(players_.size() != 2, "exactly 2 players are required");
  THROWIF(active_player_id_ >= players_.size(),
          "invalid starting player index " + std::to_string(active_player_id_));
  THROWIF(size_.RowCount() != size_.ColumnCount(), "square board is required");
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
  THROWIF(action_id >= TotalActionIdCount(),
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

void
Board::SetPlayerIdAt(size_t id, size_t row_idx, size_t col_idx)
{
  SetCellValueAt(PlayerIdToCellValue(id), row_idx, col_idx);
}

size_t
Board::PlayerIdAtFlattened(size_t flat_idx) const
{
  return CellValueToPlayerId(CellValueAtFlattened(flat_idx));
}

void
Board::SetPlayerIdAtFlattened(size_t id, size_t flat_idx)
{
  SetCellValueAtFlattened(PlayerIdToCellValue(id), flat_idx);
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

size_t
Board::Hash() const
{
  const std::vector<uint8_t>& bytes = TritonRepresentation();

  const uint64_t mulp = 2654435789;
  uint64_t hash = 104395301;
  for (uint8_t b : bytes) {
    hash += (b * mulp) ^ (hash >> 23);
  }

  return hash ^ (hash << 37);
}

bool
Board::Equal(const State* other) const
{
  return TritonRepresentation() == other->TritonRepresentation();
}

void
Board::Update(uint64_t action_id, bool enable_logging)
{
  // Occupy the cell corresponding to 'action_id'.
  const float active_player_cell_value = PlayerIdToCellValue(active_player_id_);
  const float current = CellValueAtFlattened(action_id);
  if (current == CELL_EMPTY) {
    SetCellValueAtFlattened(active_player_cell_value, action_id);
  }
  else {
    THROW("unexpected cell value " + std::to_string(current));
  }

  // Board was changed, so check to see if there is a change in the
  // terminal status of the board.
  UpdateTerminalStatus();

  // Switch board state to represent the "other" player.
  SwitchActivePlayer();
}

void
Board::SwitchActivePlayer()
{
  // Swap cell values for players.
  for (size_t fidx = 0; fidx < size_.CellCount(); ++fidx) {
    if (CellValueAtFlattened(fidx) != CELL_EMPTY) {
      SetPlayerIdAtFlattened(OtherPlayerId(PlayerIdAtFlattened(fidx)), fidx);
    }
  }

  // Switch terminal status to represent new active player.
  if (terminal_status_ == cifre::State::TerminalStatusKind::ACTIVE_WINNER) {
    terminal_status_ = cifre::State::TerminalStatusKind::OPPONENT_WINNER;
  }
  else if (terminal_status_ == cifre::State::TerminalStatusKind::OPPONENT_WINNER) {
    terminal_status_ = cifre::State::TerminalStatusKind::ACTIVE_WINNER;
  }

  // Update the valid actions for the now-active player.
  active_player_id_ = (active_player_id_ + 1) % players_.size();
  UpdateValidActions();
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
      terminal_status_ = cifre::State::TerminalStatusKind::ACTIVE_WINNER;
      return;
    }
    if (opponent_count == size_.ColumnCount()) {
      terminal_status_ = cifre::State::TerminalStatusKind::OPPONENT_WINNER;
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
      terminal_status_ = cifre::State::TerminalStatusKind::ACTIVE_WINNER;
      return;
    }
    if (opponent_count == size_.RowCount()) {
      terminal_status_ = cifre::State::TerminalStatusKind::OPPONENT_WINNER;
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
    terminal_status_ = cifre::State::TerminalStatusKind::ACTIVE_WINNER;
    return;
  }
  if (opponent_count == size_.RowCount()) {
    terminal_status_ = cifre::State::TerminalStatusKind::OPPONENT_WINNER;
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
    terminal_status_ = cifre::State::TerminalStatusKind::ACTIVE_WINNER;
    return;
  }
  if (opponent_count == size_.RowCount()) {
    terminal_status_ = cifre::State::TerminalStatusKind::OPPONENT_WINNER;
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
    terminal_status_ = cifre::State::TerminalStatusKind::DRAW;
  }
  else {
    terminal_status_ = cifre::State::TerminalStatusKind::NON_TERMINAL;
  }
}

std::string
Board::ToString(common::Logger::Level filter_level) const
{
  std::stringstream sstr;
  const size_t ch_width = (size_.ColumnCount() + 2) * 2;


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
            sstr << "  ";
          }
          else {
            sstr << ' ' << player->ToString(filter_level);
          }
        }
      }
      sstr << '\n';
    }
  }

  return sstr.str();
}

}} // namespace cifre::tictactoe
