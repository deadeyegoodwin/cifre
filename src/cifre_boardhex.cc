// Copyright 2026, David Goodwin. All rights reserved.

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
  display_kind_(BoardDisplayKind::SMALL), display_version_(0),
  plane_cnt_(plane_cnt), size_(size), active_player_id_(starting_player_id),
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
  display_kind_(other.display_kind_), display_version_(other.display_version_),
  plane_cnt_(other.plane_cnt_), size_(other.size_), players_(other.players_),
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
  case BoardDisplayKind::SMALL:  return SmallBoardToString();
  case BoardDisplayKind::LARGE:  return LargeBoardToString();
  }

  return "";  // not reached
}

std::string
BoardHex::SmallBoardToString() const
{
  std::stringstream sstr;

  //  __    __    __    __
  // /0 \__/0 \__/0 \__/0 \_
  // \_0/0 \_2/0 \_4/0 \_6/
  // /1 \_1/1 \_3/1 \_5/1 \_
  // \_0/1 \_2/1 \_4/1 \_6/
  // /2 \_1/2 \_3/2 \_5/2 \_
  // \_0/2 \_2/2 \_4/2 \_6/
  // /  \_1/  \_3/  \_5/  \_

  const auto& dsize = DisplaySize(display_kind_, display_version_);

  // Top of first row of hex cells.
  for (size_t col_idx = 0; col_idx < dsize.ColumnCount(); col_idx += 2) {
    const auto content = PositionToSmallCellContent(
      display_version_, MatrixCoordinate(0, col_idx));
    sstr << ((col_idx == 0) ? " " : "    ");
    sstr << ColorCharsToString(content[0][1], content[0][2]);
  }
  sstr << "\n";

  // Rows...
  for (size_t row_idx = 0; row_idx < dsize.RowCount(); ++row_idx) {
    for (size_t col_idx = 0; col_idx < dsize.ColumnCount(); ++col_idx) {
      if ((col_idx % 2) == 0) {
        const auto content = PositionToSmallCellContent(
          display_version_, MatrixCoordinate(row_idx, col_idx));
        sstr << ColorCharsToString(content[1]);
      }
      else if (row_idx > 0) {
        auto content = PositionToSmallCellContent(
          display_version_, MatrixCoordinate(row_idx - 1, col_idx));
        sstr
          << ColorCharsToString(
            content[2][1].Overlay(content[3][1]), content[2][2].Overlay(content[3][2]));
      }
      else {
        const auto content = PositionToSmallCellContent(
          display_version_, MatrixCoordinate(row_idx, col_idx));
        sstr << ColorCharsToString(content[0][1], content[0][2]);
      }
    }
    if ((row_idx != 0) && ((dsize.ColumnCount() % 2) == 0)) {
      const auto content = PositionToSmallCellContent(
        display_version_, MatrixCoordinate(row_idx - 1, dsize.ColumnCount() - 1));
      sstr << ColorCharsToString(content[2][3]);
    }
    sstr << "\n";

    for (size_t col_idx = 0; col_idx < dsize.ColumnCount(); ++col_idx) {
      auto content = PositionToSmallCellContent(
        display_version_, MatrixCoordinate(row_idx, col_idx));
      if ((col_idx % 2) == 0) {
        sstr
          << ColorCharsToString(content[2][0], content[2][1].Overlay(content[3][1]),
                                content[2][2].Overlay(content[3][2]), content[2][3]);
      }
      else {
        sstr << ColorCharsToString(content[1][1], content[1][2]);
        if (((dsize.ColumnCount() % 2) == 0) && (col_idx == (dsize.ColumnCount() - 1))) {
          sstr << ColorCharsToString(content[1][3]);
        }
      }
    }
    sstr << "\n";
  }

  // Lower half of last row.
  sstr << " ";
  for (size_t col_idx = 1; col_idx < dsize.ColumnCount(); col_idx += 2) {
    auto content = PositionToSmallCellContent(
      display_version_, MatrixCoordinate(dsize.RowCount() - 1, col_idx));
    sstr
      << "  "
      << ColorCharsToString(content[2][0], content[2][1].Overlay(content[3][1]),
                            content[2][2].Overlay(content[3][2]), content[2][3]);
  }

  return sstr.str();
}

std::string
BoardHex::LargeBoardToString() const
{
  std::stringstream sstr;

  // Like the small board but bigger hexes so can show more
  // information...
  //   ____        ____
  //  /    \      /    \    -> loc 0
  // /      \____/      \   -> loc 1
  // \      /    \      /   -> loc 2
  //  \____/      \____/    -> loc 3
  //       \      /
  //        \____/

  const auto& dsize = DisplaySize(display_kind_, display_version_);

  // Top of first row of hex cells.
  for (size_t col_idx = 0; col_idx < dsize.ColumnCount(); col_idx += 2) {
    const auto content = PositionToLargeCellContent(
      display_version_, MatrixCoordinate(0, col_idx));
    sstr << ((col_idx == 0) ? "  " : "        ");
    sstr << ColorCharsToString(content[0][2], content[0][3], content[0][4], content[0][5]);
  }
  sstr << "\n";

  // Rows...
  for (size_t row_idx = 0; row_idx < dsize.RowCount(); ++row_idx) {
    // Loc 0...
    for (size_t col_idx = 0; col_idx < dsize.ColumnCount(); ++col_idx) {
      if ((col_idx % 2) == 0) {
        const auto content = PositionToLargeCellContent(
          display_version_, MatrixCoordinate(row_idx, col_idx));
        if (col_idx == 0) {
          sstr << " ";
        }
        sstr
          << ColorCharsToString(content[1][1], content[1][2], content[1][3],
                                content[1][4], content[1][5], content[1][6]);
      }
      else if (row_idx > 0) {
        const auto content = PositionToLargeCellContent(
          display_version_, MatrixCoordinate(row_idx - 1, col_idx));
        sstr
          << ColorCharsToString(content[3][1], content[3][2], content[3][3],
                                content[3][4], content[3][5], content[3][6]);
      }
      else {
        sstr << "      ";
      }
    }
    if ((row_idx != 0) && ((dsize.ColumnCount() % 2) == 0)) {
      const auto content = PositionToLargeCellContent(
        display_version_, MatrixCoordinate(row_idx, dsize.ColumnCount() - 1));
      sstr << ColorCharsToString(content[3][7]);
    }
    sstr << "\n";

    // Loc 1...
    for (size_t col_idx = 0; col_idx < dsize.ColumnCount(); ++col_idx) {
      if ((col_idx % 2) == 0) {
        const auto content = PositionToLargeCellContent(
          display_version_, MatrixCoordinate(row_idx, col_idx));
        sstr << ColorCharsToString(
          content[2][0], content[2][1], content[2][2], content[2][3],
          content[2][4], content[2][5], content[2][6], content[2][7]);
      }
      else if (row_idx > 0) {
        auto content = PositionToLargeCellContent(
          display_version_, MatrixCoordinate(row_idx - 1, col_idx));
        sstr << ColorCharsToString(
          content[4][2].Overlay(content[5][2]), content[4][3].Overlay(content[5][3]),
          content[4][4].Overlay(content[5][4]), content[4][5].Overlay(content[5][5]));
      }
      else {
        const auto content = PositionToLargeCellContent(
          display_version_, MatrixCoordinate(row_idx, col_idx));
        sstr << ColorCharsToString(
          content[0][2], content[0][3], content[0][4], content[0][5]);
      }
    }
    if ((row_idx != 0) && ((dsize.ColumnCount() % 2) == 0)) {
      const auto content = PositionToLargeCellContent(
        display_version_, MatrixCoordinate(row_idx, dsize.ColumnCount() - 1));
      sstr << ColorCharsToString(content[4][6]);
    }
    sstr << "\n";

    // Loc 2...
    for (size_t col_idx = 0; col_idx < dsize.ColumnCount(); ++col_idx) {
      const auto content = PositionToLargeCellContent(
        display_version_, MatrixCoordinate(row_idx, col_idx));
      if ((col_idx % 2) == 0) {
        sstr << ColorCharsToString(
          content[3][0], content[3][1], content[3][2], content[3][3],
          content[3][4], content[3][5], content[3][6], content[3][7]);
      }
      else {
        sstr << ColorCharsToString(
          content[1][2], content[1][3], content[1][4], content[1][5]);
      }
    }
    if ((dsize.ColumnCount() % 2) == 0) {
      const auto content = PositionToLargeCellContent(
        display_version_, MatrixCoordinate(row_idx, dsize.ColumnCount() - 1));
      sstr << ColorCharsToString(content[1][6]);
    }
    sstr << "\n";

    // Loc 3...
    for (size_t col_idx = 0; col_idx < dsize.ColumnCount(); ++col_idx) {
      auto content = PositionToLargeCellContent(
        display_version_, MatrixCoordinate(row_idx, col_idx));
      if ((col_idx % 2) == 0) {
        if (col_idx == 0) {
          sstr << " ";
        }
        sstr << ColorCharsToString(
          content[4][1], content[4][2].Overlay(content[5][2]),
          content[4][3].Overlay(content[5][3]), content[4][4].Overlay(content[5][4]),
          content[4][5].Overlay(content[5][5]), content[4][6]);
      }
      else {
        sstr << ColorCharsToString(
          content[2][1], content[2][2], content[2][3],
          content[2][4], content[2][5], content[2][6]);
      }
    }
    if ((dsize.ColumnCount() % 2) == 0) {
      const auto content = PositionToLargeCellContent(
        display_version_, MatrixCoordinate(row_idx, dsize.ColumnCount() - 1));
      sstr << ColorCharsToString(content[2][7]);
    }
    sstr << "\n";
  }

  // Lower half of last row.
  sstr << "  ";
  for (size_t col_idx = 1; col_idx < dsize.ColumnCount(); col_idx += 2) {
    const auto content = PositionToLargeCellContent(
      display_version_, MatrixCoordinate(dsize.RowCount() - 1, col_idx));
    sstr
      << "    "
      << ColorCharsToString(
        content[3][0], content[3][1], content[3][2], content[3][3],
        content[3][4], content[3][5], content[3][6], content[3][7]);
  }
  sstr << "\n ";
  for (size_t col_idx = 1; col_idx < dsize.ColumnCount(); col_idx += 2) {
    auto content = PositionToLargeCellContent(
      display_version_, MatrixCoordinate(dsize.RowCount() - 1, col_idx));
    sstr
      << "      "
      << ColorCharsToString(
        content[4][1], content[4][2].Overlay(content[5][2]),
        content[4][3].Overlay(content[5][3]), content[4][4].Overlay(content[5][4]),
        content[4][5].Overlay(content[5][5]), content[4][6]);
  }

  return sstr.str();
}

BoardHex::SmallBoardCellContent
BoardHex::PositionToSmallCellContent(
  uint32_t display_version, const MatrixCoordinate& pos) const
{
  const std::string row_str = std::to_string(pos.Row());
  const std::string col_str = std::to_string(pos.Column());

  SmallBoardCellContent content;

  content[0] = {
    ColorChar(' '), ColorChar('_'), ColorChar('_'), ColorChar(' ') };
  content[1] = {
    ColorChar('/'), ColorChar(row_str[0]),
    ColorChar((row_str.size() == 1) ? ' ' : row_str[1]), ColorChar('\\') };
  if (col_str.size() == 1) {
    content[2] = {
      ColorChar('\\'), ColorChar(' '), ColorChar(col_str[0]), ColorChar('/') };
  }
  else {
    content[2] =
      { ColorChar('\\'), ColorChar(col_str[0]), ColorChar(col_str[1]),
        ColorChar('/') };
  }

  content[3] = {
    ColorChar(' '), ColorChar('_'), ColorChar('_'), ColorChar(' ') };

  return content;
}

BoardHex::LargeBoardCellContent
BoardHex::PositionToLargeCellContent(
  uint32_t display_version, const MatrixCoordinate& pos) const
{
  const std::string row_str = std::to_string(pos.Row());
  const std::string col_str = std::to_string(pos.Column());

  LargeBoardCellContent content;

  content[0] = {
    ColorChar(' '), ColorChar(' '),
    ColorChar('_'), ColorChar('_'), ColorChar('_'), ColorChar('_'),
    ColorChar(' '), ColorChar(' ') };
  content[1] = {
    ColorChar(' '), ColorChar('/'),
    ColorChar(' '), ColorChar(' '), ColorChar(' '), ColorChar(' '),
    ColorChar('\\'), ColorChar(' ') };
  content[2] = {
    ColorChar('/'), ColorChar(' '), ColorChar(' '),
    ColorChar(row_str[0]), ColorChar((row_str.size() == 1) ? ' ' : row_str[1]),
    ColorChar(' '), ColorChar(' '), ColorChar('\\') };
  if (col_str.size() == 1) {
    content[3] = {
      ColorChar('\\'), ColorChar(' '), ColorChar(' '), ColorChar(' '),
      ColorChar(col_str[0]), ColorChar(' '), ColorChar(' '), ColorChar('/') };
  }
  else {
    content[3] = {
      ColorChar('\\'), ColorChar(' '), ColorChar(' '), ColorChar(' '),
      ColorChar(col_str[0]), ColorChar(col_str[1]), ColorChar(' '), ColorChar('/') };
  }
  content[4] = {
    ColorChar(' '), ColorChar('\\'),
    ColorChar(' '), ColorChar(' '), ColorChar(' '), ColorChar(' '),
    ColorChar('/'), ColorChar(' ') };
  content[5] = {
    ColorChar(' '), ColorChar(' '),
    ColorChar('_'), ColorChar('_'), ColorChar('_'), ColorChar('_'),
    ColorChar(' '), ColorChar(' ') };

  return content;
}

} // namespace cifre
