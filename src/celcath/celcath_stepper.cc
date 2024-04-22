// Copyright 2024, David Goodwin. All rights reserved.

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <sstream>
#include <vector>

#include "celcath_board.h"
#include "celcath_stepper.h"
#include "common_error.h"

namespace cifre { namespace celcath {

//
// AttackStepper
//
const std::string&
AttackStepper::Name() const
{
  static const std::string n("attack");
  return n;
}

void
AttackStepper::Step(Board* board, const std::vector<Player*>& players) const
{
  const auto board_size = board->Size();

  // Collect the sum of neighbor values for each cell, for each player
  // in 'players'. Also add one additional entry to record the sum of
  // all neighbor values across all players.
  const size_t sum_owner_id = players.size();
  std::vector<std::vector<int32_t>> neighbor_values;
  for (size_t nidx = 0; nidx <= players.size(); ++nidx) {
    neighbor_values.emplace_back(std::vector<int32_t>(board_size.CellCount(), 0));
  }

  // Accumulate neighbor values...
  for (size_t ridx = 0; ridx < board_size.RowCount(); ++ridx) {
    for (size_t cidx = 0; cidx < board_size.ColumnCount(); ++cidx) {
      // If no 'owner' for cell (i.e. empty), then can skip it.
      const Player* owner = board->PlayerAt(ridx, cidx);
      if (owner != nullptr) {
        const size_t owner_id = owner->Id();
        assert(players[owner_id] == owner);

        // Add cell value to neighbor cells.
        const float nval = std::abs(board->CellValueAt(ridx, cidx));
        const size_t start_ridx = (ridx == 0) ? 0 : ridx - 1;
        const size_t end_ridx = std::min(ridx + 2, board_size.RowCount());
        const size_t start_cidx = (cidx == 0) ? 0 : cidx - 1;
        const size_t end_cidx = std::min(cidx + 2, board_size.ColumnCount());
        for (size_t nvrow = start_ridx; nvrow < end_ridx; ++nvrow) {
          for (size_t nvcol = start_cidx; nvcol < end_cidx; ++nvcol) {
            if ((nvrow != ridx) || (nvcol != cidx)) {
              const size_t cell_idx = (nvrow * board_size.ColumnCount()) + nvcol;
              neighbor_values[owner_id][cell_idx] += nval;
              neighbor_values[sum_owner_id][cell_idx] += nval;
            }
          }
        }
      }
    }
  }

  LOGGER_LOGHIGH(*logger_,
                 std::string("neighbor values\n") +
                 NeighborValuesToString(board_size, players, neighbor_values));

  // Update board. For each non-active-player cell, compare cell value
  // against the active player neighbor count and reduce the cell if
  // necessary.
  for (size_t fidx = 0; fidx < board_size.CellCount(); ++fidx) {
    const float cv = board->CellValueAtFlattened(fidx);
    if (cv < 0.0) {
      const int32_t nv = neighbor_values[board->ActivePlayerId()][fidx];
      if (std::abs(cv) < nv) {
        board->SetCellValueAtFlattened(cv + 1.0, fidx);
      }
    }
  }
}

std::string
AttackStepper::NeighborValuesToString(
  const cifre::BoardSize2D& board_size,
  const std::vector<Player*>& players,
  const std::vector<std::vector<int32_t>>& nv) const
{
  std::stringstream sstr;

  for (size_t idx = 0; idx <= players.size(); ++idx) {
    if (idx > 0) {
      sstr << '\n';
    }

    if (idx == players.size()) {
      sstr << "Total:\n";
    }
    else {
      sstr << players[idx]->Name() << ":\n";
    }

    for (size_t ridx = 0; ridx < board_size.RowCount(); ++ridx) {
      if (ridx != 0) {
        sstr << '\n';
      }

      for (size_t cidx = 0; cidx < board_size.ColumnCount(); ++cidx) {
        sstr << ' ' << nv[idx][(ridx * board_size.ColumnCount()) + cidx];
      }
    }
  }

  return sstr.str();
}

}} // namespace cifre::celcath
