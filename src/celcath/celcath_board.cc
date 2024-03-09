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
Board::Board(
  const std::shared_ptr<common::Logger>& logger, const Settings& settings,
  const Stepper& stepper, const std::vector<Player*>& players) :
  cifre::Board2D<float>(settings.BoardSize()),
  logger_(logger), settings_(settings), stepper_(stepper), players_(players),
  status_(Status::NON_TERMINAL), active_player_id_(settings.StartingPlayerId()),
  end_turn_count_(0), complete_actions_this_turn_(0)
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

  if (next_player) {
    LOGGER_LOGMED(*logger_, ToString());
  }

  // If necessary run the stepper and check to see if there is a
  // change in the status of the board.
  if (run_stepper) {
    LOGGER_LOGMED(*logger_, std::string("run ") + stepper_.Name());
    stepper_.Step(this, players_);
    UpdateStatus();
  }

  // Handle switch board state to represent the "other" player.
  if (next_player) {
    SwitchActivePlayer();

    // If the new active player is the starting player, then this is
    // the start of a new turn.
    if (active_player_id_ == settings_.StartingPlayerId()) {
      end_turn_count_ += 1;
    }

    LOGGER_LOGMED(*logger_, ToString());
  }
}

void
Board::SwitchActivePlayer()
{
  const size_t new_active_player_id_ = (active_player_id_ + 1) % players_.size();

  // Rotate board cells 180 degrees so that opponent's
  // (i.e. 'new_active_player_id') home is row 0. Swap cell values for
  // players.
  size_t ridx_rot = size_.RowCount();
  for (size_t ridx = 0; ridx < (size_.RowCount() / 2); ++ridx) {
    ridx_rot -= 1;

    size_t cidx_rot = size_.ColumnCount();
    for (size_t cidx = 0; cidx < size_.ColumnCount(); ++cidx) {
      cidx_rot -= 1;
      const size_t t0 = PlayerIdAt(ridx, cidx);
      const size_t t1 = PlayerIdAt(ridx_rot, cidx_rot);
      SetPlayerIdAt(OtherPlayerId(t0), ridx_rot, cidx_rot);
      SetPlayerIdAt(OtherPlayerId(t1), ridx, cidx);
    }
  }

  // If there is an odd number of rows, then swap the middle row that
  // was not handled by the above.
  if ((size_.RowCount() % 2) != 0) {
    const size_t ridx = size_.RowCount() / 2;
    size_t cidx_rot = size_.ColumnCount();
    for (size_t cidx = 0; cidx < (size_.ColumnCount() / 2); ++cidx) {
      cidx_rot -= 1;
      const size_t t0 = PlayerIdAt(ridx, cidx);
      const size_t t1 = PlayerIdAt(ridx, cidx_rot);
      SetPlayerIdAt(OtherPlayerId(t0), ridx, cidx_rot);
      SetPlayerIdAt(OtherPlayerId(t1), ridx, cidx);
    }

    // If also an odd number of columns then need to toggle the player
    // in the very center of the board.
    if ((size_.ColumnCount() % 2) != 0) {
      const size_t ridx = size_.RowCount() / 2;
      const size_t cidx = size_.ColumnCount() / 2;
      const size_t t0 = PlayerIdAt(ridx, cidx);
      SetPlayerIdAt(OtherPlayerId(t0), ridx, cidx);
    }
  }

  // Switch status to represent that there is a new active player.
  if (status_ == Status::ACTIVE_WINNER) {
    status_ = Status::OPPONENT_WINNER;
  }
  else if (status_ == Status::OPPONENT_WINNER) {
    status_ = Status::ACTIVE_WINNER;
  }

  // Make opponent the active player.
  active_player_id_ = new_active_player_id_;
  complete_actions_this_turn_ = 0;

  // Update the valid actions for the now-active player.
  UpdateValidActions();
}

void
Board::UpdateValidActions()
{
  valid_action_ids_.clear();

  const float active_player_cell_value = PlayerIdToCellValue(active_player_id_);

  // The active player's home row is row 0, the opponent player's home
  // row is size_.RowCount() - 1.

  // FIXME ruleset, for now can play on any cell except own home row
  // and opponents 2 home rows.
  for (size_t ridx = 1; ridx < (size_.RowCount() - 2); ++ridx) {
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

  // FIXME rulset for now must have cell on opponents first row to win.

  // Row 0 is active player's home row, so if any opponent on that row
  // then the opponent wins.
  bool opponent_winner = false;
  for (size_t cidx = 0; cidx < size_.ColumnCount(); ++cidx) {
    if (PlayerIdAt(0 /* row_idx */, cidx) == OpponentPlayerId()) {
      opponent_winner = true;
      break;
    }
  }

  // Row n is opponent player's home row, so if any active player on
  // that row then active player wins.
  bool active_winner = false;
  for (size_t cidx = 0; cidx < size_.ColumnCount(); ++cidx) {
    if (PlayerIdAt(size_.RowCount() - 1 /* row_idx */, cidx) == ActivePlayerId()) {
      active_winner = true;
      break;
    }
  }

  if (opponent_winner && active_winner) {
    status_ = Status::DRAW;
  }
  else if (active_winner) {
    status_ = Status::ACTIVE_WINNER;
  }
  else if (opponent_winner) {
    status_ = Status::OPPONENT_WINNER;
  }
  else {
    status_ = Status::NON_TERMINAL;
  }
}

std::string
Board::ToString(common::Logger::Level filter_level) const
{
  std::stringstream sstr;
  const size_t rh_width = 4;
  const size_t ch_width = (size_.ColumnCount() + 2) * 2;

  // Board is displayed with the active player's home row (row 0) at
  // the top and opponent player's home row (row n) at the bottom.

  // Active home row is the "top" of the board.
  const std::string& active_name = ActivePlayer()->Name() + " - " + ActivePlayer()->BoardCharacter();
  sstr << std::string(rh_width + ((ch_width - active_name.size()) / 2), ' ') << active_name << '\n';

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

  // Opponent player home row is the "bottom" of the board.
  const std::string& opponent_name = OpponentPlayer()->Name() + " - " + OpponentPlayer()->BoardCharacter();
  sstr << std::string(rh_width + ((ch_width - opponent_name.size()) / 2), ' ') << opponent_name;

  return sstr.str();
}

}} // namespace cifre::celcath
