// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "celcath_action.h"
#include "cifre_alphazero.h"
#include "common_logger.h"
#include "common_random.h"

namespace cifre { namespace celcath {

class Board;
class Player;

//
// Player
//
// A player. Typically this serves as a base-class for more
// interesting characters.
//
class Player {
public:
  // Create 'name'd human player that uses 'board_char' as the
  // character representing them on the board. 'neighbor_value'
  // indicates the value to use for neighbor cells occupied by this
  // player.
  Player(
    const std::shared_ptr<common::Logger>& logger,
    const std::string& name, char board_char, int32_t neighbor_value);
  virtual ~Player() = default;

  // Return the player's name.
  const std::string& Name() const { return name_; }

  // Return the character that represents the player on the board.
  char BoardCharacter() const { return board_char_; }

  // Return the cell neighbor value for this player.
  int32_t NeighborValue() const { return neighbor_value_; }

  // Called to notify that a new game is starting. Default behavior is
  // to do nothing.
  virtual void GameStart() { }

  // Called to notify that game has ended at 'terminal_state' with a
  // terminal 'status'. Note the 'status' may differ from the actual
  // terminal status of 'terminal_state', but any end-game actions
  // should respect 'status'. 'status' of NON_TERMINAL is not allowed
  // and GameEnd should throw exception if that value is received.
  virtual void GameEnd(
    const cifre::State* terminal_state, cifre::State::TerminalStatusKind status) {
    THROWIF(status == cifre::State::TerminalStatusKind::NON_TERMINAL,
            "game ended with NON-TERMINAL status"); }

  // Get the next action for the player given current 'board' state.
  virtual Action NextAction(const Board& board) const;

  // Return string representation of any player statistics. Default
  // behavior is to return empty string.
  virtual std::string ToStatisticsString(common::Logger::Level filter_level = common::Logger::Level::LOW) const {
    return std::string(); }

  // Return string representation of the player.
  std::string ToString(common::Logger::Level filter_level = common::Logger::Level::LOW) const;

protected:
  std::shared_ptr<common::Logger> logger_;
  const std::string name_;
  const char board_char_;
  const int32_t neighbor_value_;
};

//
// HumanPlayer
//
// A player controlled by a human.
//
class HumanPlayer : public Player {
public:
  // Create 'name'd human player that uses 'board_char' as the
  // character representing them on the board. 'neighbor_value'
  // indicates the value to use for neighbor cells occupied by this
  // player.
  HumanPlayer(
    const std::shared_ptr<common::Logger>& logger,
    const std::string& name, char board_char, int32_t neighbor_value);

  // See Player::NextAction().
  Action NextAction(const Board& board) const override;

private:
  std::string Usage() const;
};

//
// AlphaZeroPlayer
//
// A player controlled by alpha-zero.
//
class AlphaZeroPlayer : public Player {
public:
  // Create 'name'd alpha-zero player that uses 'board_char' as the
  // character representing them on the board. 'neighbor_value'
  // indicates the value to use for neighbor cells occupied by this
  // player.
  AlphaZeroPlayer(
    const std::shared_ptr<common::Logger>& logger,
    const std::string& name, char board_char, int32_t neighbor_value,
    const std::shared_ptr<cifre::AlphaZero>& alphazero);

  // See Player::GameStart().
  void GameStart() override;

  // See Player::GameEnd().
  void GameEnd(const cifre::State* terminal_state, cifre::State::TerminalStatusKind status) override;

  // See Player::NextAction().
  Action NextAction(const Board& board) const override;

  // See Player::ToStatisticsString().
  std::string ToStatisticsString(common::Logger::Level filter_level = common::Logger::Level::LOW) const override;

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
  // Create 'name'd random player that uses 'board_char' as the
  // character representing them on the board. 'neighbor_value'
  // indicates the value to use for neighbor cells occupied by this
  // player.
  RandomPlayer(
    const std::shared_ptr<common::Logger>& logger,
    const std::string& name, char board_char, int32_t neighbor_value);

  // See Player::NextAction().
  Action NextAction(const Board& board) const override;

private:
  mutable common::Random rnd_;
};

}} // namespace cifre::celcath
