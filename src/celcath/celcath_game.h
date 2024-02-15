// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <cstddef>
#include <cstdint>

#include "celcath_board.h"
#include "celcath_player.h"
#include "celcath_stepper.h"
#include "common_logger.h"

namespace cifre { namespace celcath {

class Game;

//
// Game
//
// A game.
//
class Game {
public:
  // Create game that uses 'board' and alternates moves across 'players'.
  Game(common::Logger* logger, Stepper* stepper,
       Board* board, const std::vector<Player*>& players);

  // Play the game.
  void Play();

private:
  common::Logger* logger_;
  Stepper* stepper_;
  Board* board_;
  std::vector<Player*> players_;
};

}} // namespace cifre::celcath
