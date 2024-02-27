// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <cstddef>
#include <cstdint>

#include "celcath_action.h"
#include "celcath_player.h"
#include "celcath_settings.h"
#include "cifre_board2d.h"
#include "common_logger.h"

namespace cifre { namespace celcath {

class Board;

//
// BoardStatus
//
class BoardStatus : public cifre::State::Status {
public:
  BoardStatus(bool is_terminal) : is_terminal_(is_terminal) { }

  // See cifre::State::Status
  bool IsTerminal() const override { return is_terminal_; }

private:
  friend class Board;
  bool is_terminal_;
};

//
// Board
//
// The celcath playing board and all associated state.
//
class Board : public cifre::Board2D<float> {
public:
  // Create a board based on 'settings'.
  Board(const Settings& settings, const std::vector<Player*>& players);

  // Copy constructor.
  Board(const Board& other) = default;

  // Return the ID of the active player.
  size_t ActivePlayerId() const { return active_player_id_; }

  // Return the number of actions that the active player has completed
  // this turn, and the minimum and maximum allowed actions for this
  // turn.
  size_t CompletedActionsThisTurn() const { return complete_actions_this_turn_; }
  size_t MinActionsThisTurn() const { return settings_.MinActionsPerTurn(); }
  size_t MaxActionsThisTurn() const { return settings_.MaxActionsPerTurn(); }

  // Is 'action' valid for the current board state.
  bool IsValidAction(const Action& action) const;

  // Return the Action associated with an action identifier.
  Action ActionIdToAction(uint64_t action_id) const;

  // Return the Action associated with a string.
  Action StringToAction(const std::string& str) const;

  // Return the string representation of an 'action'.
  std::string ActionToString(
    const Action& action, common::Logger::Level filter_level = common::Logger::Level::LOW) const;

  // See cifre::State::Status
  const cifre::State::Status& CurrentStatus() const override {
    return status_; }

  // See cifre::State
  size_t TotalActionIdCount() const override {
    return settings_.TotalActionIdCount(); }

  // See cifre::State
  const std::unordered_set<uint64_t>& ValidActionIds() const override {
    return valid_action_ids_; }

  // See cifre::State
  void Update(uint64_t action_id) override;

  // See cifre::State
  std::string ToString(common::Logger::Level filter_level = common::Logger::Level::LOW) const override;

private:
  static constexpr float CELL_EMPTY = 0.0;
  static constexpr float CELL_PLAYER0 = 1.0;
  static constexpr float CELL_PLAYER1 = -1.0;

  // Convert player ID to/from the corresponding cell value.
  float PlayerIdToCellValue(size_t pid) const {
    return (pid == 0) ? CELL_PLAYER0 : CELL_PLAYER1;
  }
  size_t PlayerIdFromCellValue(float v) const {
    if (v == CELL_PLAYER0) { return 0; }
    if (v == CELL_PLAYER1) { return 1; }
    THROW("cannot convert cell value " + std::to_string(v) + " to player ID");
  }
  const Player& PlayerFromCellValue(float v) const {
    if (v == CELL_PLAYER0) { return *players_[0]; }
    if (v == CELL_PLAYER1) { return *players_[1]; }
    if (v == CELL_EMPTY) { return Player::DEAD; }
    THROW("cannot convert cell value " + std::to_string(v) + " to player");
  }

  // Settings shared across all Board instances.
  const Settings& settings_;

  // Players shared across all Board instances.
  const std::vector<cifre::celcath::Player*>& players_;

  // Current status of this state.
  BoardStatus status_;

  // ID of the currently active player.
  size_t active_player_id_;

  // Active player gets some number of actions each turn.
  size_t complete_actions_this_turn_;

  // The set of action ids that are valid for the current board.
  std::unordered_set<uint64_t> valid_action_ids_;
};

}} // namespace cifre::celcath
