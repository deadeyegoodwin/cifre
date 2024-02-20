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
Board::Board(const Settings& settings, const std::vector<Player*>& players) :
  cifre::Board2D<float>(settings.BoardSize()),
  settings_(settings), players_(players), status_(false /* is_terminal */),
  active_player_id_(settings.StartingPlayerId()), complete_actions_this_turn_(0)
{
  THROWIF(players_.size() != 2, "exactly 2 players are required");

  // Initially board is empty so all cells are valid to toggle.
  for (size_t cidx = 0; cidx < size_.CellCount(); ++cidx) {
    cells_[cidx] = CELL_EMPTY;
    valid_action_ids_.insert(cidx);
  }

  // End-turn action is only valid initially if 0 actions are allowed
  // in a turn.
  if (settings_.MinActionsPerTurn() == 0) {
    valid_action_ids_.insert(settings_.EndTurnActionId());
  }
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
    int32_t col_idx = posstr[0] - 'a';
    int32_t row_idx = posstr[1] - '1';
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
  bool next_player = false;
  bool run_stepper = false;
  bool next_move = false;

  // When turn ends need to run the stepper and then change to next
  // player.
  if (action_id == settings_.EndTurnActionId()) {
    next_player = run_stepper = true;
  }
  // All other actions toggle the liveness of the appropriate cell.
  // FIXME need to adjust:
  //   THROWIF(valid_action_ids_.erase(action_id) != 1,
  //            "expected ");
  else {
    const float active_player_cell_value = PlayerIdToCellValue(active_player_id_);
    const float current = CellValueAtFlattened(action_id);
    if (current == active_player_cell_value) {
      SetCellValueAtFlattened(CELL_EMPTY, action_id);
    }
    else if (current == CELL_EMPTY) {
      SetCellValueAtFlattened(active_player_cell_value, action_id);
    }

    next_move = true;
  }

  if (run_stepper) {
    // FIXME
  }

  if (next_player) {
    active_player_id_ = (active_player_id_ + 1) % players_.size();
    complete_actions_this_turn_ = 0;
    // FIXME toggle valid action IDs
  } else if (next_move) {
    complete_actions_this_turn_ += 1;
    if (complete_actions_this_turn_ >= settings_.MaxActionsPerTurn()) {
      valid_action_ids_.clear();
    }
    if (complete_actions_this_turn_ >= settings_.MinActionsPerTurn()) {
      valid_action_ids_.insert(settings_.EndTurnActionId());
    }
  }
}

std::string
Board::ToString(common::Logger::Level filter_level) const
{
  std::stringstream sstr;

  // Column letters across the top.
  for (size_t col_idx = 0; col_idx < (size_.ColumnCount() + 1); ++col_idx) {
    if (col_idx == 0) {
      sstr << "       ";
    }
    else {
      sstr << char('a' + col_idx - 1) << ' ';
    }
  }
  sstr << "\n    " << std::string((size_.ColumnCount() + 2) * 2, '-') << '\n';

  // Board with each row number on the left.
  for (size_t row_idx = 0; row_idx < (size_.RowCount() + 2); ++row_idx) {
    if (row_idx == 0) {
      sstr << "   |";
    }
    else if (row_idx == (size_.RowCount() + 1)) {
      sstr << "\n   |";
    }
    else {
      sstr << '\n';
      if (row_idx < (size_.RowCount() + 1)) {
        sstr << ' ' << row_idx << ' ';
      }
      else {
        sstr << "   ";
      }

      sstr << "|  ";
      if (row_idx < (size_.RowCount() + 1)) {
        for (size_t col_idx = 0; col_idx < size_.ColumnCount(); ++col_idx) {
          sstr << ' ' << CellValueAt(row_idx - 1, col_idx);
        }
      }
    }
  }

  return sstr.str();
}

}} // namespace cifre::celcath
