// Copyright 2024, David Goodwin. All rights reserved.

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <regex>
#include <string>
#include <sstream>

#include "cifre_board2d.h"
#include "common_error.h"

namespace cifre {

//
// BoardSize2D
//
BoardSize2D::BoardSize2D(const std::string& str)
{
  static std::regex regex_square("^([1-9][0-9]*)$");
  static std::regex regex_rectangle("^([1-9][0-9]*)x([1-9][0-9]*)$");

  std::smatch m;
  if (!std::regex_match(str, m, regex_rectangle)) {
    THROWIF(!std::regex_match(str, m, regex_square),
            "board size must be specified as 'NxM'");
    row_cnt_ = column_cnt_ = std::stoi(m[1]);
  }
  else {
    row_cnt_ = std::stoi(m[1]);
    column_cnt_ = std::stoi(m[2]);
  }
}

std::string
BoardSize2D::ToString() const
{
  return std::to_string(row_cnt_) + "x" + std::to_string(column_cnt_);
}

//
// Board2D
//
Board2D::Board2D(const cifre::BoardSize2D& size, const std::vector<Player*>& players,
                 size_t starting_player_id) :
  size_(size), players_(players), active_player_id_(starting_player_id),
  terminal_status_(cifre::State::TerminalStatusKind::NON_TERMINAL)
{
  THROWIF(players_.size() != PLAYER_COUNT, "exactly 2 players are required");
  THROWIF(active_player_id_ >= PLAYER_COUNT,
          "invalid starting player index " + std::to_string(active_player_id_));

  // Initialize storage for flattened cell values to all zeros
  // (CELL_EMPTY). Derived class must override initialization if
  // required.
  cells_flattened_.resize(size_.CellCount() * sizeof(float));
  cells_ = reinterpret_cast<float*>(cells_flattened_.data());
}

Board2D::Board2D(const Board2D& other) :
  size_(other.size_), players_(other.players_),
  active_player_id_(other.active_player_id_), terminal_status_(other.terminal_status_),
  valid_action_ids_(other.valid_action_ids_), cells_flattened_(other.cells_flattened_)
{
  cells_ = reinterpret_cast<float*>(cells_flattened_.data());
}

size_t
Board2D::Hash() const
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
Board2D::Equal(const State* other) const
{
  return TritonRepresentation() == other->TritonRepresentation();
}

bool
Board2D::IsValidAction(const Action& action) const
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
Board2D::StringToAction(const std::string& str) const
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
Board2D::ActionToString(const Action& action) const
{
  if (action.Kind() != Action::ActionKind::ACTIONID) {
    return action.ToString();
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

  THROW("cannot convert action ID to string, " + action.ToString());
}

Player*
Board2D::PlayerAt(size_t row_idx, size_t col_idx) const
{
  const size_t id = PlayerIdAt(row_idx, col_idx);
  if (id == PLAYER_COUNT) {
    return nullptr;
  }

  return players_[id];
}

Player*
Board2D::PlayerAtFlattened(size_t flat_idx) const
{
  const size_t id = PlayerIdAtFlattened(flat_idx);
  if (id == PLAYER_COUNT) {
    return nullptr;
  }

  return players_[id];
}

void
Board2D::SwitchActivePlayer()
{
  // Swap cell values for players.
  for (size_t fidx = 0; fidx < size_.CellCount(); ++fidx) {
    size_t pid = PlayerIdAtFlattened(fidx);
    if (pid != PLAYER_COUNT) {
      SetPlayerIdAtFlattened((pid == 0) ? 1 : 0, fidx);
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


std::string
Board2D::ToString() const
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
            sstr << ' ' << player->ToString();
          }
        }
      }
      sstr << '\n';
    }
  }

  return sstr.str();
}

} // namespace cifre
