// Copyright 2026, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <set>
#include <vector>

#include "cifre_action.h"
#include "cifre_display.h"
#include "cifre_hex.h"
#include "cifre_player.h"
#include "cifre_state.h"
#include "cifre_std.h"
#include "cifre_vec.h"
#include "common_util.h"

namespace cifre {

//
// BoardHex
//
// A specialization of State for two-player, two-dimensional hexagonal
// game boards.
//
// Provides a console representation of the board and methods to
// convert text commands to/from actions. The console representation
// relies on the derived class to provide the edge and "interior"
// content representation of a hex based on the values for that hex
// across all planes.
//
// Acts as a cifre::State with the triton representation being a
// C-order flattening of a 3D space composed of a configurable number
// of planes with each plane being a 2D representation of the board
// with offsets as follows:
//  __    __    __    __
// /0 \__/0 \__/0 \__/0 \_
// \_0/0 \_2/0 \_4/0 \_6/
// /1 \_1/1 \_3/1 \_5/1 \_
// \_0/1 \_2/1 \_4/1 \_6/
// /2 \_1/2 \_3/2 \_5/2 \_
// \_0/2 \_2/2 \_4/2 \_6/
//    \_1/  \_3/  \_5/
//
// For example, the following hex is stored in position [1, 3] in the
// 2D representation:
//  __
// /1 \ ; row-idx = 1
// \_3/ ; column-idx = 3
//
// State is represented as [ row idx, col idx, plane_idx ]. Derived
// classes must implement appropriate action and board update logic
// via State::TotalActionIdCount(), State::StringToAction(),
// State::ActionToString(), and State::Update().
//
// See cifre::State for information about memory management requires
// classes derived from State.
//
class BoardHex : public State {
public:
  static constexpr size_t PLAYER_COUNT = 2;

  // Ways that the board can be display.
  enum class BoardDisplayKind {
    SMALL,          // Small hexes
    LARGE,          // Large hexes
  };

public:
  // Create a board based on a given number of planes, 2D 'size' and
  // 'players', where player ID is the index of a given player in
  // 'players'. 'starting_player_id' indicates the player that will go
  // first. Dynamic allocations required by the board should be
  // performed in 'arena'.
  BoardHex(
    const size_t plane_cnt, const cifre::MatrixSize& size,
    const std::vector<Player*>& players, size_t starting_player_id,
    std::pmr::memory_resource* arena);

  // Construct board as a copy of 'other' using 'arena' for memory
  // required by constructed board.
  BoardHex(const BoardHex& other, std::pmr::memory_resource* arena);

  // Destructor
  virtual ~BoardHex() = default;

  // Return the number of planes required for the board state. Each 2D
  // plane's size is Size().
  size_t PlaneCount() const { return plane_cnt_; }

  // Return the board size.
  const MatrixSize& Size() const { return size_; }

  // Return the player ID that represents "no player". By convention
  // this ID is equal to the number of players in the game.
  size_t NoPlayerId() const { return players_.size(); }

  // Return the ID of the active player. See cifre::State.
  size_t ActivePlayerId() const override { return active_player_id_; }

  // Return the active player.
  Player* ActivePlayer() const { return players_[active_player_id_]; }

  // See cifre::State
  StateTerminalStatus TerminalStatus() const override { return terminal_status_; }

  // See cifre::State
  const std::pmr::vector<uint64_t>& ValidActionIds() const override {
    return valid_action_ids_; }

  // See cifre::State.
  virtual bool IsValidAction(const Action& action) const override;

  // Get or set the way the board is displayed. Board display is
  // controlled by a BoardDisplayKind and a game-specific display
  // version which is an integer used to indicate a "version" of the
  // display. It is up to the game to manage display kind and version.
  BoardDisplayKind DisplayKind() const { return display_kind_; }
  uint32_t DisplayVersion() const { return display_version_; }
  void SetDisplay(BoardDisplayKind kind, uint32_t ver = 0) const {
    display_kind_ = kind; display_version_ = ver; }

  // Return the ID of the opponent player. There must be exactly two
  // players, so the opponent ID is the opposite of active player ID.
  size_t OpponentPlayerId() const { return (active_player_id_ == 0) ? 1 : 0; }

  // Return the opponent player.
  Player* OpponentPlayer() const { return players_[OpponentPlayerId()]; }

  // See cifre::State.
  virtual std::string ToString() const override;

  // Return the header and trailer to display before / after when
  // displaying the board.
  virtual std::string BoardHeader() const;
  virtual std::string BoardTrailer() const;

protected:
  // Don't allow default copy constructor or assignment to force the
  // version above that requires an arena. Don't allow move assignment
  // as it does not move the allocator for any pmr containers (move
  // constructor is ok).
  BoardHex(BoardHex&) = delete;
  BoardHex& operator=(BoardHex&) = delete;
  BoardHex(const BoardHex&) = delete;
  BoardHex& operator=(const BoardHex&) = delete;
  BoardHex& operator=(BoardHex&&) = delete;

  // Board display. The following defines and methods return a
  // 2-dimensional array of char/color pairs that describe the
  // appearance of a board hex. The returned values describe both the
  // edges and the interior content of a cell.
  //
  // Small cells require a 4x4 array of char/color pairs, as follows:
  //
  //  --  ; unused, up-edge(2x), unused
  // /  \ ; up-left-edge, interior(x2), up-right-edge
  // \  / ; down-left-edge, interior(x2), down-right-edge
  //  --  ; unused, down-edge(x2), unused
  //
  // Note that the default BoardHex::ToString() draws the small hex so
  // that the lower-interior and down-edge representation overlap. The
  // default ToString() function will use whichever description is
  // non-' '. If both are non-' ' then will use the interior cell
  // description.
  //
  // Large cells require a 6x8 array of char/color pairs, as follows:
  //
  //   ----   ; unused (x2), up-edge (x4), unused(x2)
  //  /    \  ; unused, up-left-edge, interior(x4), up-right-edge, unused
  // /      \ ; up-left-edge, interior(x6), up-right-edge
  // \      / ; down-left-edge, interior(x6), down-right-edge
  //  \    /  ; unused, down-left-edge, interior(x4), down-right-edge, unused
  //   ----   ; unused (x2), down-edge (x4), unused(x2)


  // Note that the default BoardHex::ToString() draws the large hex so
  // that the lower-interior and down-edge representation overlap. The
  // default ToString() function will use whichever description is
  // non-' '. If both are non-' ' then will use the interior cell
  // description.
  static constexpr size_t SMALL_BOARD_CELL_CONTENT_HEIGHT = 4;
  static constexpr size_t SMALL_BOARD_CELL_CONTENT_WIDTH = 4;
  static constexpr size_t LARGE_BOARD_CELL_CONTENT_HEIGHT = 6;
  static constexpr size_t LARGE_BOARD_CELL_CONTENT_WIDTH = 8;
  using SmallBoardCellContent =
    cifre::array<cifre::array<ColorChar, SMALL_BOARD_CELL_CONTENT_WIDTH>,
                 SMALL_BOARD_CELL_CONTENT_HEIGHT>;
  using LargeBoardCellContent =
    cifre::array<cifre::array<ColorChar, LARGE_BOARD_CELL_CONTENT_WIDTH>,
                 LARGE_BOARD_CELL_CONTENT_HEIGHT>;

  virtual SmallBoardCellContent PositionToSmallCellContent(
    uint32_t display_version, const MatrixCoordinate& pos) const;
  virtual LargeBoardCellContent PositionToLargeCellContent(
    uint32_t display_version, const MatrixCoordinate& pos) const;

  std::string SmallBoardToString() const;
  std::string LargeBoardToString() const;

  // Display kind and version are mutable so that we can change it
  // even in a const State object. It is not part of the state seen by
  // the algorithms or hashing so it is ok to have it mutable.
  mutable BoardDisplayKind display_kind_;
  mutable uint32_t display_version_;

  // Number of planes.
  const size_t plane_cnt_;

  // Size of the board.
  const MatrixSize size_;

  // The players. The player's index in this vector is the player ID.
  cifre::array<cifre::Player*, PLAYER_COUNT> players_;

  // ID of the currently active player.
  size_t active_player_id_;

  // The status of the board indicating winning condition.
  cifre::StateTerminalStatus terminal_status_;

  // The set of action ids that are valid for the current board.
  std::pmr::vector<uint64_t> valid_action_ids_;
};

} // namespace cifre
