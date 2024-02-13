// Copyright 2024, David Goodwin. All rights reserved.

#include <cstddef>
#include <cstdint>
#include <string>
#include <sstream>
#include <vector>

#include "celcath_player.h"
#include "celcath_stepper.h"
#include "common_error.h"

namespace celcath {

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
B3S23Stepper::Step(Board* board, const std::vector<Player*>& players)
{
  auto board_size = board->Size();
  auto board_size_with_border = board->SizeWithBorder();

  // Collect the sum of neighbor values for each cell, for each player
  // in 'players'. Also add one additional entry to record the sum of
  // all neighbor values across the players and for neighbors that are
  // not in 'players' (like the borders).
  const size_t neighbor_values_sum_idx = players.size();
  std::vector<std::vector<std::vector<int32_t>>> neighbor_values;
  for (size_t nidx = 0; nidx < board_size.RowCount(); ++nidx) {
    std::vector<std::vector<int32_t>> column_players;
    column_players.resize(board_size.ColumnCount(),
                          std::vector<int32_t>(players.size() + 1, 0));
    neighbor_values.emplace_back(std::move(column_players));
  }

  // Accumulate neighbor values...
  for (size_t row_idx = 0; row_idx < board_size_with_border.RowCount(); ++row_idx) {
    for (size_t col_idx = 0; col_idx < board_size_with_border.ColumnCount(); ++col_idx) {
      Cell* cell = board->CellAtWithBorder(row_idx, col_idx);
      THROWIF(cell == nullptr, "stepper failed to get cell");

      // If 'cell' is empty or doesn't contribute any neighbor value,
      // then can skip it.
      Player* owner = cell->Owner();
      if ((owner != nullptr) && (owner->NeighborValue() != 0)) {
        // Get 'owner' index in 'neighbor_values' as we want to
        // attribute these neighbor counts in that index. If 'owner'
        // is not in 'players' then it must be something like a border
        // so we attribute it to 'neighbor_values_sum_idx' only.
        size_t owner_idx = neighbor_values_sum_idx;
        for (size_t idx = 0; idx < players.size(); ++idx) {
          if (players[idx] == owner) {
            owner_idx = idx;
            break;
          }
        }

        // Add 'owner's neighbor value to neighbor cells. We don't
        // aggregate neighbor values for the border cells because they
        // never change due to generation stepping.
        const int32_t nval = owner->NeighborValue();
        const size_t start_row_idx = (row_idx <= 2) ? 0 : row_idx - 2;
        const size_t end_row_idx = std::min(row_idx + 1, board_size.RowCount());
        const size_t start_col_idx = (col_idx <= 2) ? 0 : col_idx - 2;
        const size_t end_col_idx = std::min(col_idx + 1, board_size.ColumnCount());
        for (size_t nvrow = start_row_idx; nvrow < end_row_idx; ++nvrow) {
          for (size_t nvcol = start_col_idx; nvcol < end_col_idx; ++nvcol) {
            if ((nvrow != (row_idx - 1)) || (nvcol != (col_idx - 1))) {
              neighbor_values[nvrow][nvcol][owner_idx] += nval;
              if (owner_idx != neighbor_values_sum_idx) {
                neighbor_values[nvrow][nvcol][neighbor_values_sum_idx] += nval;
              }
            }
          }
        }
      }
    }
  }

  LOGGER_LOGMED(*logger_, std::string("neighbor values"));
  LOGGER_LOGMED(*logger_, NeighborValuesToString(players, neighbor_values));

  // Update board based on the neighbor value of a cell. Border cells
  // are never updated.
  for (size_t nvrow = 0; nvrow < board_size.RowCount(); ++nvrow) {
    for (size_t nvcol = 0; nvcol < board_size.ColumnCount(); ++nvcol) {
      const int32_t nv = neighbor_values[nvrow][nvcol][neighbor_values_sum_idx];

      // If 3 neighbors, cell lives...
      if (nv == 3) {
        // If cell is already alive, then we can either keep it as is,
        // or we can have it be taken over by the player that has the
        // majority of neighbors. FIXME ruleset: for now just keep
        // owner as is when 3 neighbors.
        Cell* cell = board->CellAt(nvrow, nvcol);
        THROWIF(cell == nullptr, "stepper failed to get cell");
        Player* player = cell->Owner();
        if (player == nullptr) {
          // The player that has the majority neighbor count gets the
          // cell, if no majority then cells stays dead.
          int32_t max_neighbor_cnt = 0;
          for (size_t idx = 0; idx < players.size(); ++idx) {
            const int32_t pnv = neighbor_values[nvrow][nvcol][idx];
            if (pnv > max_neighbor_cnt) {
              max_neighbor_cnt = pnv;
              player = players[idx];
            }
            else if (pnv == max_neighbor_cnt) {
              player = nullptr;
            }
          }

          cell->SetOwner(player);
        }
      }
      // If 2 neighbors, cell stays as it is; otherwise cell is dead.
      else if (nv != 2) {
        Cell* cell = board->CellAt(nvrow, nvcol);
        THROWIF(cell == nullptr, "stepper failed to get cell");
        cell->SetOwner(nullptr);
      }
    }
  }
}

std::string
B3S23Stepper::NeighborValuesToString(
  const std::vector<Player*>& players,
  const std::vector<std::vector<std::vector<int32_t>>>& nv) const
{
  std::stringstream sstr;

  for (size_t idx = 0; idx < (players.size() + 1); ++idx) {
    if (idx > 0) {
      sstr << '\n';
    }

    if (idx == players.size()) {
      sstr << "Total:\n";
    } else {
      sstr << players[idx]->Name() << ":\n";
    }

    size_t row_idx = 0;
    for (const auto& r : nv) {
      if (row_idx > 0) {
        sstr << '\n';
      }

      for (const auto& c : r) {
        sstr << ' ' << c[idx];
      }

      row_idx++;
    }
  }

  return sstr.str();
}

} // celcath
