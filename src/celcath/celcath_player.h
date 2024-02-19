// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "cifre_alphazero.h"
#include "common_logger.h"

namespace cifre { namespace celcath {

class Player;

//
// Player
//
// A player.
//
class Player {
public:
  // Player to use for a cell that should always be dead.
  static Player DEAD;

  // Player to use for a cell that should always be live.
  static Player LIVE;

  // Create 'name'd human player that uses 'board_char' as the
  // character representing them on the board. 'neighbor_value'
  // indicates the value to use for neighbor cells occupied by this
  // player.
  Player(const std::string& name, char board_char, int32_t neighbor_value);

  // Create 'name'd alpha-zero player that uses 'board_char' as the
  // character representing them on the board. 'neighbor_value'
  // indicates the value to use for neighbor cells occupied by this
  // player.
  Player(const std::string& name, char board_char, int32_t neighbor_value,
         std::unique_ptr<cifre::AlphaZero> alpha_zero);

  // Return the player's name.
  const std::string& Name() const { return name_; }

  // Return the character that represents the player on the board.
  char BoardCharacter() const { return board_char_; }

  // Return the cell neighbor value for this player.
  int32_t NeighborValue() const { return neighbor_value_; }

  // Return string representation of the player.
  std::string ToString(common::Logger::Level filter_level) const;

protected:
  const std::string name_;
  const char board_char_;
  const int32_t neighbor_value_;

  // Will be nullptr for players not using alpha-zero.
  std::unique_ptr<cifre::AlphaZero> alpha_zero_;
};

}} // namespace cifre::celcath
