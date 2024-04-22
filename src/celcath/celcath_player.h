// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "cifre_state.h"
#include "cifre_player.h"
#include "common_logger.h"

namespace cifre { namespace celcath {

//
// HumanPlayer
//
// An celcath player controlled by a human.
//
class HumanPlayer : public cifre::Player {
public:
  // Create 'name'd human player that uses 'ch' as the one character
  // representation.
  HumanPlayer(const std::shared_ptr<common::Logger>& logger,
              size_t id, const std::string& name, char ch);

  // See cifre::Player::NextAction().
  Action NextAction(const cifre::State& board, size_t max_game_turns_remaining) const override;

private:
  std::string Usage() const;
};

}} // namespace cifre::celcath
