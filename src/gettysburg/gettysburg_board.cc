// Copyright 2024, David Goodwin. All rights reserved.

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>

#include "gettysburg_board.h"
#include "common_error.h"
#include "common_util.h"

namespace cifre { namespace gettysburg {

enum class PieceActionKind {
  MOVE_UP = 0, MOVE_UPRIGHT, MOVE_DOWNRIGHT, MOVE_DOWN, MOVE_DOWNLEFT, MOVE_UPLEFT,
  MOVE_END };

//
// Board
//
Board::Board(const cifre::BoardSize2D& size, const std::vector<Player*>& players,
             size_t starting_player_id) :
  cifre::Board2DHex(PLANE_COUNT, size, players, starting_player_id)
{
  // The input planes are based on the size of the board. Because the
  // model is trained/used by always having the piece currently being
  // controlled at the center of the plane, there must be a border
  // around the map to allow any actual map hex to be at the center of
  // the plane. These border hexes are marked as being inaccessible.
  plane_column_cnt_ = ((size_.ColumnCount() - 1) * 3) + 1;
  plane_row_cnt_ = ((size_.RowCount() - 1) * 3) + 1;
  board_first_column_ = size_.ColumnCount() - 1;
  board_first_row_ = size_.RowCount() - 1;

  cells_flattened_.resize(plane_row_cnt_ * plane_column_cnt_ * plane_cnt_ * sizeof(float));
  cells_ = reinterpret_cast<float*>(cells_flattened_.data());

  // Initially set union player at board position [0, 0] and
  // confederate at board position [row-1, col-1].
  SetUnionAt(CELL_ACTIVE, 0, 0);
  SetConfederateAt(CELL_ACTIVE, size_.RowCount() - 1, size_.ColumnCount() - 1);
  SetGoalAt(CELL_ACTIVE, size_.RowCount() / 2, size_.ColumnCount() / 2);

  // Initialize 'valid_action_ids_' and 'terminal_status_'.
  UpdateValidActions();
  UpdateTerminalStatus();
}

size_t
Board::TotalActionIdCount() const
{
  // Six move actions (one for each direction), plus one to end this
  // pieces movement.
  return 7;
}

std::string
Board::ActionToString(const Action& action) const
{
  if (action.Kind() != Action::ActionKind::ACTIONID) {
    return action.ToString();
  }

  const PieceActionKind action_id = (PieceActionKind)action.ActionId();
  switch (action_id) {
  case PieceActionKind::MOVE_UP: return "MOVE_UP";
  case PieceActionKind::MOVE_UPRIGHT: return "MOVE_UPRIGHT";
  case PieceActionKind::MOVE_DOWNRIGHT: return "MOVE_DOWNRIGHT";
  case PieceActionKind::MOVE_DOWN: return "MOVE_DOWN";
  case PieceActionKind::MOVE_DOWNLEFT: return "MOVE_DOWNLEFT";
  case PieceActionKind::MOVE_UPLEFT: return "MOVE_UPLEFT";
  case PieceActionKind::MOVE_END: return "MOVE_END";
  }

  return "<unknown action>";
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
    default:
      break;
    }
  }

  common::ToLower(posstr);

  // Moves
  if ((posstr == "up") || (posstr == "u")) {
    return Action(Action::ActionKind::ACTIONID, (uint64_t)PieceActionKind::MOVE_UP);
  }
  if ((posstr == "up-right") || (posstr == "upright") || (posstr == "ur")) {
    return Action(Action::ActionKind::ACTIONID, (uint64_t)PieceActionKind::MOVE_UPRIGHT);
  }
  if ((posstr == "down-right") || (posstr == "downright") || (posstr == "dr")) {
    return Action(Action::ActionKind::ACTIONID, (uint64_t)PieceActionKind::MOVE_DOWNRIGHT);
  }
  if ((posstr == "down") || (posstr == "d")) {
    return Action(Action::ActionKind::ACTIONID, (uint64_t)PieceActionKind::MOVE_DOWN);
  }
  if ((posstr == "down-left") || (posstr == "downleft") || (posstr == "dl")) {
    return Action(Action::ActionKind::ACTIONID, (uint64_t)PieceActionKind::MOVE_DOWNLEFT);
  }
  if ((posstr == "up-left") || (posstr == "upleft") || (posstr == "ul")) {
    return Action(Action::ActionKind::ACTIONID, (uint64_t)PieceActionKind::MOVE_UPLEFT);
  }
  if (posstr == "end") {
    return Action(Action::ActionKind::ACTIONID, (uint64_t)PieceActionKind::MOVE_END);
  }

  return Action(Action::ActionKind::INVALID);
}

void
Board::Update(uint64_t action_id)
{
  // Check terminal status and if not at terminal then advance to next
  // active player.
  UpdateTerminalStatus();
  if (terminal_status_.Kind() == cifre::StateTerminalStatus::TerminalKind::NON_TERMINAL) {

    UpdateValidActions();
  }
}

void
Board::UpdateValidActions()
{
  valid_action_ids_.clear();
  valid_action_ids_ = { 0, 1, 2, 3, 4, 5, 6 }; // FIXME
}

void
Board::UpdateTerminalStatus()
{
  // FIXME
//  terminal_status_ =
//    cifre::StateTerminalStatus(
//      cifre::StateTerminalStatus::TerminalKind::WINNER, CellValueToPlayer(cv)->Id());
//  return;

  // Otherwise make sure the terminal status is NON_TERMINAL.
  THROWIF(
    terminal_status_.Kind() != cifre::StateTerminalStatus::TerminalKind::NON_TERMINAL,
    "expected NON_TERMINAL status");
}

std::string
Board::BoardCellUpperString(size_t row_idx, size_t col_idx) const
{
  std::string str = std::to_string(row_idx);
  if (str.size() == 1) {
    str += " ";
  }
  return str;
}

std::string
Board::BoardCellLowerString(size_t row_idx, size_t col_idx) const
{
  std::string str = std::to_string(col_idx);
  if (str.size() == 1) {
    str = "_" + str;
  }
  return str;
}

}} // namespace cifre::gettysburg
