// Copyright 2025, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "cifre_action.h"
#include "cifre_board2d_hex.h"
#include "gettysburg_player.h"
#include "gettysburg_unit.h"

namespace cifre { namespace gettysburg {

//
// Board
//
// The gettysburg playing board.
//
class Board : public cifre::Board2DHex {
public:
  // Create board...
  Board(const cifre::BoardSize2D& size, const std::vector<Player*>& players,
        size_t starting_player_id);

  // Copy constructor.
  Board(const Board& other);

  // See cifre::State
  State* Clone() const override { return new Board(*this); }

  // See cifre::State
  void Update(uint64_t action_id) override;

  // See cifre::State
  size_t TotalActionIdCount() const override;

  // See cifre::State
  std::string ActionToString(const Action& action) const override;

  // See cifre::State
  Action StringToAction(const std::string& str) const override;

  // See cifre::Board2DHex
  std::string BoardCellUpperString(size_t row_idx, size_t col_idx) const;
  std::string BoardCellLowerString(size_t row_idx, size_t col_idx) const;

  // Return a debug representation of the board.
  std::string ToDebugString() const;

public:
  static constexpr size_t PLANE_COUNT = 4;
  static constexpr size_t PLANE_IDX_UNION = 0;
  static constexpr size_t PLANE_IDX_CONFEDERATE = 1;
  static constexpr size_t PLANE_IDX_GOAL = 2;
  static constexpr size_t PLANE_IDX_MOVE = 3;

  static constexpr float CELL_EMPTY = 0.0;
  static constexpr float CELL_ACTIVE = 1.0;

  // Return the value of the plane cell at the given row, column, and
  // plane. Row and column index are relative to the plane.
  float PlaneCellValueAt(size_t row_idx, size_t col_idx, size_t plane_idx) const {
    assert((row_idx < plane_row_cnt_) &&
           (col_idx < plane_column_cnt_) &&
           (plane_idx < PLANE_COUNT));
    return cells_[(row_idx * plane_column_cnt_ * PLANE_COUNT) +
                  (col_idx * PLANE_COUNT) + plane_idx]; }

  // Set the value of the plane cell at the given row, column, and
  // plane. Row and column index are relative to the plane.
  void SetPlaneCellValueAt(float value, size_t row_idx, size_t col_idx, size_t plane_idx) {
    assert((row_idx < plane_row_cnt_) &&
           (col_idx < plane_column_cnt_) &&
           (plane_idx < PLANE_COUNT));
    cells_[(row_idx * plane_column_cnt_ * PLANE_COUNT) +
           (col_idx * PLANE_COUNT) + plane_idx] = value; }

  // Return the value of the board cell at the given row, column, and
  // plane. Row and column index are relative to the visible board.
  float BoardCellValueAt(size_t row_idx, size_t col_idx, size_t plane_idx) const {
    assert((row_idx < size_.RowCount()) &&
           (col_idx < size_.ColumnCount()) &&
           (plane_idx < PLANE_COUNT));
    return cells_[((row_idx + board_first_row_) * plane_column_cnt_ * PLANE_COUNT) +
                  ((col_idx + board_first_column_) * PLANE_COUNT) + plane_idx]; }

  // Set the value of the board cell at the given row, column, and
  // plane. Row and column index are relative to the visible board.
  void SetBoardCellValueAt(float value, size_t row_idx, size_t col_idx, size_t plane_idx) {
    assert((row_idx < size_.RowCount()) &&
           (col_idx < size_.ColumnCount()) &&
           (plane_idx < PLANE_COUNT));
    cells_[((row_idx + board_first_row_) * plane_column_cnt_ * PLANE_COUNT) +
           ((col_idx + board_first_column_) * PLANE_COUNT) + plane_idx] = value; }

  void SetUnionAt(float value, size_t row_idx, size_t col_idx) {
    SetBoardCellValueAt(value, row_idx, col_idx, PLANE_IDX_UNION); }
  float UnionAt(size_t row_idx, size_t col_idx) const {
    return BoardCellValueAt(row_idx, col_idx, PLANE_IDX_UNION); }

  void SetConfederateAt(float value, size_t row_idx, size_t col_idx) {
    SetBoardCellValueAt(value, row_idx, col_idx, PLANE_IDX_CONFEDERATE); }
  float ConfederateAt(size_t row_idx, size_t col_idx) const {
    return BoardCellValueAt(row_idx, col_idx, PLANE_IDX_CONFEDERATE); }

  void SetGoalAt(float value, size_t row_idx, size_t col_idx) {
    SetBoardCellValueAt(value, row_idx, col_idx, PLANE_IDX_GOAL); }
  float GoalAt(size_t row_idx, size_t col_idx) const {
    return BoardCellValueAt(row_idx, col_idx, PLANE_IDX_GOAL); }

  void SetMoveAt(float value, size_t row_idx, size_t col_idx) {
    SetBoardCellValueAt(value, row_idx, col_idx, PLANE_IDX_MOVE); }
  float MoveAt(size_t row_idx, size_t col_idx) const {
    return BoardCellValueAt(row_idx, col_idx, PLANE_IDX_MOVE); }

private:
  static constexpr char UNION_CHAR = 'u';
  static constexpr char CONFEDERATE_CHAR = 'c';

  void UpdateTerminalStatus();
  void UpdateValidActions();
  std::string DebugCellUpperString(size_t row_idx, size_t col_idx) const;
  std::string DebugCellLowerString(size_t row_idx, size_t col_idx) const;

  size_t plane_row_cnt_;
  size_t plane_column_cnt_;
  size_t board_first_row_;
  size_t board_first_column_;

  std::vector<std::unique_ptr<Unit>> union_units_;
  std::vector<std::unique_ptr<Unit>> confederate_units_;
};

}} // namespace cifre::gettysburg
