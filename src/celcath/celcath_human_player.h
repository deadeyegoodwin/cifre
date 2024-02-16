// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <cstddef>
#include <cstdint>

#include "celcath_player.h"
#include "common_logger.h"

namespace cifre { namespace celcath {

class HumanPlayer;

//
// HumanPlayer
//
// A human player.
//
class HumanPlayer : public Player {
public:
  // Create 'name'd player that uses 'board_char' as the character
  // representing them on the board. 'neighbor_value' indicates the
  // value to use for neighbor cells occupied by this player.
  HumanPlayer(const std::string& name, char board_char, int32_t neighbor_value = 1.0) :
    Player(name, board_char, neighbor_value) { }
};

}} // namespace cifre::celcath
