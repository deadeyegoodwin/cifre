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
Board::Board(const cifre::BoardSize2D& size, const std::vector<Player*>& players,
             size_t starting_player_id) :
  cifre::Board2DBasic(size, players, starting_player_id),
  step_action_id_(size_.CellCount())
{
  // Each player starts with a block in corner.
  const size_t clast = size_.ColumnCount() - 1;
  const size_t rlast = size_.RowCount() - 1;
  SetCellValueAt(CELL_ACTIVE_PLAYER, 0, 0);
  SetCellValueAt(CELL_ACTIVE_PLAYER, 0, 1);
  SetCellValueAt(CELL_ACTIVE_PLAYER, 1, 0);
  SetCellValueAt(CELL_ACTIVE_PLAYER, 1, 1);
  SetCellValueAt(CELL_OPPONENT_PLAYER, rlast, clast);
  SetCellValueAt(CELL_OPPONENT_PLAYER, rlast, clast - 1);
  SetCellValueAt(CELL_OPPONENT_PLAYER, rlast - 1, clast);
  SetCellValueAt(CELL_OPPONENT_PLAYER, rlast - 1, clast - 1);

  // Initialize 'valid_action_ids_' and 'terminal_status_'.
  UpdateValidActions();
  UpdateTerminalStatus();
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
    Step();
  }
  else {
    // Increment selected cell value.
    const float cv = CellValueAtFlattened(action_id);
    if (cv == CELL_EMPTY) {
      SetCellValueAtFlattened(CELL_ACTIVE_PLAYER, action_id);
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

  // Can play in any empty cell.
  for (size_t ridx = 0; ridx < size_.RowCount(); ++ridx) {
    for (size_t cidx = 0; cidx < size_.ColumnCount(); ++cidx) {
      const size_t fidx = (ridx * size_.ColumnCount()) + cidx;
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
  // Check winning conditions and set 'terminal_status_'. Win if other
  // side has no live cells. Logic is 2-player specific.
  assert(players_.size() == 2);

  // Count number of active and opponent live cells.
  size_t active_count = 0;
  size_t opponent_count = 0;
  for (size_t ridx = 0; ridx < size_.RowCount(); ++ridx) {
    for (size_t cidx = 0; cidx < size_.ColumnCount(); ++cidx) {
      const float cv = CellValueAt(ridx, cidx);
      if (cv == CELL_ACTIVE_PLAYER) {
        active_count++;
      }
      else if (cv == CELL_OPPONENT_PLAYER) {
        opponent_count++;
      }
    }
  }

  const bool opponent_win = (active_count == 0);
  const bool active_win = (opponent_count == 0);

  if (active_win && !opponent_win) {
    terminal_status_ =
      cifre::StateTerminalStatus(
        cifre::StateTerminalStatus::TerminalKind::WINNER, active_player_id_);
  }
  else if (opponent_win && !active_win) {
    terminal_status_ =
      cifre::StateTerminalStatus(
        cifre::StateTerminalStatus::TerminalKind::WINNER, OpponentPlayerId());
  }
  else if (opponent_win && active_win) {
    terminal_status_ =
      cifre::StateTerminalStatus(cifre::StateTerminalStatus::TerminalKind::DRAW);
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

void
Board::Step()
{
  // Collect the number of neighbors for each cell, for each player in
  // 'players'. Also add one additional entry to record the sum of all
  // neighbors across all players.

  // Logic is 2-player specific.
  assert(players_.size() == 2);

  const size_t sum_owner_id = players_.size();
  std::vector<std::vector<int32_t>> neighbor_values;
  for (size_t nidx = 0; nidx <= players_.size(); ++nidx) {
    neighbor_values.emplace_back(std::vector<int32_t>(size_.CellCount(), 0));
  }

  // Accumulate neighbor values...
  for (size_t ridx = 0; ridx < size_.RowCount(); ++ridx) {
    for (size_t cidx = 0; cidx < size_.ColumnCount(); ++cidx) {
      // If no 'owner' for cell (i.e. empty), then can skip it.
      const Player* owner = PlayerAt(ridx, cidx);
      if (owner != nullptr) {
        const size_t owner_id = owner->Id();
        assert(players_[owner_id] == owner);

        // Increment count of all neighbor cells to show that 'owner'
        // is a neighbor.
        const size_t start_ridx = (ridx == 0) ? 0 : ridx - 1;
        const size_t end_ridx = std::min(ridx + 2, size_.RowCount());
        const size_t start_cidx = (cidx == 0) ? 0 : cidx - 1;
        const size_t end_cidx = std::min(cidx + 2, size_.ColumnCount());
        for (size_t nvrow = start_ridx; nvrow < end_ridx; ++nvrow) {
          for (size_t nvcol = start_cidx; nvcol < end_cidx; ++nvcol) {
            if ((nvrow != ridx) || (nvcol != cidx)) {
              const size_t cell_idx = (nvrow * size_.ColumnCount()) + nvcol;
              neighbor_values[owner_id][cell_idx] += 1;
              neighbor_values[sum_owner_id][cell_idx] += 1;
            }
          }
        }
      }
    }
  }

  // FIXME
//  LOGGER_LOGHIGH(*logger_,
//                 std::string("neighbor values\n") +
//                 NeighborValuesToString(size_, players_, neighbor_values));

  // Update board using b3s23 rule for each player neighbor counts.
  const size_t opponent_player_id = OpponentPlayerId();
  for (size_t fidx = 0; fidx < size_.CellCount(); ++fidx) {
    // If both players want to be born in the cell then keep the cell
    // as it is.
    const int32_t total_nv = neighbor_values[sum_owner_id][fidx];
    if (total_nv >= 3) {
      const int32_t active_nv = neighbor_values[active_player_id_][fidx];
      const int32_t opponent_nv = neighbor_values[opponent_player_id][fidx];
      if ((active_nv == 3) && (opponent_nv == 3)) {
        continue;
      }
      else if (active_nv == 3) {
        SetCellValueAtFlattened(CELL_ACTIVE_PLAYER, fidx);
        continue;
      }
      else if (opponent_nv == 3) {
        SetCellValueAtFlattened(CELL_OPPONENT_PLAYER, fidx);
        continue;
      }
    }

    // If cell is alive and current owner has 2 or 3 neighbors, then
    // keep it alive.
    if (total_nv >= 2) {
      const Player* owner = PlayerAtFlattened(fidx);
      if (owner != nullptr) {
        const int32_t owner_nv = neighbor_values[owner->Id()][fidx];
        if ((owner_nv == 2) || (owner_nv == 3)) {
          continue;
        }
      }
    }

    SetCellValueAtFlattened(CELL_EMPTY, fidx);
  }
}

std::string
Board::NeighborValuesToString(const std::vector<std::vector<int32_t>>& nv) const
{
  std::stringstream sstr;

  for (size_t idx = 0; idx <= players_.size(); ++idx) {
    if (idx > 0) {
      sstr << '\n';
    }

    if (idx == players_.size()) {
      sstr << "Total:\n";
    }
    else {
      sstr << players_[idx]->Name() << ":\n";
    }

    for (size_t ridx = 0; ridx < size_.RowCount(); ++ridx) {
      if (ridx != 0) {
        sstr << '\n';
      }

      for (size_t cidx = 0; cidx < size_.ColumnCount(); ++cidx) {
        sstr << ' ' << nv[idx][(ridx * size_.ColumnCount()) + cidx];
      }
    }
  }

  return sstr.str();
}

}} // namespace cifre::celcath
