// Copyright 2025, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <set>
#include <vector>

#include "cifre_action.h"
#include "cifre_player.h"
#include "cifre_size.h"
#include "cifre_state.h"
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
// relies on the derived class to provide the "interior" content
// representation of a hex based on the values for that hex across all
// planes.
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
  enum class DisplayKind {
    SMALL,          // Small hexes with minimal content
    LARGE,          // Large hexes
    LARGE_VERBOSE   // Large hexes with additional debug-level content
  };

  // Hex directions.
  enum class Direction { UP=0, UP_RT=1, DN_RT=2, DN=3, DN_LT=4, UP_LT=5, NONE=6 };
  static constexpr size_t DIRECTION_COUNT = 6;

  static std::string DirectionToString(const Direction d) {
    switch (d) {
    case Direction::UP: return "UP";
    case Direction::UP_RT: return "UP-RT";
    case Direction::DN_RT: return "DN-RT";
    case Direction::DN: return "DN";
    case Direction::DN_LT: return "DN-LT";
    case Direction::UP_LT: return "UP-LT";
    case Direction::NONE: return "NONE";
    }
    return "<none>"; // unreachable
  }

public:
  // Create a board based on a given number of planes, 2D 'size' and
  // 'players', where player ID is the index of a given player in
  // 'players'. 'starting_player_id' indicates the player that will go
  // first. Dynamic allocations required by the board should be
  // performed in 'arena'.
  BoardHex(
    const size_t plane_cnt, const cifre::Size2D& size,
    const std::vector<Player*>& players, size_t starting_player_id,
    std::pmr::memory_resource* arena);

  // Construct board as a copy of 'other' using 'arena' for memory
  // required by constructed board.
  BoardHex(const BoardHex& other, std::pmr::memory_resource* arena);

  // Destructor
  virtual ~BoardHex() = default;

  // See cifre::State. Hash based on 'active_player_id_' and 'cells_'.
  virtual size_t Hash() const override;

  // See cifre::State. Equally is determined by 'active_player_id_'
  // and 'cells_'.
  virtual bool Equal(const State* other) const override;

  // Return the number of planes required for the board state. Each 2D
  // plane's size is Size().
  size_t PlaneCount() const { return plane_cnt_; }

  // Return the board size.
  const Size2D& Size() const { return size_; }

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

  // Get or set the way the board is displayed.
  DisplayKind Display() const { return display_kind_; }
  void SetDisplay(DisplayKind d) const { display_kind_ = d; }

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

  // Return a set of strings that give the content for a given
  // cell. Default behavior is to show the row and column index in the
  // cell. 'kind' indicates the kind of content to return.
  //
  //   SMALL: Return two, two-character strings that give the upper and lower
  //          content of a small hex
  //   LARGE: Return four strings of lengths 4, 6, 6, and 4 that give
  //          the top to bottom content of a large hex
  //   LARGE_VERBOSE: Same as LARGE but return verbose content
  virtual std::vector<std::string> BoardCell(
    DisplayKind kind, size_t row_idx, size_t col_idx) const;

protected:
  // Don't allow default copy constructor or assignment to force the
  // version above that requires an arena.
  BoardHex(BoardHex& other) = delete;
  BoardHex& operator=(BoardHex&) = delete;
  BoardHex(const BoardHex& other) = delete;
  BoardHex& operator=(const BoardHex&) = delete;

  std::string SmallBoard() const;
  std::string LargeBoard() const;

  // Display kind is mutable so that we can change it even in a const
  // State object. It is not part of the state seen by the algorithms
  // or hashing so it is ok to have it mutable.
  mutable DisplayKind display_kind_;

  // Number of planes.
  const size_t plane_cnt_;

  // Size of the board.
  const Size2D size_;

  // The players. The player's index in this vector is the player ID.
  std::array<cifre::Player*, PLAYER_COUNT> players_;

  // ID of the currently active player.
  size_t active_player_id_;

  // The status of the board indicating winning condition.
  cifre::StateTerminalStatus terminal_status_;

  // The set of action ids that are valid for the current board.
  std::pmr::vector<uint64_t> valid_action_ids_;
};

} // namespace cifre
