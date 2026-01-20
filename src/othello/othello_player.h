// Copyright 2026, David Goodwin. All rights reserved.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "cifre_logger.h"
#include "cifre_player.h"
#include "cifre_state.h"

namespace cifre { namespace othello {

//
// HumanPlayer
//
// An othello player controlled by a human.
//
class HumanPlayer : public cifre::Player {
public:
  // Create 'name'd human player.
  HumanPlayer(const std::shared_ptr<Logger>& logger,
              size_t id, const std::string& name);

  // See cifre::Player
  Kind PlayerKind() const override { return Kind::HUMAN; }

  // See cifre::Player::NextAction().
  Action NextAction(const cifre::State& board, size_t max_game_turns_remaining) const override;

private:
  std::string Usage() const;
};

}} // namespace cifre::othello
