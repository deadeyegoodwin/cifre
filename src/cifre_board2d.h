// Copyright 2025, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "cifre_action.h"
#include "cifre_player.h"
#include "cifre_state.h"
#include "cifre_vec.h"
#include "common_util.h"

namespace cifre {

//
// Board2D
//
// A specialization of State for two player games played on a 2D
// surface of "cells" where each cell can have only one of three
// states: empty, "owned" by active player, or "owned" by opponent
// player. By default the board has one action for each board cell
// (i.e. "occupy the cell"), but derived classes can override
// TotalActionIdCount(), StringToAction(), ActionToString() to
// add/remove actions.
//
// Provides a console representation of the board and methods to
// convert text commands to/from actions.
//
// A board cell has a value of CELL_ACTIVE_PLAYER if it is occupied by
// the active player, a value of CELL_OPPONENT_PLAYER if it is
// occupied by the other player, or CELL_EMPTY if it is empty. Acts as
// a cifre::State with the triton representation being a flattened
// row-major ordering of the board cell values. Derived classes must
// implement appropriate action and board update logic via
// State::Update().
//
// See cifre::State for information about memory management requires
// classes derived from State.
//
class Board2D : public State {
public:
  static constexpr size_t PLAYER_COUNT = 2;

  // Create a board based on a given 'size' and 'players', where
  // player ID is the index of a given player in 'players'.
  // 'starting_player_id' indicates the player that will go first.
  Board2D(
    const cifre::MatrixSize& size, const std::vector<Player*>& players,
    size_t starting_player_id, std::pmr::memory_resource* arena);

  // Construct board as a copy of 'other' using 'arena' for memory
  // required by constructed board.
  Board2D(const Board2D& other, std::pmr::memory_resource* arena);

  // Destructor
  virtual ~Board2D() = default;

  // See cifre::State. Hash based on 'active_player_id_' and 'cells_'.
  virtual size_t Hash() const override;

  // See cifre::State. Equally is determined by 'active_player_id_'
  // and 'cells_'.
  virtual bool Equal(const State* other) const override;

  // One action for each board cell. See cifre::State.
  virtual size_t TotalActionIdCount() const override {
    return size_.ElementCount(); }

  // See cifre::State.
  virtual Action StringToAction(const std::string& str) const override;

  // See cifre::State.
  virtual std::string ActionToString(const Action& action) const override;

  // See cifre::State
  StateTerminalStatus TerminalStatus() const override { return terminal_status_; }

  // See cifre::State
  const std::pmr::vector<uint64_t>& ValidActionIds() const override {
    return valid_action_ids_; }

  // See cifre::State.
  virtual bool IsValidAction(const Action& action) const override;

  // Get the representation of the state suitable for feeding to the
  // model via triton. Returns flattened row-major ordering of the
  // cell values. See cifre::State.
  const FlattenedCells& TritonRepresentation() const override {
    return cells_flattened_;
  }

  // Return the board size.
  const MatrixSize& Size() const { return size_; }

  // Return the player ID that represents "no player". By convention
  // this ID is equal to the number of players in the game.
  size_t NoPlayerId() const { return players_.size(); }

  // Return the ID of the active player. See cifre::State.
  size_t ActivePlayerId() const override { return active_player_id_; }

  // Return the active player.
  Player* ActivePlayer() const { return players_[active_player_id_]; }

  // Return the ID of the opponent player. There must be exactly two
  // players, so the opponent ID is the opposite of active player ID.
  size_t OpponentPlayerId() const { return (active_player_id_ == 0) ? 1 : 0; }

  // Return the opponent player.
  Player* OpponentPlayer() const { return players_[OpponentPlayerId()]; }

  // Return the player that owns the specified cell, or nullptr if
  // cell is empty.
  virtual Player* PlayerAt(size_t row_idx, size_t col_idx) const {
    return CellValueToPlayer(CellValueAt(row_idx, col_idx)); }

  // Set 'player' to be owner of the specified cell. 'player' may be
  // nullptr to set the cell to no-owner / empty.
  virtual void SetPlayerAt(Player* player, size_t row_idx, size_t col_idx) {
    SetCellValueAt(PlayerToCellValue(player), row_idx, col_idx); }

  // Return the player that owns the specified cell at the flattened
  // index, or nullptr if cell is empty.
  virtual Player* PlayerAtFlattened(size_t flat_idx) const {
    return CellValueToPlayer(CellValueAtFlattened(flat_idx)); }

  // Set 'player' to be owner of the specified cell at the flattend
  // index. 'player' may be nullptr to set the cell to no-owner /
  // empty.
  virtual void SetPlayerAtFlattened(Player* player, size_t flat_idx) {
    SetCellValueAtFlattened(PlayerToCellValue(player), flat_idx); }

  // See cifre::State.
  virtual std::string ToString() const override;

protected:
  static constexpr float CELL_EMPTY = 0.0;
  static constexpr float CELL_ACTIVE_PLAYER = 1.0;
  static constexpr float CELL_OPPONENT_PLAYER = -1.0;

  // Don't allow default copy constructor or assignment to force the
  // version above that requires an arena. Don't allow move
  // assignment as it does not move the allocator for any pmr
  // containers (move constructor is ok).
  Board2D(Board2D&) = delete;
  Board2D& operator=(Board2D&) = delete;
  Board2D(const Board2D&) = delete;
  Board2D& operator=(const Board2D&) = delete;
  Board2D& operator=(Board2D&&) = delete;

  // Return the value of the cell at the given row and column index.
  float CellValueAt(size_t row_idx, size_t col_idx) const {
    assert((row_idx < size_.RowCount()) && (col_idx < size_.ColumnCount()));
    return cells_[(row_idx * size_.ColumnCount()) + col_idx]; }

  // Set the value of the cell at the given row and column index.
  void SetCellValueAt(const float value, size_t row_idx, size_t col_idx) {
    assert((row_idx < size_.RowCount()) && (col_idx < size_.ColumnCount()));
    cells_[(row_idx * size_.ColumnCount()) + col_idx] = value; }

  // Return the value of the cell at the flattened index 'flat_idx'.
  float CellValueAtFlattened(size_t flat_idx) const {
    assert(flat_idx < size_.ElementCount()); return cells_[flat_idx]; }

  // Set the value of the cell at the flattened index 'flat_idx'.
  void SetCellValueAtFlattened(const float value, size_t flat_idx) {
    assert(flat_idx < size_.ElementCount()); cells_[flat_idx] = value; }

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

  // Size of the board.
  const MatrixSize size_;

  // The players. The player's index in this vector is the player ID.
  std::array<cifre::Player*, PLAYER_COUNT> players_;

  // ID of the currently active player.
  size_t active_player_id_;

  // The status of the board indicating winning condition.
  cifre::StateTerminalStatus terminal_status_;

  // The set of action ids that are valid for the current board.
  std::pmr::vector<uint64_t> valid_action_ids_;

  // 'cells_' is simply a type-casted pointer into 'cells_flattened_'.
  FlattenedCells cells_flattened_;
  float* cells_;
};

} // namespace cifre
