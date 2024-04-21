// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "cifre_action.h"
#include "cifre_board2d_basic.h"
#include "tictactoe_player.h"

namespace cifre { namespace tictactoe {

//
// Board
//
// The tictactoe playing board.
//
class Board : public cifre::Board2DBasic {
public:
  // Create board...
  Board(const cifre::BoardSize2D& size, const std::vector<Player*>& players,
        size_t starting_player_id);

  // Copy constructor.
  Board(const Board& other) : cifre::Board2DBasic(other) { }

  // See cifre::State
  State* Clone() const override { return new Board(*this); }

  // Update the terminal status for board. See cifre::Board2DBasic.
  void UpdateTerminalStatus() override;

  // Update the valid actions for board. See cifre::Board2DBasic.
  void UpdateValidActions() override;

  // See cifre::State
  void Update(uint64_t action_id) override;
};

}} // namespace cifre::tictactoe
