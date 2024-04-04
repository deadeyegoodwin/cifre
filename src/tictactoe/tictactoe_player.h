// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "cifre_state.h"
#include "cifre_player.h"
#include "common_logger.h"

namespace cifre { namespace tictactoe {

//
// HumanPlayer
//
// A tic-tac-toe player controlled by a human.
//
class HumanPlayer : public cifre::Player {
public:
  // Create 'name'd human player that uses 'ch' as the one character
  // representation. 'neighbor_value' indicates the value to use for
  // neighbor cells occupied by this player.
  HumanPlayer(
    const std::shared_ptr<common::Logger>& logger,
    const std::string& name, char ch, int32_t neighbor_value);

  // See cifre::Player::NextAction().
  Action NextAction(const cifre::State& board, size_t max_game_turns_remaining) const override;

private:
  std::string Usage() const;
};

}} // namespace cifre::tictactoe
