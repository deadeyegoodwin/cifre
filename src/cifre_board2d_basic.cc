// Copyright 2025, David Goodwin. All rights reserved.

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <regex>
#include <string>
#include <sstream>

#include "cifre_board2d_basic.h"
#include "common_error.h"

namespace cifre {

//
// Board2DBasic
//
Board2DBasic::Board2DBasic(
  const cifre::BoardSize2D& size, const std::vector<Player*>& players,
  size_t starting_player_id) :
  Board2D(1 /* plane_cnt */, size, players, starting_player_id)
{
  THROWIF(players_.size() != PLAYER_COUNT, "exactly 2 players are required");
  THROWIF(NoPlayerId() != PLAYER_COUNT, "exactly 2 players are required");

  // Player 0 is 'o' and player 1 is 'x'.
  players[0]->SetChar('o');
  players[1]->SetChar('x');

  cells_flattened_.resize(size_.CellCount() * sizeof(float));
  cells_ = reinterpret_cast<float*>(cells_flattened_.data());
}

Board2DBasic::Board2DBasic(const Board2DBasic& other) :
  Board2D(other)
{
  cells_flattened_ = other.cells_flattened_;
  cells_ = reinterpret_cast<float*>(cells_flattened_.data());
}

Action
Board2DBasic::StringToAction(const std::string& str) const
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
    case '!': return Action(Action::ActionKind::SHOW_DEBUG_STATE);
    default:
      break;
    }
  }
  // Parse 2 character strings. Cell specifier must be a row number (1
  // based) followed by a column letter ('a' based).
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
Board2DBasic::ActionToString(const Action& action) const
{
  if (action.Kind() != Action::ActionKind::ACTIONID) {
    return action.ToString();
  }

  const uint64_t action_id = action.ActionId();

  // Get cell specifier for action. Cell specifier must be a row
  // number (1 based) followed by a column letter ('a' based).
  const size_t row_idx = action_id / size_.ColumnCount();
  const size_t col_idx = action_id % size_.ColumnCount();
  const char row_ch = '1' + row_idx;
  const char col_ch = 'a' + col_idx;

  const float current = CellValueAtFlattened(action_id);
  if (current == CELL_ACTIVE_PLAYER) {
    return std::string("REMOVE [") + row_ch + "," + col_ch + "]";
  }
  else if (current == CELL_EMPTY) {
    return std::string("ADD [") + row_ch + "," + col_ch + "]";
  }

  THROW("cannot convert action ID to string, " + action.ToString());
}

std::string
Board2DBasic::ToString() const
{
  std::stringstream sstr;
  const size_t ch_width = (size_.ColumnCount() + 2) * 2;

  // Player key.
  sstr
    << " active(p" << ActivePlayerId() << ")=" << ActivePlayer()->Name()
    << "(" << ActivePlayer()->Char() << "), "
    << "opponent(p" << OpponentPlayerId() << ")=" << OpponentPlayer()->Name()
    << "(" << OpponentPlayer()->Char() << ")"
    << "\n";

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
  sstr << "   |\n";
  for (size_t row_idx = size_.RowCount(); row_idx > 0; --row_idx) {
    sstr << ' ' << row_idx << " |  ";
    for (size_t col_idx = 0; col_idx < size_.ColumnCount(); ++col_idx) {
      Player* player = PlayerAt(row_idx - 1, col_idx);
      if (player == nullptr) {
        sstr << "  ";
      }
      else {
        sstr << ' ' << player->ToString();
      }
    }
    sstr << '\n';
  }
  sstr << "   |";

  return sstr.str();
}

} // namespace cifre
