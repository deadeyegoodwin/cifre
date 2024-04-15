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
// B3S23Stepper
//
const std::string&
B3S23Stepper::Name() const
{
  static const std::string n("b3s23");
  return n;
}

void
B3S23Stepper::Step(Board* board, const std::vector<Player*>& players) const
{
  // For now the "neighbor value" assigned to an occupied cell is 1
  // (and value for an empty cell is implicitly 0).
  static constexpr int32_t NEIGHBOR_VALUE = 1;

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
      // If no 'owner' for cell (i.e. empty) or 'owner' doesn't
      // contribute any neighbor value, then can skip it.
      const Player* owner = board->PlayerAt(ridx, cidx);
      if ((owner != nullptr) && (NEIGHBOR_VALUE != 0)) {
        const size_t owner_id = board->PlayerIdAt(ridx, cidx);
        assert(players[owner_id] == owner);

        // Add 'owner's neighbor value to neighbor cells.
        const int32_t nval = NEIGHBOR_VALUE;
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

  // Update board based on the neighbor value of a cell.
  for (size_t nvcell = 0; nvcell < board_size.CellCount(); ++nvcell) {
    const int32_t nv = neighbor_values[sum_owner_id][nvcell];

    // If 3 neighbors, cell lives...
    if (nv == 3) {
      // If cell is already alive, then we can either keep it as is,
      // or we can have it be taken over by the player that has the
      // majority of neighbors. FIXME ruleset: for now just keep owner
      // as is when 3 neighbors.
      Player* player = board->PlayerAtFlattened(nvcell);
      if (player == nullptr) {
        // The player that has the majority neighbor count gets the
        // cell, if no majority then cells stays dead.
        int32_t max_neighbor_cnt = 0;
        for (size_t idx = 0; idx < players.size(); ++idx) {
          const int32_t pnv = neighbor_values[idx][nvcell];
          if (pnv > max_neighbor_cnt) {
            max_neighbor_cnt = pnv;
            player = players[idx];
          }
          else if (pnv == max_neighbor_cnt) {
            player = nullptr;
          }
        }

        board->SetPlayerAtFlattened(player, nvcell);
      }
    }
    // If 2 neighbors, cell stays as it is; otherwise cell is dead.
    else if (nv != 2) {
      board->SetPlayerAtFlattened(nullptr, nvcell);
    }
  }
}

std::string
B3S23Stepper::NeighborValuesToString(
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
