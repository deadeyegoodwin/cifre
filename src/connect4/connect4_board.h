// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "cifre_action.h"
#include "cifre_board2d_basic.h"
#include "connect4_player.h"

namespace cifre { namespace connect4 {

//
// Board
//
// The connect4 playing board. The top of the board as it is displayed
// is row 'n'. Pieces played in a column "fall" to the lowest
// numbered, unoccupied row in that column.
//
class Board : public cifre::Board2DBasic {
public:
  // Create board at the required starting state.
  Board(const cifre::BoardSize2D& size, const std::vector<Player*>& players,
        size_t starting_player_id);

  // See cifre::State
  State* Clone() const override { return new Board(*this); }

  // One action for each board column. See cifre::Board2DBasic.
  size_t TotalActionIdCount() const override { return size_.ColumnCount(); }

  // See cifre::State
  void Update(uint64_t action_id) override;

  // See cifre::Board2DBasic.
  std::string ActionToString(const Action& action) const override;

  // See cifre::Board2DBasic.
  Action StringToAction(const std::string& str) const override;

private:
  void UpdateTerminalStatus();
  void UpdateValidActions();
  void SwitchActivePlayer();
};

}} // namespace cifre::connect4
