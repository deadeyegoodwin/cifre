// Copyright 2025, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "cifre_action.h"
#include "cifre_board2d_hex.h"
#include "gettysburg_player.h"

namespace cifre { namespace gettysburg {

//
// Board
//
// The gettysburg playing board.
//
class Board : public cifre::Board2DHex {
public:
  // Create board...
  Board(const cifre::BoardSize2D& size, const std::vector<Player*>& players,
        size_t starting_player_id);

  // Copy constructor.
  Board(const Board& other) : cifre::Board2DHex(other) { }

  // See cifre::State
  State* Clone() const override { return new Board(*this); }

  // See cifre::State
  void Update(uint64_t action_id) override;

  // FIXME
  size_t TotalActionIdCount() const override;

  // FIXME
  std::string ActionToString(const Action& action) const override;

  // FIXME
  Action StringToAction(const std::string& str) const override;

private:
  void UpdateTerminalStatus();
  void UpdateValidActions();
};

}} // namespace cifre::gettysburg
