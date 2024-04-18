// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "cifre_action.h"
#include "cifre_board2d.h"
#include "celcath_player.h"
#include "celcath_stepper.h"

namespace cifre { namespace celcath {

//
// Board
//
// The celcath playing board and all associated state. Each board has
// an active player that it is respective to that player that the
// board state is maintained. Row 0 of the board is the home row of
// the active player, and row 'n' is the home row of the opponent
// player.
//
class Board : public cifre::Board2D {
public:
  // Create board at the required starting state.
  Board(const cifre::BoardSize2D& size, const std::vector<Player*>& players,
        size_t starting_player_id, const std::shared_ptr<Stepper>& stepper);

  // Copy constructor.
  Board(const Board& other) :
    cifre::Board2D(other), step_action_id_(other.step_action_id_),
    stepper_(other.stepper_) { }

  // See cifre::State
  State* Clone() const override { return new Board(*this); }

  // One action for each board cell, plus the "step" action. See
  // cifre::Board2D.
  size_t TotalActionIdCount() const override { return size_.CellCount() + 1; }

  // Update the terminal status for board.
  void UpdateTerminalStatus() override;

  // Update the valid actions for board. See cifre::Board2D.
  void UpdateValidActions() override;

  // See cifre::State
  void Update(uint64_t action_id) override;

  // See cifre::Board2D.
  std::string ActionToString(const Action& action) const override;

  // See cifre::Board2D.
  Action StringToAction(const std::string& str) const override;

  // See cifre::Board2D.
  void SwitchActivePlayer() override;

private:
  float SwitchCellValue(const float v) const;

  const size_t step_action_id_;
  std::shared_ptr<Stepper> stepper_;
};

}} // namespace cifre::celcath
