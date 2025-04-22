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
  pass_action_id_(size_.CellCount())
{
  // Start with a block in the center.
  const size_t cmid = size_.ColumnCount() / 2;
  const size_t rmid = size_.RowCount() / 2;
  SetCellValueAt(CELL_ACTIVE_PLAYER, rmid, cmid);
  SetCellValueAt(CELL_ACTIVE_PLAYER, rmid, cmid - 1);
  SetCellValueAt(CELL_ACTIVE_PLAYER, rmid - 1, cmid);
  SetCellValueAt(CELL_ACTIVE_PLAYER, rmid - 1, cmid - 1);

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
      (action_id == pass_action_id_)) {
    return "Pass";
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
    case 'p': return Action(Action::ActionKind::ACTIONID, pass_action_id_);
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
  if (action_id != pass_action_id_) {
    // Set selected cell value.
    const float cv = CellValueAtFlattened(action_id);
    if (cv == CELL_EMPTY) {
      SetCellValueAtFlattened(CELL_ACTIVE_PLAYER, action_id);
    }
    else {
      THROW("unexpected cell value " + std::to_string(cv));
    }
  }

  // Check terminal status and if not at terminal then advance to next
  // active player.
  UpdateTerminalStatus();
  if (terminal_status_.Kind() == cifre::StateTerminalStatus::TerminalKind::NON_TERMINAL) {
    active_player_id_ = (active_player_id_ + 1) % players_.size();

    // If back to the starting player then step the board.
    // FIXME use starting_player_id
    if (active_player_id_ == 0 /* starting_player_id_ */) {
      Step();
    }
  }

  UpdateValidActions();
}

void
Board::UpdateValidActions()
{
  valid_action_ids_.clear();

  // Can play in any empty cell.
  for (size_t ridx = 0; ridx < size_.RowCount(); ++ridx) {
    for (size_t cidx = 0; cidx < size_.ColumnCount(); ++cidx) {
      const size_t fidx = (ridx * size_.ColumnCount()) + cidx;
      if (CellValueAtFlattened(fidx) == CELL_EMPTY) {
          valid_action_ids_.insert(fidx);
      }
    }
  }

  // "pass" action is always available
  valid_action_ids_.insert(pass_action_id_);
}

void
Board::UpdateTerminalStatus()
{
  // Win if at least x% of cells are alive. FIXME determine cell count
  // needed at init time.
  constexpr float x = 0.25;
  const size_t win_count = size_.CellCount() * x;

  // Count number of live cells.
  size_t live_count = 0;
  for (size_t fidx = 0; fidx < size_.CellCount(); ++fidx) {
    if (CellValueAtFlattened(fidx) == CELL_ACTIVE_PLAYER) {
      live_count++;
    }
  }

  if (live_count >= win_count) {
    terminal_status_ =
      cifre::StateTerminalStatus(
        cifre::StateTerminalStatus::TerminalKind::WINNER, active_player_id_);
  }
  else {
    // Otherwise make sure the terminal status is NON_TERMINAL.
    THROWIF(
      terminal_status_.Kind() != cifre::StateTerminalStatus::TerminalKind::NON_TERMINAL,
      "expected NON_TERMINAL status");
  }
}

void
Board::Step()
{
  // Collect the number of neighbors for each cell...
  std::vector<int32_t> neighbor_values(size_.CellCount(), 0);
  for (size_t ridx = 0; ridx < size_.RowCount(); ++ridx) {
    for (size_t cidx = 0; cidx < size_.ColumnCount(); ++cidx) {
      // If cell alive, increment count of all neighbor cells.
      if (CellValueAt(ridx, cidx) == CELL_ACTIVE_PLAYER) {
        const size_t start_ridx = (ridx == 0) ? 0 : ridx - 1;
        const size_t end_ridx = std::min(ridx + 2, size_.RowCount());
        const size_t start_cidx = (cidx == 0) ? 0 : cidx - 1;
        const size_t end_cidx = std::min(cidx + 2, size_.ColumnCount());
        for (size_t nvrow = start_ridx; nvrow < end_ridx; ++nvrow) {
          for (size_t nvcol = start_cidx; nvcol < end_cidx; ++nvcol) {
            if ((nvrow != ridx) || (nvcol != cidx)) {
              const size_t cell_idx = (nvrow * size_.ColumnCount()) + nvcol;
              neighbor_values[cell_idx] += 1;
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

  // Update board using b3s23 rule.
  for (size_t fidx = 0; fidx < size_.CellCount(); ++fidx) {
    const int32_t nv = neighbor_values[fidx];
    if (nv == 3) {
      SetCellValueAtFlattened(CELL_ACTIVE_PLAYER, fidx);
    }
    else if (nv != 2) {
      SetCellValueAtFlattened(CELL_EMPTY, fidx);
    }
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
