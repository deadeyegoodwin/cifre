// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "cifre_action.h"
#include "cifre_player.h"
#include "cifre_state.h"
#include "common_util.h"

namespace cifre {

//
// BoardSize2D
//
// Represents the size of a 2D board, in rows and columns.
//
class BoardSize2D {
public:
  // A square board size with 'cnt' rows and columns.
  BoardSize2D(size_t cnt) : BoardSize2D(cnt, cnt) {}

  // A rectangular board size with 'rcnt' rows and 'ccnt' columns.
  BoardSize2D(size_t rcnt, size_t ccnt) : row_cnt_(rcnt), column_cnt_(ccnt) {}

  // Create a BoardSize2D by parsing a string with the format
  // "<rows>x<columns>", for example "4x3"; or format
  // "<rows>,<columns>", for example "4,3".
  BoardSize2D(const std::string& str);

  // Return the number of rows in the board size.
  size_t RowCount() const { return row_cnt_; }

  // Return the number of columns in the board size.
  size_t ColumnCount() const { return column_cnt_; }

  // Return the number of cells (rows * cols) in the board size.
  size_t CellCount() const { return row_cnt_ * column_cnt_; }

  // Return the board size as a std::pair { row-count, column-count }.
  std::pair<size_t, size_t> Size() const { return { row_cnt_, column_cnt_ }; }

  // Return string representation of the board size.
  std::string ToString() const;

private:
  size_t row_cnt_;
  size_t column_cnt_;
};

//
// Board2D
//
// A two player, 2D game board where each cell can be empty or "owned"
// by one of the players. By default the board has one action for each
// board cell (i.e. "occupy the cell"), but derived classes can
// override TotalActionIdCount() and ActionToString() to change.
//
// Provides a console representation of the board and methods to
// convert text commands to/from actions.
//
// A board cell has a value of CELL_ACTIVE_PLAYER if it is occupied by
// the active player, a value of CELL_OPPONENT_PLAYER if it is
// occupied by the other player, or CELL_EMPTY if it is empty. Acts as
// a cifre::State with the triton representation being a flattened
// row-major ordering of the board cell values. Derived classes must
// implement appropriate action and board update logic.
//
class Board2D : public State {
public:
  // Create a board based of a given 'size' and 'players', where
  // player ID is the index of a given player in
  // 'players'. 'starting_player_id' indicates the player that will go
  // first.
  Board2D(const cifre::BoardSize2D& size, const std::vector<Player*>& players,
          size_t starting_player_id);

  // Copy constructor.
  Board2D(const Board2D& other);

  // Destructor
  virtual ~Board2D() = default;

  // Return the board size.
  const BoardSize2D& Size() const { return size_; }

  // Return the Action associated with a string.
  Action StringToAction(const std::string& str) const;

  // Return the string representation of an 'action'. See
  // cifre::State.
  virtual std::string ActionToString(const Action& action) const override;

  // See cifre::State.
  bool IsValidAction(const Action& action) const override;

  // Return the ID of the active player. See cifre::State.
  size_t ActivePlayerId() const override { return active_player_id_; }

  // Return the active player.
  Player* ActivePlayer() const { return players_[active_player_id_]; }

  // Return the ID of the opponent player. There must be exactly two
  // players, so the opponent ID is the opposite of active player ID.
  size_t OpponentPlayerId() const { return (active_player_id_ == 0) ? 1 : 0; }

  // Return the opponent player.
  Player* OpponentPlayer() const { return players_[OpponentPlayerId()]; }

  // Return the ID of the player that owns the specified cell. Returns
  // the total number of players (PLAYER_COUNT) to indicate that the
  // cell is not owned by any player (i.e. empty).
  size_t PlayerIdAt(size_t row_idx, size_t col_idx) const {
    return CellValueToPlayerId(CellValueAt(row_idx, col_idx)); }

  // Set 'id' to be owner of the specified cell. 'id' may be the total
  // number of players (PLAYER_COUNT) to set the cell to empty.
  void SetPlayerIdAt(size_t id, size_t row_idx, size_t col_idx) {
    SetCellValueAt(PlayerIdToCellValue(id), row_idx, col_idx); }

  // Return the ID of the player that owns the specified cell at the
  // flattened index. Returns the total number of players
  // (PLAYER_COUNT) to indicate that the cell is not owned by any
  // player (i.e. empty).
  size_t PlayerIdAtFlattened(size_t flat_idx) const {
    return CellValueToPlayerId(CellValueAtFlattened(flat_idx));
  }

  // Set 'id' to be owner of the specified cell at the flattened
  // index. 'id' may be the total number of players (PLAYER_COUNT) to
  // set the cell to empty.
  void SetPlayerIdAtFlattened(size_t id, size_t flat_idx) {
    SetCellValueAtFlattened(PlayerIdToCellValue(id), flat_idx);
  }

  // Return the player that owns the specified cell, or nullptr if
  // cell is empty.
  Player* PlayerAt(size_t row_idx, size_t col_idx) const;

  // Set 'player' to be owner of the specified cell. 'player' may be
  // nullptr to set the cell to empty.
  void SetPlayerAt(Player* player, size_t row_idx, size_t col_idx) {
    SetCellValueAt(PlayerToCellValue(player), row_idx, col_idx); }

  // Return the player that owns the specified cell at the flattened
  // index, or nullptr if cell is empty.
  Player* PlayerAtFlattened(size_t flat_idx) const;

  // Set 'player' to be owner of the specified cell at the flattend
  // index. 'player' may be nullptr to set the cell to empty.
  void SetPlayerAtFlattened(Player* player, size_t flat_idx) {
    SetCellValueAtFlattened(PlayerToCellValue(player), flat_idx); }

  // Update the board as necessary to make the opponent player the
  // active player.
  void SwitchActivePlayer();

  // See cifre::State
  size_t Hash() const override;

  // See cifre::State
  bool Equal(const State* other) const override;

  // See cifre::State
  StateTerminalStatus TerminalStatus() const override { return terminal_status_; }

  // See cifre::State
  virtual size_t TotalActionIdCount() const override { return size_.CellCount(); }

  // See cifre::State
  const std::unordered_set<uint64_t>& ValidActionIds() const override {
    return valid_action_ids_; }

  // Get the representation of the state suitable for feeding to the
  // model via triton. Returns flattened row-major ordering of the
  // cell values. See cifre::State.
  const std::vector<uint8_t>& TritonRepresentation() const override {
    return cells_flattened_;
  }

  // See cifre::State
  std::string ToString() const override;

protected:
  static constexpr size_t PLAYER_COUNT = 2;
  static constexpr float CELL_EMPTY = 0.0;
  static constexpr float CELL_ACTIVE_PLAYER = 1.0;
  static constexpr float CELL_OPPONENT_PLAYER = -1.0;

  // Update the terminal status for the board by setting
  // 'terminal_status_'.
  virtual void UpdateTerminalStatus() = 0;

  // Update the valid actions for the board by setting
  // 'valid_action_ids_'.
  virtual void UpdateValidActions() = 0;

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
    assert(flat_idx < size_.CellCount()); return cells_[flat_idx]; }

  // Set the value of the cell at the flattened index 'flat_idx'.
  void SetCellValueAtFlattened(const float value, size_t flat_idx) {
    assert(flat_idx < size_.CellCount()); cells_[flat_idx] = value; }

  // Convert player ID to/from the corresponding cell value. A player
  // ID of value PLAYER_COUNT represents an empty cell (i.e. no
  // player).
  float PlayerIdToCellValue(size_t pid) const {
    if (pid == ActivePlayerId()) { return CELL_ACTIVE_PLAYER; }
    if (pid == OpponentPlayerId()) { return CELL_OPPONENT_PLAYER; }
    assert(pid == PLAYER_COUNT);
    return CELL_EMPTY;
  }
  size_t CellValueToPlayerId(float v) const {
    if (v == CELL_ACTIVE_PLAYER) { return ActivePlayerId(); }
    if (v == CELL_OPPONENT_PLAYER) { return OpponentPlayerId(); }
    assert(v == CELL_EMPTY);
    return PLAYER_COUNT;
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

  // Size of the board.
  const BoardSize2D size_;

  // The players. The player's index in this vector is the player ID.
  const std::vector<cifre::Player*> players_;

  // ID of the currently active player.
  size_t active_player_id_;

  // The status of the board indicating winning condition.
  cifre::StateTerminalStatus terminal_status_;

  // The set of action ids that are valid for the current board.
  std::unordered_set<uint64_t> valid_action_ids_;

  // Triton-friendly cell contents. 'cells_' is simply a type-casted
  // pointer into 'cells_flattened_'. A board cell has a value of
  // CELL_ACTIVE_PLAYER if it is occupied by the active player, a
  // value of CELL_OPPONENT_PLAYER if it is occupied by the other
  // player, or CELL_EMPTY if it is empty. Acts as
  std::vector<uint8_t> cells_flattened_;
  float* cells_;
};

} // namespace cifre
