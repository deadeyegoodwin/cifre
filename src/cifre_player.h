// Copyright 2025, David Goodwin. All rights reserved.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "cifre_action.h"
#include "cifre_algo.h"
#include "common_logger.h"
#include "common_random.h"

namespace cifre {

//
// Player
//
// Base class for a player.
//
class Player {
public:
  enum class Kind { UNKNOWN, HUMAN, ALGO };

public:
  Player(
    const std::shared_ptr<common::Logger>& logger, size_t id, const std::string& name);
  virtual ~Player() = default;

  // Return the player's ID.
  size_t Id() const { return id_; }

  // Return the player's name.
  const std::string& Name() const { return name_; }

  // Set or return the one character representation of the player.
  void SetChar(char ch) { ch_ = ch; }
  char Char() const { return ch_; }

  // Called to notify that a new game is starting. Default behavior is
  // to do nothing.
  virtual void GameStart() { }

  // Called to notify that game has ended at 'terminal_state' with a
  // terminal 'status'. Note the 'status' may differ from the actual
  // terminal status of 'terminal_state', but any end-game actions
  // should respect 'status'. 'status' of NON_TERMINAL is not allowed
  // and GameEnd should throw exception if that value is received.
  virtual void GameEnd(
    const State& terminal_state, const StateTerminalStatus& status) {
    THROWIF(status.Kind() == StateTerminalStatus::TerminalKind::NON_TERMINAL,
            "game ended with NON-TERMINAL status"); }

  // Get the next action for the player given current 'state' and with
  // 'max_game_sideturns_remaining' indicating the maximum number of
  // side-turns remaining in the game. 'max_game_sideturns_remaining'
  // indicates the maximum number of game side-turns remaining. A
  // "side-turn" is a player taking a turn. A game with a maximum
  // lenth of N turns, with P players will have N*P side-turns.
  virtual Action NextAction(const State& state, size_t max_game_sideturns_remaining) const;

  // Return string representation of the player.
  std::string ToString(common::Logger::Level filter_level = common::Logger::Level::LOW) const;

protected:
  std::shared_ptr<common::Logger> logger_;
  const size_t id_;
  const std::string name_;
  char ch_;
};

//
// AlgoPlayer
//
// A player controlled by a cifre::Algo.
//
class AlgoPlayer : public Player {
public:
  AlgoPlayer(
    const std::shared_ptr<common::Logger>& logger,
    size_t id, const std::string& name, const std::shared_ptr<cifre::Algo>& algo);

  // See Player::NextAction().
  Action NextAction(const State& state, size_t max_game_sideturns_remaining) const override;

private:
  std::shared_ptr<cifre::Algo> algo_;
};

} // namespace cifre
