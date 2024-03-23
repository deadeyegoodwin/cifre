// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "cifre_action.h"
#include "cifre_board2d.h"
#include "common_logger.h"
#include "tictactoe_player.h"

namespace cifre { namespace tictactoe {

class Board;

//
// Board
//
// The tictactoe playing board and all associated state. A board cell
// has a value of CELL_ACTIVE_PLAYER if it is occupied by the active
// player, a value of CELL_OPPONENT_PLAYER if it is occupied by the
// other player, or CELL_EMPTY if it is empty.
//
class Board : public cifre::Board2D<float> {
public:
  // Create a board based on 'settings'.
  Board(const std::shared_ptr<common::Logger>& logger, const cifre::BoardSize2D& size,
        const std::vector<Player*>& players);

  // Copy constructor.
  Board(const Board& other) = default;

  // Return the active player.
  Player* ActivePlayer() const { return players_[active_player_id_]; }

  // Return the ID of the opponent player.
  size_t OpponentPlayerId() const { return (active_player_id_ == 0) ? 1 : 0; }

  // Return the opponent player.
  Player* OpponentPlayer() const { return players_[OpponentPlayerId()]; }

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

  // Set 'id' to be owner of the specified cell. 'id' may be the total
  // number of players to set the cell to empty.
  void SetPlayerIdAt(size_t id, size_t row_idx, size_t col_idx);

  // Return the ID of the player that owns the specified cell at the
  // flattened index. Returns the total number of players to indicate
  // that the cell is not owned by any player (i.e. empty).
  size_t PlayerIdAtFlattened(size_t flat_idx) const;

  // Set 'id' to be owner of the specified cell at the flattened
  // index. 'id' may be the total number of players to set the cell to
  // empty.
  void SetPlayerIdAtFlattened(size_t id, size_t flat_idx);

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

  // See cifre::State
  State* Clone() const override { return new Board(*this); }

  // See cifre::State
  size_t Hash() const override;

  // See cifre::State
  bool Equal(const State* other) const override;

  // See cifre::State
  size_t ActivePlayerId() const override { return active_player_id_; }

  // See cifre::State
  TerminalStatusKind TerminalStatus() const override { return terminal_status_; }

  // See cifre::State
  size_t TotalActionIdCount() const override { return size_.CellCount(); }

  // See cifre::State
  const std::unordered_set<uint64_t>& ValidActionIds() const override {
    return valid_action_ids_; }

  // See cifre::State
  void Update(uint64_t action_id, bool enable_logging = true) override;

  // See cifre::State
  std::string ToString(common::Logger::Level filter_level = common::Logger::Level::LOW) const override;

private:
  static constexpr float CELL_EMPTY = 0.0;
  static constexpr float CELL_ACTIVE_PLAYER = 1.0;
  static constexpr float CELL_OPPONENT_PLAYER = -1.0;

  // Return the ID of the other player ACTIVE<->OPPONENT. EMPTY does
  // not change.
  size_t OtherPlayerId(size_t pid) const {
    if (pid == players_.size()) { return pid; }
    return (pid == 0) ? 1 : 0;
  }

  // Convert player ID to/from the corresponding cell value. A player
  // ID of value players_.size() represents an empty cell (i.e. no
  // player).
  float PlayerIdToCellValue(size_t pid) const {
    if (pid == ActivePlayerId()) { return CELL_ACTIVE_PLAYER; }
    if (pid == OpponentPlayerId()) { return CELL_OPPONENT_PLAYER; }
    assert(pid == players_.size());
    return CELL_EMPTY;
  }
  size_t CellValueToPlayerId(float v) const {
    if (v == CELL_ACTIVE_PLAYER) { return ActivePlayerId(); }
    if (v == CELL_OPPONENT_PLAYER) { return OpponentPlayerId(); }
    assert(v == CELL_EMPTY);
    return players_.size();
  }

  // Convert player to/from the corresponding cell value. A nullptr
  // player represents an empty cell (i.e. no player).
  float PlayerToCellValue(Player* player) const {
    if (player == players_[ActivePlayerId()]) { return CELL_ACTIVE_PLAYER; }
    if (player == players_[OpponentPlayerId()]) { return CELL_OPPONENT_PLAYER; }
    assert(player == nullptr);
    return CELL_EMPTY;
  }
  Player* CellValueToPlayer(float v) const {
    if (v == CELL_ACTIVE_PLAYER) { return players_[ActivePlayerId()]; }
    if (v == CELL_OPPONENT_PLAYER) { return players_[OpponentPlayerId()]; }
    assert(v == CELL_EMPTY);
    return nullptr;
  }

  // Update the terminal status and valid actions for board.
  void UpdateTerminalStatus();
  void UpdateValidActions();

  // Update the board as necessary to make the opponent player the
  // active player.
  void SwitchActivePlayer();

  // Shared logger.
  std::shared_ptr<common::Logger> logger_;

  // Players shared across all Board instances.
  const std::vector<cifre::tictactoe::Player*>& players_;

  // ID of the currently active player.
  size_t active_player_id_;

  // The status of the board indicating winning condition.
  cifre::State::TerminalStatusKind terminal_status_;

  // The set of action ids that are valid for the current board.
  std::unordered_set<uint64_t> valid_action_ids_;

  // The board's hash and equality (cifre::State::Hash() and
  // cifre::State::Equal()) should include only those things that
  // uniquely distinquish it from another state in ways that are
  // important to actual gameplay (and thus important to the
  // alpha-zero algorithm, or any other algorithm that is being
  // applied to the board). Currently this includes the board itself
  // (from cifre::Board2d), and any members declared below. When new
  // members are added to this class they need to be evaluated to
  // determine if they belong with the below members and included in
  // the hash/equal or not.
};

}} // namespace cifre::tictactoe
