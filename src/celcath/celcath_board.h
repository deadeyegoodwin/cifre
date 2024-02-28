// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "celcath_action.h"
#include "celcath_player.h"
#include "celcath_settings.h"
#include "celcath_stepper.h"
#include "cifre_board2d.h"
#include "common_logger.h"

namespace cifre { namespace celcath {

class Board;

//
// Board
//
// The celcath playing board and all associated state.
//
class Board : public cifre::Board2D<float> {
public:
  //
  // Status
  //
  class Status : public cifre::State::Status {
  public:
    Status(bool is_terminal = false) :
      is_terminal_(is_terminal), is_draw_(true) { }

    // See cifre::State::Status
    bool IsTerminal() const override { return is_terminal_; }

    // If the game a draw? Valid only if IsTerminal == true.
    bool IsDraw() const {
      assert(is_terminal_);
      return is_draw_; }

    // Return the ID of the winning player. Valid only is IsTerminal ==
    // true and IsDraw == false.
    size_t WinnerId() const {
      assert(is_terminal_ && !is_draw_);
      return winner_id_; }

  private:
    friend class Board;
    bool is_terminal_;
    bool is_draw_;
    size_t winner_id_;
  };

public:
  // Create a board based on 'settings'.
  Board(const Settings& settings, const Stepper& stepper,
        const std::vector<Player*>& players);

  // Copy constructor.
  Board(const Board& other) = default;

  // Return the status of the board.
  const Board::Status& BoardStatus() const { return status_; }

  // Return the ID of the active player.
  size_t ActivePlayerId() const { return active_player_id_; }

  // Return the active player.
  Player* ActivePlayer() const { return players_[active_player_id_]; }

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

  // Return the ID of the player that owns the specified cell. Returns
  // the total number of players to indicate that the cell is not
  // owned by any player (i.e. empty).
  size_t PlayerIdAt(size_t row_idx, size_t col_idx) const;

  // Return the ID of the player that owns the specified cell at the
  // flattened index. Returns the total number of players to indicate
  // that the cell is not owned by any player (i.e. empty).
  size_t PlayerIdAtFlattened(size_t flat_idx) const;

  // Return the player that owns the specified cell, or nullptr if
  // cell is empty.
  Player* PlayerAt(size_t row_idx, size_t col_idx) const;

  // Set 'player' to be owner of the specified cell. 'player' may be
  // nullptr to set the cell to empty.
  void SetPlayerAt(Player* player, size_t row_idx, size_t col_idx);

  // Return the player that owns the specified cell at the flattened
  // index, or nullptr if cell is empty.
  Player* PlayerAtFlattened(size_t flat_idx) const;

  // Set 'player' to be owner of the specified cell at the flattend
  // index. 'player' may be nullptr to set the cell to empty.
  void SetPlayerAtFlattened(Player* player, size_t flat_idx);

  // See cifre::State::Status
  const cifre::State::Status& StateStatus() const override {
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

  // Convert player ID to/from the corresponding cell value. A player
  // ID of value players_.size() represents an empty cell (i.e. no
  // player).
  float PlayerIdToCellValue(size_t pid) const {
    if (pid == 0) { return CELL_PLAYER0; }
    if (pid == 1) { return CELL_PLAYER1; }
    assert(pid == players_.size());
    return CELL_EMPTY;
  }
  size_t CellValueToPlayerId(float v) const {
    if (v == CELL_PLAYER0) { return 0; }
    if (v == CELL_PLAYER1) { return 1; }
    assert(v == CELL_EMPTY);
    return players_.size();
  }

  // Convert player to/from the corresponding cell value. A nullptr
  // player represents an empty cell (i.e. no player).
  float PlayerToCellValue(Player* player) const {
    if (player == players_[0]) { return CELL_PLAYER0; }
    if (player == players_[1]) { return CELL_PLAYER1; }
    assert(player == nullptr);
    return CELL_EMPTY;
  }
  Player* CellValueToPlayer(float v) const {
    if (v == CELL_PLAYER0) { return players_[0]; }
    if (v == CELL_PLAYER1) { return players_[1]; }
    assert(v == CELL_EMPTY);
    return nullptr;
  }

  // Update the status of the board.
  void UpdateStatus();

  // Settings shared across all Board instances.
  const Settings& settings_;

  // Players shared across all Board instances.
  const Stepper& stepper_;

  // Players shared across all Board instances.
  const std::vector<cifre::celcath::Player*>& players_;

  // Current status of this state.
  Board::Status status_;

  // ID of the currently active player.
  size_t active_player_id_;

  // Active player gets some number of actions each turn.
  size_t complete_actions_this_turn_;

  // The set of action ids that are valid for the current board.
  std::unordered_set<uint64_t> valid_action_ids_;
};

}} // namespace cifre::celcath
