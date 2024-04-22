// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "cifre_action.h"
#include "cifre_board2d.h"
#include "celcath_player.h"
#include "celcath_stepper.h"

namespace cifre { namespace celcath {

//
// Board
//
// The celcath playing board and all associated state. Each board has
// an active player that it is respective to that player that the
// board state is maintained. Row 0 of the board is the home row of
// the active player.
//
class Board : public cifre::Board2D {
public:
  // Create board at the required starting state.
  Board(const cifre::BoardSize2D& size, const std::vector<Player*>& players,
        size_t starting_player_id, const std::shared_ptr<Stepper>& stepper);

  // Copy constructor.
  Board(const Board& other) :
    cifre::Board2D(other), step_action_id_(other.step_action_id_),
    stepper_(other.stepper_) { }

  // See cifre::State
  State* Clone() const override { return new Board(*this); }

  // One action for each board cell, plus the "step" action. See
  // cifre::State.
  size_t TotalActionIdCount() const override { return size_.CellCount() + 1; }

  // Return the player that owns the specified cell, or nullptr if no
  // owner.
  Player* PlayerAt(size_t row_idx, size_t col_idx) const;
  Player* PlayerAtFlattened(size_t flat_idx) const;

  // See cifre::State
  void Update(uint64_t action_id) override;

  // See cifre::State.
  std::string ActionToString(const Action& action) const override;

  // See cifre::State.
  Action StringToAction(const std::string& str) const override;

  // See cifre::State.
  std::string ToString() const override;

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

private:
  // Logic assumes 1.0 step between EMPTY and LG.
  static constexpr float CELL_EMPTY = 0.0;
  static constexpr float CELL_SM = 1.0;
  static constexpr float CELL_MD = 2.0;
  static constexpr float CELL_LG = 3.0;

  void UpdateTerminalStatus();
  void UpdateValidActions();
  void SwitchActivePlayer();
  float SwitchCellValue(const float v) const;

  const size_t step_action_id_;
  std::shared_ptr<Stepper> stepper_;
};

}} // namespace cifre::celcath
