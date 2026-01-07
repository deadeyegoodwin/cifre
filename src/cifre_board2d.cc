// Copyright 2025, David Goodwin. All rights reserved.

#include <algorithm>
#include <string>
#include <sstream>

#include "cifre_board2d.h"
#include "common_error.h"

namespace cifre {

//
// Board2D
//
Board2D::Board2D(
  const cifre::MatrixSize& size, const std::vector<Player*>& players,
  size_t starting_player_id, std::pmr::memory_resource* arena) :
  size_(size), active_player_id_(starting_player_id),
  terminal_status_(cifre::StateTerminalStatus::TerminalKind::NON_TERMINAL),
  valid_action_ids_(arena), cells_flattened_(arena)
{
  THROWIF(players.size() != PLAYER_COUNT, "exactly 2 players are required");
  THROWIF(NoPlayerId() != PLAYER_COUNT, "exactly 2 players are required");
  THROWIF(active_player_id_ >= NoPlayerId(),
          "invalid starting player index " + std::to_string(active_player_id_));

  for (size_t idx = 0; idx < PLAYER_COUNT; ++idx) {
    players_[idx] = players[idx];
  }

#ifndef NDEBUG
  for (size_t pidx = 0; pidx < players_.size(); ++pidx) {
    assert(players_[pidx]->Id() == pidx);
  }
#endif

  // Player 0 is 'o' and player 1 is 'x'.
  players_[0]->SetChar('o');
  players_[1]->SetChar('x');

  cells_flattened_.resize(size_.ElementCount() * sizeof(float));
  cells_ = reinterpret_cast<float*>(cells_flattened_.data());
}

Board2D::Board2D(const Board2D& other, std::pmr::memory_resource* arena) :
  size_(other.size_), players_(other.players_),
  active_player_id_(other.active_player_id_), terminal_status_(other.terminal_status_),
  valid_action_ids_(other.valid_action_ids_.begin(), other.valid_action_ids_.end(), arena),
  cells_flattened_(other.cells_flattened_, arena)
{
  cells_ = reinterpret_cast<float*>(cells_flattened_.data());
}

size_t
Board2D::Hash() const
{
  constexpr uint64_t mulp = 2654435789;
  uint64_t hash = 104395301;

  hash += (active_player_id_ * mulp) ^ (hash >> 23);
  hash += FlattenedCellsHash<32>()(TritonRepresentation());
  return hash ^ (hash << 37);
}

bool
Board2D::Equal(const State* other) const
{
  return ((active_player_id_ == other->ActivePlayerId()) &&
          (TritonRepresentation() == other->TritonRepresentation()));
}

bool
Board2D::IsValidAction(const Action& action) const
{
  switch (action.Kind()) {
  case Action::ActionKind::INVALID:
    return false;

  case Action::ActionKind::HELP:
  case Action::ActionKind::QUIT:
  case Action::ActionKind::SHOW_STATE:
  case Action::ActionKind::CUSTOM_0:
  case Action::ActionKind::CUSTOM_1:
  case Action::ActionKind::CUSTOM_2:
  case Action::ActionKind::CUSTOM_3:
    return true;

  case Action::ActionKind::ACTIONID:
    return std::find(
      valid_action_ids_.begin(), valid_action_ids_.end(), action.ActionId()) != valid_action_ids_.end();
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
    case 's': return Action(Action::ActionKind::SHOW_STATE);
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
Board2D::ActionToString(const Action& action) const
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
Board2D::ToString() const
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
