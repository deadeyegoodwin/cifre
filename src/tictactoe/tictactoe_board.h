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

  // See cifre::State
  void Update(uint64_t action_id) override;

private:
  void UpdateTerminalStatus();
  void UpdateValidActions();
  void SwitchActivePlayer();
};

}} // namespace cifre::tictactoe
