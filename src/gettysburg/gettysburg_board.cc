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

//
// Board
//
Board::Board(const cifre::BoardSize2D& size, const std::vector<Player*>& players,
             size_t starting_player_id) :
  cifre::Board2DHex(PLANE_COUNT, size, players, starting_player_id)
{
  // Player 0 is 'u' (union) and player 1 is 'c' (confederate).
  players[0]->SetChar('u');
  players[1]->SetChar('c');

  // The input planes are based on the size of the board. Because the
  // model is trained/used by always having the unit currently being
  // controlled at the center of the plane, there must be a border
  // around the map to allow any actual map hex to be at the center of
  // the plane. These border hexes are marked as being inaccessible.
  plane_column_cnt_ = ((size_.ColumnCount() - 1) * 3) + 1;
  plane_row_cnt_ = ((size_.RowCount() - 1) * 3) + 1;
  board_first_column_ = size_.ColumnCount() - 1;
  board_first_row_ = size_.RowCount() - 1;

  cells_flattened_.resize(plane_row_cnt_ * plane_column_cnt_ * plane_cnt_ * sizeof(float));
  cells_ = reinterpret_cast<float*>(cells_flattened_.data());

  // Initially set union unit at board position [0, 0] and
  // confederate at board position [row-1, col-1].
  auto union_unit = std::make_unique<Unit>(Unit::ArmyKind::UNION, 0, 0);
  union_unit->AddToBoard(this);
  union_units_.push_back(std::move(union_unit));

  auto confederate_unit = std::make_unique<Unit>(
    Unit::ArmyKind::CONFEDERATE, size_.RowCount() - 1, size_.ColumnCount() - 1);
  confederate_unit->AddToBoard(this);
  confederate_units_.push_back(std::move(confederate_unit));

  // Set a goal at the center of the board.
  SetGoalAt(CELL_ACTIVE, (size_.RowCount() / 2) - 1, (size_.ColumnCount() / 2) - 1);

  // Initialize 'valid_action_ids_' and 'terminal_status_'.
  UpdateValidActions();
  UpdateTerminalStatus();
}

Board::Board(const Board& other) :
  cifre::Board2DHex(other),
  plane_row_cnt_(other.plane_row_cnt_), plane_column_cnt_(other.plane_column_cnt_),
  board_first_row_(other.board_first_row_), board_first_column_(other.board_first_column_)
{
}

size_t
Board::TotalActionIdCount() const
{
  return size_t(Unit::ActionKind::ACTION_COUNT);
}

std::string
Board::ActionToString(const Action& action) const
{
  if (action.Kind() != Action::ActionKind::ACTIONID) {
    return action.ToString();
  }

  return Unit::ActionKindToString((Unit::ActionKind)action.ActionId());
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
    case '!': return Action(Action::ActionKind::SHOW_DEBUG_STATE);
    default:
      break;
    }
  }

  common::ToLower(posstr);

  // Moves
  if ((posstr == "up") || (posstr == "u")) {
    return Action(Action::ActionKind::ACTIONID, (uint64_t)Unit::ActionKind::MOVE_UP);
  }
  if ((posstr == "up-right") || (posstr == "upright") || (posstr == "ur")) {
    return Action(Action::ActionKind::ACTIONID, (uint64_t)Unit::ActionKind::MOVE_UPRIGHT);
  }
  if ((posstr == "down-right") || (posstr == "downright") || (posstr == "dr")) {
    return Action(Action::ActionKind::ACTIONID, (uint64_t)Unit::ActionKind::MOVE_DOWNRIGHT);
  }
  if ((posstr == "down") || (posstr == "d")) {
    return Action(Action::ActionKind::ACTIONID, (uint64_t)Unit::ActionKind::MOVE_DOWN);
  }
  if ((posstr == "down-left") || (posstr == "downleft") || (posstr == "dl")) {
    return Action(Action::ActionKind::ACTIONID, (uint64_t)Unit::ActionKind::MOVE_DOWNLEFT);
  }
  if ((posstr == "up-left") || (posstr == "upleft") || (posstr == "ul")) {
    return Action(Action::ActionKind::ACTIONID, (uint64_t)Unit::ActionKind::MOVE_UPLEFT);
  }
  if (posstr == "end") {
    return Action(Action::ActionKind::ACTIONID, (uint64_t)Unit::ActionKind::MOVE_END);
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
  if (UnionAt(row_idx, col_idx) == CELL_ACTIVE) {
    return std::string(1, UNION_CHAR) + " ";
  }
  if (ConfederateAt(row_idx, col_idx) == CELL_ACTIVE) {
    return std::string(1, CONFEDERATE_CHAR) + " ";
  }

  return "  ";
}

std::string
Board::BoardCellLowerString(size_t row_idx, size_t col_idx) const
{
  if (GoalAt(row_idx, col_idx) == CELL_ACTIVE) {
    return "_*";
  }

  return "__";
}

std::string
Board::ToDebugString() const
{
  std::stringstream sstr;

  // Show all planes...

  // Top of first row of hex cells.
  for (size_t col_idx = 0; col_idx < plane_column_cnt_; col_idx += 2) {
    if (col_idx == 0) {
      sstr << " __";
    }
    else {
      sstr << "    __";
    }
  }
  sstr << "\n";

  // Rows...
  for (size_t row_idx = 0; row_idx < plane_row_cnt_; ++row_idx) {
    for (size_t col_idx = 0; col_idx < plane_column_cnt_; ++col_idx) {
      if ((col_idx % 2) == 0) {
        sstr << "/" << DebugCellUpperString(row_idx, col_idx) << "\\";
      }
      else if (row_idx > 0) {
        sstr << DebugCellLowerString(row_idx - 1, col_idx);
      }
      else {
        sstr << "__";
      }
    }
    if ((row_idx != 0) && ((plane_column_cnt_ % 2) == 0)) {
      sstr << "/";
    }
    sstr << "\n";

    for (size_t col_idx = 0; col_idx < plane_column_cnt_; ++col_idx) {
      if ((col_idx % 2) == 0) {
        sstr << "\\" << DebugCellLowerString(row_idx, col_idx) << "/";
      }
      else {
        sstr << DebugCellUpperString(row_idx, col_idx);
      }
    }
    if ((plane_column_cnt_ % 2) == 0) {
      sstr << "\\";
    }
    sstr << "\n";
  }

  // Lower half of last row.
  sstr << " ";
  for (size_t col_idx = 0; col_idx < plane_column_cnt_; ++col_idx) {
    if ((col_idx % 2) == 1) {
      sstr << "  \\" << DebugCellLowerString(plane_row_cnt_ - 1, col_idx) << "/";
    }
  }
  sstr << "\n";

  return sstr.str();
}

std::string
Board::DebugCellUpperString(size_t row_idx, size_t col_idx) const
{
  if (PlaneCellValueAt(row_idx, col_idx, PLANE_IDX_UNION) == CELL_ACTIVE) {
    return std::string(1, UNION_CHAR) + " ";
  }
  if (PlaneCellValueAt(row_idx, col_idx, PLANE_IDX_CONFEDERATE) == CELL_ACTIVE) {
    return std::string(1, CONFEDERATE_CHAR) + " ";
  }

  return "  ";
}

std::string
Board::DebugCellLowerString(size_t row_idx, size_t col_idx) const
{
  if (PlaneCellValueAt(row_idx, col_idx, PLANE_IDX_GOAL) == CELL_ACTIVE) {
    return "_*";
  }

  return "__";
}

}} // namespace cifre::gettysburg
