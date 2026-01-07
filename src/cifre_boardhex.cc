// Copyright 2025, David Goodwin. All rights reserved.

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>
#include <sstream>

#include "cifre_boardhex.h"
#include "common_error.h"

namespace cifre {

//
// BoardHex
//
BoardHex::BoardHex(
  const size_t plane_cnt, const cifre::MatrixSize& size,
  const std::vector<Player*>& players, size_t starting_player_id,
  std::pmr::memory_resource* arena) :
  display_kind_(DisplayKind::SMALL), plane_cnt_(plane_cnt), size_(size),
  active_player_id_(starting_player_id),
  terminal_status_(cifre::StateTerminalStatus::TerminalKind::NON_TERMINAL),
  valid_action_ids_(arena)
{
  THROWIF(players.size() != PLAYER_COUNT, "exactly 2 players are required");
  THROWIF(NoPlayerId() != PLAYER_COUNT, "exactly 2 players are required");
  THROWIF(active_player_id_ >= NoPlayerId(),
          "invalid starting player index " + std::to_string(active_player_id_));

  for (size_t idx = 0; idx < PLAYER_COUNT; ++idx) {
    players_[idx] = players[idx];
  }

#ifndef NDEBUG
  for (size_t pidx = 0; pidx < players_.size(); ++pidx) {
    assert(players_[pidx]->Id() == pidx);
  }
#endif
}

BoardHex::BoardHex(const BoardHex& other, std::pmr::memory_resource* arena) :
  display_kind_(other.display_kind_), plane_cnt_(other.plane_cnt_),
  size_(other.size_), players_(other.players_),
  active_player_id_(other.active_player_id_), terminal_status_(other.terminal_status_),
  valid_action_ids_(other.valid_action_ids_.begin(), other.valid_action_ids_.end(), arena)
{
}

bool
BoardHex::IsValidAction(const Action& action) const
{
  switch (action.Kind()) {
  case Action::ActionKind::INVALID:
    return false;

  case Action::ActionKind::HELP:
  case Action::ActionKind::QUIT:
  case Action::ActionKind::SHOW_STATE:
  case Action::ActionKind::CUSTOM_0:
  case Action::ActionKind::CUSTOM_1:
  case Action::ActionKind::CUSTOM_2:
  case Action::ActionKind::CUSTOM_3:
    return true;

  case Action::ActionKind::ACTIONID:
    return std::find(
      valid_action_ids_.begin(), valid_action_ids_.end(), action.ActionId()) != valid_action_ids_.end();
  }

  return false;
}

std::string
BoardHex::ToString() const
{
  switch (display_kind_) {
  case DisplayKind::SMALL:
    return SmallBoard();
  case DisplayKind::LARGE:
  case DisplayKind::LARGE_VERBOSE:
    return LargeBoard();
  }

  return "";  // not reached
}

std::string
BoardHex::SmallBoard() const
{
  std::stringstream sstr;

  sstr << BoardHeader() << "\n";

  //  __    __    __    __
  // /0 \__/0 \__/0 \__/0 \_
  // \_0/0 \_2/0 \_4/0 \_6/
  // /1 \_1/1 \_3/1 \_5/1 \_
  // \_0/1 \_2/1 \_4/1 \_6/
  // /2 \_1/2 \_3/2 \_5/2 \_
  // \_0/2 \_2/2 \_4/2 \_6/
  // /  \_1/  \_3/  \_5/  \_

  // Top of first row of hex cells.
  for (size_t col_idx = 0; col_idx < size_.ColumnCount(); col_idx += 2) {
    if (col_idx == 0) {
      sstr << " __";
    }
    else {
      sstr << "    __";
    }
  }
  sstr << "\n";

  // Rows...
  for (size_t row_idx = 0; row_idx < size_.RowCount(); ++row_idx) {
    for (size_t col_idx = 0; col_idx < size_.ColumnCount(); ++col_idx) {
      if ((col_idx % 2) == 0) {
        sstr << "/" << BoardCell(display_kind_, row_idx, col_idx)[0] << "\\";
      }
      else if (row_idx > 0) {
        sstr << BoardCell(display_kind_, row_idx - 1, col_idx)[1];
      }
      else {
        sstr << "__";
      }
    }
    if ((row_idx != 0) && ((size_.ColumnCount() % 2) == 0)) {
      sstr << "/";
    }
    sstr << "\n";

    for (size_t col_idx = 0; col_idx < size_.ColumnCount(); ++col_idx) {
      if ((col_idx % 2) == 0) {
        sstr << "\\" << BoardCell(display_kind_, row_idx, col_idx)[1] << "/";
      }
      else {
        sstr << BoardCell(display_kind_, row_idx, col_idx)[0];
      }
    }
    if ((size_.ColumnCount() % 2) == 0) {
      sstr << "\\";
    }
    sstr << "\n";
  }

  // Lower half of last row.
  sstr << " ";
  for (size_t col_idx = 1; col_idx < size_.ColumnCount(); col_idx += 2) {
    sstr << "  \\" << BoardCell(display_kind_, size_.RowCount() - 1, col_idx)[1] << "/";
  }
  sstr << "\n";

  sstr << BoardTrailer();

  return sstr.str();
}

std::string
BoardHex::LargeBoard() const
{
  std::stringstream sstr;

  sstr << BoardHeader() << "\n";

  // Like the small board but bigger hexes so can show more
  // information...
  //   ____        ____
  //  /    \      /    \    -> str 0
  // /      \____/      \   -> str 1
  // \      /    \      /   -> str 2
  //  \____/      \____/    -> str 3
  //       \      /
  //        \____/

  // Top of first row of hex cells.
  for (size_t col_idx = 0; col_idx < size_.ColumnCount(); col_idx += 2) {
    if (col_idx == 0) {
      sstr << "  ____";
    }
    else {
      sstr << "        ____";
    }
  }
  sstr << "\n";

  // Rows...
  for (size_t row_idx = 0; row_idx < size_.RowCount(); ++row_idx) {
    // Loc 0...
    for (size_t col_idx = 0; col_idx < size_.ColumnCount(); ++col_idx) {
      if ((col_idx % 2) == 0) {
        if (col_idx == 0) {
          sstr << " ";
        }
        sstr << "/" << BoardCell(display_kind_, row_idx, col_idx)[0] << "\\";

      }
      else if (row_idx > 0) {
        sstr << BoardCell(display_kind_, row_idx - 1, col_idx)[2];
      }
      else {
        sstr << "      ";
      }
    }
    if ((row_idx != 0) && ((size_.ColumnCount() % 2) == 0)) {
      sstr << "/";
    }
    sstr << "\n";

    // Loc 1...
    for (size_t col_idx = 0; col_idx < size_.ColumnCount(); ++col_idx) {
      if ((col_idx % 2) == 0) {
        sstr << "/" << BoardCell(display_kind_, row_idx, col_idx)[1] << "\\";
      }
      else if (row_idx > 0) {
        sstr << BoardCell(display_kind_, row_idx - 1, col_idx)[3];
      }
      else {
        sstr << "____";
      }
    }
    if ((row_idx != 0) && ((size_.ColumnCount() % 2) == 0)) {
      sstr << "/";
    }
    sstr << "\n";

    // Loc 2...
    for (size_t col_idx = 0; col_idx < size_.ColumnCount(); ++col_idx) {
      auto cstr = BoardCell(display_kind_, row_idx, col_idx);
      if ((col_idx % 2) == 0) {
        sstr << "\\" << cstr[2] << "/";
      }
      else {
        sstr << cstr[0];
      }
    }
    if ((size_.ColumnCount() % 2) == 0) {
      sstr << "\\";
    }
    sstr << "\n";

    // Loc 3...
    for (size_t col_idx = 0; col_idx < size_.ColumnCount(); ++col_idx) {
      auto cstr = BoardCell(display_kind_, row_idx, col_idx);
      if ((col_idx % 2) == 0) {
        if (col_idx == 0) {
          sstr << " ";
        }
        sstr << "\\" << cstr[3] << "/";
      }
      else {
        sstr << cstr[1];
      }
    }
    if ((size_.ColumnCount() % 2) == 0) {
      sstr << "\\";
    }
    sstr << "\n";
  }

  // Lower half of last row.
  sstr << "  ";
  for (size_t col_idx = 1; col_idx < size_.ColumnCount(); col_idx += 2) {
    sstr << "    \\" << BoardCell(display_kind_, size_.RowCount() - 1, col_idx)[2] << "/";
  }
  sstr << "\n ";
  for (size_t col_idx = 1; col_idx < size_.ColumnCount(); col_idx += 2) {
    sstr << "      \\" << BoardCell(display_kind_, size_.RowCount() - 1, col_idx)[3] << "/";
  }
  sstr << "\n";

  sstr << BoardTrailer();

  return sstr.str();
}

std::string
BoardHex::BoardHeader() const
{
  return "";
}

std::string
BoardHex::BoardTrailer() const
{
  return "";
}

std::vector<std::string>
BoardHex::BoardCell(DisplayKind kind, size_t row_idx, size_t col_idx) const
{
  std::string upper = std::to_string(row_idx);
  if (upper.size() == 1) {
    upper += " ";
  }

  std::string lower = std::to_string(col_idx);
  if (lower.size() == 1) {
    lower = "_" + lower;
  }

  return { upper, lower };
}

} // namespace cifre
