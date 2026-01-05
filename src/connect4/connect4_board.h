// Copyright 2025, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "cifre_action.h"
#include "cifre_board2d.h"
#include "connect4_player.h"

namespace cifre { namespace connect4 {

//
// Board
//
// The connect4 playing board. The top of the board as it is displayed
// is row 'n'. Pieces played in a column "fall" to the lowest
// numbered, unoccupied row in that column.
//
class Board : public cifre::Board2D {
public:
  // Create board at the required starting state.
  Board(const cifre::Size2D& size, const std::vector<Player*>& players,
        size_t starting_player_id, std::pmr::memory_resource* arena);

  // See cifre::State
  StateUniquePtr Copy(std::pmr::memory_resource* arena) const override;
  void Update(uint64_t action_id) override;

  // One action for each board column. See cifre::Board2D.
  size_t TotalActionIdCount() const override { return size_.ColumnCount(); }

  // See cifre::Board2D.
  std::string ActionToString(const Action& action) const override;

  // See cifre::Board2D.
  Action StringToAction(const std::string& str) const override;

private:
  // Don't allow default copy constructor or assignment to force the
  // version below that requires an arena. Don't allow move assignment
  // as it does not move the allocator for any pmr containers (move
  // constructor is ok).
  Board(Board& other) = delete;
  Board& operator=(Board&) = delete;
  Board(const Board& other) = delete;
  Board& operator=(const Board&) = delete;
  Board& operator=(Board&& other) = delete;

  // Construct board as a copy of 'other' using 'arena' for memory
  // required by constructed board. This constructor is kept private
  // so that Copy() must be used, see cifre::State.
  Board(const Board& other, std::pmr::memory_resource* arena) :
    Board2D(other, arena) { }

  void UpdateTerminalStatus();
  void UpdateValidActions();
  void SwitchActivePlayer();
};

}} // namespace cifre::connect4
