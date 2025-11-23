// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "cifre_action.h"
#include "cifre_board2d_basic.h"
#include "othello_player.h"

namespace cifre { namespace othello {

//
// Board
//
// The othello playing board.
//
class Board : public cifre::Board2DBasic {
public:
  // Create board at the required starting state.
  Board(const cifre::BoardSize2D& size, const std::vector<Player*>& players,
        size_t starting_player_id);

  // Copy constructor.
  Board(const Board& other) :
    cifre::Board2DBasic(other), do_nothing_action_id_(other.do_nothing_action_id_),
    active_do_nothing_(other.active_do_nothing_), opponent_do_nothing_(other.opponent_do_nothing_) { }

  // Return the action ID representing "do nothing".
  size_t DoNothingActionId() const { return do_nothing_action_id_; }

  // See cifre::State
  Board* Copy() const override { return new Board(*this); }
  void Update(uint64_t action_id) override;

  // One action for each board cell, plus the "do nothing" action. See
  // cifre::Board2DBasic.
  size_t TotalActionIdCount() const override { return size_.CellCount() + 1; }

  // Return the string representation of an 'action'.  See
  // cifre::Board2DBasic.
  std::string ActionToString(const Action& action) const override;

private:
  Board& operator=(const Board&) = delete;  // assignment not allowed

  void UpdateTerminalStatus();
  void UpdateValidActions();
  void SwitchActivePlayer();

  const size_t do_nothing_action_id_;
  bool active_do_nothing_;
  bool opponent_do_nothing_;
};

}} // namespace cifre::othello
