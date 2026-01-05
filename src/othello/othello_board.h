// Copyright 2025, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "cifre_action.h"
#include "cifre_board2d.h"
#include "othello_player.h"

namespace cifre { namespace othello {

//
// Board
//
// The othello playing board.
//
class Board : public cifre::Board2D {
public:
  // Create board at the required starting state.
  Board(const cifre::Size2D& size, const std::vector<Player*>& players,
        size_t starting_player_id, std::pmr::memory_resource* arena);

  // Return the action ID representing "do nothing".
  size_t DoNothingActionId() const { return do_nothing_action_id_; }

  // See cifre::State
  StateUniquePtr Copy(std::pmr::memory_resource* arena) const override;
  void Update(uint64_t action_id) override;

  // One action for each board cell, plus the "do nothing" action. See
  // cifre::Board2D.
  size_t TotalActionIdCount() const override { return size_.CellCount() + 1; }

  // Return the string representation of an 'action'.  See
  // cifre::Board2D.
  std::string ActionToString(const Action& action) const override;

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
    cifre::Board2D(other, arena), do_nothing_action_id_(other.do_nothing_action_id_),
    active_do_nothing_(other.active_do_nothing_), opponent_do_nothing_(other.opponent_do_nothing_) { }

  void UpdateTerminalStatus();
  void UpdateValidActions();
  void SwitchActivePlayer();

  const size_t do_nothing_action_id_;
  bool active_do_nothing_;
  bool opponent_do_nothing_;
};

}} // namespace cifre::othello
