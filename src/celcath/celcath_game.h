// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "celcath_board.h"
#include "celcath_player.h"
#include "celcath_stepper.h"
#include "cifre_game.h"
#include "common_logger.h"

namespace cifre { namespace celcath {

class Game;

//
// Game
//
// A game. A Game object is not thread-safe; typically usage is for a
// single thread to own and interact with a given Game object.
//
class Game : public cifre::Game {
public:
  // Create game that uses 'board' and alternates moves across 'players'.
  Game(common::Logger* logger, std::unique_ptr<Stepper> stepper, Board&& board,
       std::vector<const Player*>&& players);

  // Play the game.
  void Play();

  // From cifre::Game
  uint64_t TotalActionCount() const override;
  const std::vector<uint64_t>& StateActions(const State& state) const override;
  void ApplyAction(State* state) const override;

private:
  common::Logger* logger_;
  std::unique_ptr<Stepper> stepper_;
  Board board_;
  std::vector<const Player*> players_;

  uint64_t total_action_count_;
};

}} // namespace cifre::celcath
