// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "cifre_board2d.h"

namespace cifre {

//
// Board2DBasic
//
// A specialization of Board2D for two player games where each cell
// can have only one of three states: empty, "owned" by active player,
// owned by opponent player. By default the board has one action for
// each board cell (i.e. "occupy the cell"), but derived classes can
// override TotalActionIdCount(), StringToAction(), ActionToString()
// to add/remove actions.
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
class Board2DBasic : public Board2D {
public:
  static constexpr size_t PLAYER_COUNT = 2;

  // Create a board based of a given 'size' and 'players', where
  // player ID is the index of a given player in
  // 'players'. 'starting_player_id' indicates the player that will go
  // first.
  Board2DBasic(
    const cifre::BoardSize2D& size, const std::vector<Player*>& players,
    size_t starting_player_id);

  // Copy constructor.
  Board2DBasic(const Board2DBasic& other) : Board2D(other) { }

  // Destructor
  virtual ~Board2DBasic() = default;

  // One action for each board cell. See cifre::State.
  virtual size_t TotalActionIdCount() const override { return size_.CellCount(); }

  // See cifre::State.
  virtual Action StringToAction(const std::string& str) const override;

  // See cifre::State.
  virtual std::string ActionToString(const Action& action) const override;

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
};

} // namespace cifre
