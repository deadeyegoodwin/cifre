// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "cifre_action.h"
#include "cifre_board2d_basic.h"
#include "celcath_player.h"

namespace cifre { namespace celcath {

//
// Board
//
// The celcath playing board.
//
class Board : public cifre::Board2DBasic {
public:
  // Create board...
  Board(const cifre::BoardSize2D& size, const std::vector<Player*>& players,
        size_t starting_player_id);

  // Copy constructor.
  Board(const Board& other) :
    cifre::Board2DBasic(other), pass_action_id_(other.pass_action_id_) { }

  // See cifre::State
  State* Clone() const override { return new Board(*this); }

  // See cifre::State
  void Update(uint64_t action_id) override;

  // One action for each board cell, plus the "pass" action.  See
  // cifre::Board2DBasic.
  size_t TotalActionIdCount() const override { return size_.CellCount() + 1; }

  // See cifre::Board2DBasic.
  std::string ActionToString(const Action& action) const override;

  // See cifre::Board2DBasic.
  Action StringToAction(const std::string& str) const override;

private:
  void UpdateTerminalStatus();
  void UpdateValidActions();
  void SwitchActivePlayer();

  void Step();
  std::string NeighborValuesToString(
    const std::vector<std::vector<int32_t>>& nv) const;

  const size_t pass_action_id_;
};

}} // namespace cifre::celcath
