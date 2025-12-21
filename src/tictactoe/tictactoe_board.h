// Copyright 2025, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "cifre_action.h"
#include "cifre_board2d.h"
#include "tictactoe_player.h"

namespace cifre { namespace tictactoe {

//
// Board
//
// The tictactoe playing board.
//
class Board : public cifre::Board2D {
public:
  // Create board...
  Board(const cifre::Size2D& size, const std::vector<Player*>& players,
        size_t starting_player_id);

  // Copy constructor.
  Board(const Board& other) : cifre::Board2D(other) { }

  // See cifre::State
  Board* Copy() const override { return new Board(*this); }
  void Update(uint64_t action_id) override;

private:
  Board& operator=(const Board&) = delete;  // assignment not allowed

  void UpdateTerminalStatus();
  void UpdateValidActions();
  void SwitchActivePlayer();
};

}} // namespace cifre::tictactoe
