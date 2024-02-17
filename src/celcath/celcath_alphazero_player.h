// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <cstddef>
#include <cstdint>

#include "celcath_player.h"
#include "common_logger.h"

namespace cifre { namespace celcath {

class AlphaZeroPlayer;

//
// AlphaZeroPlayer
//
// A player controlled buy alpha-zero algorithm.
//
class AlphaZeroPlayer : public Player {
public:
  // Create 'name'd player that uses 'board_char' as the character
  // representing them on the board. 'neighbor_value' indicates the
  // value to use for neighbor cells occupied by this player.
  AlphaZeroPlayer(common::Logger* logger, const std::string& name, char board_char,
                  const std::string& model_name);
};

}} // namespace cifre::celcath
