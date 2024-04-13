// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "cifre_action.h"
#include "cifre_alphazero.h"
#include "common_logger.h"
#include "common_random.h"

namespace cifre {

//
// Player
//
// A player. Typically this serves as a base-class for more
// interesting characters.
//
class Player {
public:
  enum class PlayerKind { UNKNOWN, HUMAN, RANDOM, ALPHAZERO, PERFECT };
  static PlayerKind StringToPlayerKind(const std::string& str);
  static std::string PlayerKindToString(const PlayerKind kind);

public:
  // Create player of given 'kind'. Return nullptr if unable to create
  // the player.
  static Player* Create(
    const std::shared_ptr<common::Logger>& logger,
    const PlayerKind kind, const std::string& name, const char ch,
    const std::shared_ptr<AlphaZero>& alphazero);

  virtual ~Player() = default;

  // Return the player's name.
  const std::string& Name() const { return name_; }

  // Return the one character representation of the player.
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
    const cifre::State& terminal_state, const cifre::StateTerminalStatus& status) {
    THROWIF(status.Kind() == cifre::StateTerminalStatus::TerminalKind::NON_TERMINAL,
            "game ended with NON-TERMINAL status"); }

  // Get the next action for the player given current 'state' and with
  // 'max_game_turns_remaining' indicating the maximum number of turns
  // remaining in the game.
  virtual Action NextAction(const State& state, size_t max_game_turns_remaining) const;

  // Return string representation of the player.
  std::string ToString(common::Logger::Level filter_level = common::Logger::Level::LOW) const;

protected:
  Player(
    const std::shared_ptr<common::Logger>& logger, const std::string& name, char ch);

  std::shared_ptr<common::Logger> logger_;
  const std::string name_;
  const char ch_;
};

//
// AlphaZeroPlayer
//
// A player controlled by alpha-zero.
//
class AlphaZeroPlayer : public Player {
public:
  // Create 'name'd alpha-zero player that uses 'ch' as the one
  // character representation.
  AlphaZeroPlayer(
    const std::shared_ptr<common::Logger>& logger,
    const std::string& name, char ch,
    const std::shared_ptr<cifre::AlphaZero>& alphazero);

  // See Player::NextAction().
  Action NextAction(const State& state, size_t max_game_turns_remaining) const override;

private:
  std::shared_ptr<cifre::AlphaZero> alphazero_;
};

//
// RandomPlayer
//
// A player that randomly chooses actions.
//
class RandomPlayer : public Player {
public:
  // Create 'name'd random player that uses 'ch' as the one character
  // representation.
  RandomPlayer(
    const std::shared_ptr<common::Logger>& logger, const std::string& name, char ch);

  // See Player::NextAction().
  Action NextAction(const State& state, size_t max_game_turns_remaining) const override;

private:
  mutable common::Random rnd_;
};

} // namespace cifre
