// Copyright 2025, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "cifre_board2d.h"

namespace cifre {

//
// Board2DHex
//
// A specialization of Board2D for two-player, two-dimensional
// hexagonal game boards.
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
class Board2DHex : public Board2D {
public:
  static constexpr size_t PLAYER_COUNT = 2;

  // Create a board based on a given number of planes, 2D 'size' and
  // 'players', where player ID is the index of a given player in
  // 'players'. 'starting_player_id' indicates the player that will go
  // first.
  Board2DHex(
    const size_t plane_cnt, const cifre::BoardSize2D& size,
    const std::vector<Player*>& players, size_t starting_player_id);

  // Copy constructor.
  Board2DHex(const Board2DHex& other) : Board2D(other) { }

  // Destructor
  virtual ~Board2DHex() = default;

  // Return the ID of the opponent player. There must be exactly two
  // players, so the opponent ID is the opposite of active player ID.
  size_t OpponentPlayerId() const { return (active_player_id_ == 0) ? 1 : 0; }

  // Return the opponent player.
  Player* OpponentPlayer() const { return players_[OpponentPlayerId()]; }

  // See cifre::State.
  virtual std::string ToString() const override;

  // Derived class must implement each method to return a
  // two-character string that gives the content string for the upper
  // and lower part of a given cell.
  virtual std::string BoardCellUpperString(size_t row_idx, size_t col_idx) const = 0;
  virtual std::string BoardCellLowerString(size_t row_idx, size_t col_idx) const = 0;
};

} // namespace cifre
