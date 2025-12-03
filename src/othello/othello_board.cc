// Copyright 2025, David Goodwin. All rights reserved.

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>

#include "othello_board.h"
#include "common_error.h"
#include "common_util.h"

namespace cifre { namespace othello {

//
// Board
//
Board::Board(const cifre::BoardSize2D& size, const std::vector<Player*>& players,
             size_t starting_player_id) :
  cifre::Board2DBasic(size, players, starting_player_id),
  do_nothing_action_id_(size_.CellCount()),
  active_do_nothing_(false), opponent_do_nothing_(false)
{
  // Each player has two pieces diagonal from each other in the middle
  // of the board. Board must have even number of rows and columns
  // Othello when parsing board size.
  const size_t iidx = size_.RowCount() / 2;
  SetCellValueAt(CELL_ACTIVE_PLAYER, iidx, iidx);
  SetCellValueAt(CELL_ACTIVE_PLAYER, iidx - 1, iidx - 1);
  SetCellValueAt(CELL_OPPONENT_PLAYER, iidx, iidx - 1);
  SetCellValueAt(CELL_OPPONENT_PLAYER, iidx - 1, iidx);

  // Initialize 'valid_action_ids_' and 'terminal_status_'.
  UpdateValidActions();
  UpdateTerminalStatus();
}

std::string
Board::ActionToString(const Action& action) const
{
  if ((action.Kind() == Action::ActionKind::ACTIONID) &&
      (action.ActionId() == do_nothing_action_id_)) {
    return "NO MOVE";
  }

  return cifre::Board2DBasic::ActionToString(action);
}

void
Board::Update(uint64_t action_id)
{
  if (action_id != do_nothing_action_id_) {
    // Cell corresponding to action must be empty.
    THROWIF(CellValueAtFlattened(action_id) != CELL_EMPTY,
            "player " + ActivePlayer()->Name() + " cannot occupy cell owned by " +
            PlayerAtFlattened(action_id)->Name());

    // Occupy the cell corresponding to 'action_id' and flip all the
    // outflanked opponent cells.
    SetCellValueAtFlattened(CELL_ACTIVE_PLAYER, action_id);

    size_t total_outflank_count = 0;

    const size_t ridx = action_id / size_.RowCount();
    const size_t cidx = action_id % size_.RowCount();
    const size_t start_ridx = (ridx == 0) ? 0 : ridx - 1;
    const size_t end_ridx = std::min(ridx + 2, size_.RowCount());
    const size_t start_cidx = (cidx == 0) ? 0 : cidx - 1;
    const size_t end_cidx = std::min(cidx + 2, size_.ColumnCount());
    for (size_t nridx = start_ridx; nridx < end_ridx; ++nridx) {
      for (size_t ncidx = start_cidx; ncidx < end_cidx; ++ncidx) {
        if (CellValueAt(nridx, ncidx) == CELL_OPPONENT_PLAYER) {
          const int dr = nridx - ridx;
          const int dc = ncidx - cidx;
          assert((dr != 0) || (dc != 0));
          size_t sridx = nridx;
          size_t scidx = ncidx;
          int dist = std::min(
            (dr == 0) ? size_.RowCount() : ((dr < 0) ? sridx : (size_.RowCount() - sridx - 1)),
            (dc == 0) ? size_.ColumnCount() : ((dc < 0) ? scidx : (size_.ColumnCount() - scidx - 1)));
          size_t outflank_count = 0;
          while (dist > 0) {
            sridx += dr;
            scidx += dc;
            ++outflank_count;

            const float cv = CellValueAt(sridx, scidx);
            if (cv == CELL_EMPTY) {
              break; // no outflank, try another direction
            }
            else if (cv == CELL_ACTIVE_PLAYER) {
              total_outflank_count += outflank_count;
              // Outflank between originally empty cell at [ridx,
              // cidx] and [sridx, scidx]. Flip outflanked cells.
              for (; outflank_count > 0; --outflank_count) {
                sridx -= dr;
                scidx -= dc;
                assert(PlayerAt(sridx, scidx) == OpponentPlayer());
                SetCellValueAt(CELL_ACTIVE_PLAYER, sridx, scidx);
              }
              break;
            }

            --dist;
          }
        }
      }
    }

    assert(total_outflank_count > 0);
  }

  // Check terminal status and if not at terminal then advance to next
  // active player.
  UpdateTerminalStatus();
  if (terminal_status_.Kind() == cifre::StateTerminalStatus::TerminalKind::NON_TERMINAL) {
    // Switch board state to represent the "other" player. This also
    // calls UpdateValidActions() to get updated actions and also set
    // 'active_do_nothing_'. 'opponent_do_nothing_' becomes what is
    // currently 'active_do_nothing_'.
    opponent_do_nothing_ = active_do_nothing_;
    SwitchActivePlayer();
  }
}

void
Board::UpdateValidActions()
{
  valid_action_ids_.clear();

  // An action is valid if it represents occupying an empty cell that
  // outflanks one or more opponents. From each active player cell, A,
  // that has an opponent neighbor, check cells in that direction and
  // if an empty cell, E, is encountered before an active cell,
  // then can play in cell E.
  for (size_t ridx = 0; ridx < size_.RowCount(); ++ridx) {
    for (size_t cidx = 0; cidx < size_.ColumnCount(); ++cidx) {
      if (CellValueAt(ridx, cidx) == CELL_ACTIVE_PLAYER) {
        const size_t start_ridx = (ridx == 0) ? 0 : ridx - 1;
        const size_t end_ridx = std::min(ridx + 2, size_.RowCount());
        const size_t start_cidx = (cidx == 0) ? 0 : cidx - 1;
        const size_t end_cidx = std::min(cidx + 2, size_.ColumnCount());
        for (size_t nridx = start_ridx; nridx < end_ridx; ++nridx) {
          for (size_t ncidx = start_cidx; ncidx < end_cidx; ++ncidx) {
            if (CellValueAt(nridx, ncidx) == CELL_OPPONENT_PLAYER) {
              const int dr = nridx - ridx;
              const int dc = ncidx - cidx;
              assert((dr != 0) || (dc != 0));
              size_t sridx = nridx;
              size_t scidx = ncidx;
              int dist = std::min(
                (dr == 0) ? size_.RowCount() : ((dr < 0) ? sridx : (size_.RowCount() - sridx - 1)),
                (dc == 0) ? size_.ColumnCount() : ((dc < 0) ? scidx : (size_.ColumnCount() - scidx - 1)));
              while (dist > 0) {
                sridx += dr;
                scidx += dc;
                const float cv = CellValueAt(sridx, scidx);
                if (cv == CELL_ACTIVE_PLAYER) {
                  break; // no outflank
                }
                else if (cv == CELL_EMPTY) {
                  const size_t cell_idx = (sridx * size_.ColumnCount()) + scidx;
                  valid_action_ids_.insert(cell_idx);
                  break; // outflank
                }

                --dist;
              }
            }
          }
        }
      }
    }
  }

  // If unable to find any cell where a move can be made, then must
  // "do nothing".
  active_do_nothing_ = valid_action_ids_.empty();
  if (active_do_nothing_) {
    valid_action_ids_.insert(do_nothing_action_id_);
  }
}

void
Board::SwitchActivePlayer()
{
  // Swap cell values for players.
  for (size_t fidx = 0; fidx < size_.CellCount(); ++fidx) {
    const float cv = CellValueAtFlattened(fidx);
    if (cv != CELL_EMPTY) {
      const float ncv =
        ((cv == CELL_ACTIVE_PLAYER) ? CELL_OPPONENT_PLAYER : CELL_ACTIVE_PLAYER);
      SetCellValueAtFlattened(ncv, fidx);
    }
  }

  // Update the valid actions for the now-active player.
  active_player_id_ = (active_player_id_ + 1) % players_.size();
  UpdateValidActions();
}

void
Board::UpdateTerminalStatus()
{
  // Check winning conditions and set 'terminal_status_'.
  size_t active_count = 0;
  size_t opponent_count = 0;
  for (size_t cidx = 0; cidx < size_.CellCount(); ++cidx) {
    const float cv = CellValueAtFlattened(cidx);
    if (cv == CELL_ACTIVE_PLAYER) {
      active_count++;
    }
    else if (cv == CELL_OPPONENT_PLAYER) {
      opponent_count++;
    }
  }

  // If neither player can move or all squares taken, then game is over.
  if ((active_do_nothing_ && opponent_do_nothing_) ||
      ((active_count + opponent_count) == size_.CellCount())) {
    if (active_count > opponent_count) {
      terminal_status_ =
        cifre::StateTerminalStatus(
          cifre::StateTerminalStatus::TerminalKind::WINNER, ActivePlayerId());
    }
    else if (opponent_count > active_count) {
      terminal_status_ =
        cifre::StateTerminalStatus(
          cifre::StateTerminalStatus::TerminalKind::WINNER, OpponentPlayerId());
    }
    else {
      terminal_status_ =
        cifre::StateTerminalStatus(cifre::StateTerminalStatus::TerminalKind::DRAW);
    }
  }
  else {
    THROWIF(
      terminal_status_.Kind() != cifre::StateTerminalStatus::TerminalKind::NON_TERMINAL,
      "expected NON_TERMINAL status");
  }
}

}} // namespace cifre::othello
