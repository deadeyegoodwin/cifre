// Copyright 2025, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "cifre_action.h"
#include "cifre_board2d_hex.h"
#include "gettysburg_board_cell_factory.h"
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

  // Destructor
  virtual ~Board();

  // See cifre::State.
  // Hash/compare all members that uniquely define this board.
  virtual size_t Hash() const override;
  virtual bool Equal(const State* other) const override;

  // See cifre::State
  Board* Copy() const override;
  void Update(uint64_t action_id) override;
  Board* CopyAndUpdate(uint64_t action_id) const override;

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
  // Return the value of the plane cell at the given row, column, and
  // plane. Row and column index are relative to the plane.
  float PlaneCellValueAt(size_t row_idx, size_t col_idx, size_t plane_idx) const {
    assert((row_idx < plane_row_cnt_) &&
           (col_idx < plane_column_cnt_) &&
           (plane_idx < BoardCellFactory::PLANE_COUNT));
    return cells_[(row_idx * plane_column_cnt_ * BoardCellFactory::PLANE_COUNT) +
                  (col_idx * BoardCellFactory::PLANE_COUNT) + plane_idx]; }

  // Set the value of the plane cell at the given row, column, and
  // plane. Row and column index are relative to the plane.
  void SetPlaneCellValueAt(float value, size_t row_idx, size_t col_idx, size_t plane_idx) {
    assert((row_idx < plane_row_cnt_) &&
           (col_idx < plane_column_cnt_) &&
           (plane_idx < BoardCellFactory::PLANE_COUNT));
    cells_[(row_idx * plane_column_cnt_ * BoardCellFactory::PLANE_COUNT) +
           (col_idx * BoardCellFactory::PLANE_COUNT) + plane_idx] = value; }

  // Return the value of the board cell at the given row, column, and
  // plane. Row and column index are relative to the visible board.
  float BoardCellValueAt(size_t row_idx, size_t col_idx, size_t plane_idx) const {
    assert((row_idx < size_.RowCount()) &&
           (col_idx < size_.ColumnCount()) &&
           (plane_idx < BoardCellFactory::PLANE_COUNT));
    return cells_[((row_idx + active_first_row_) * plane_column_cnt_ * BoardCellFactory::PLANE_COUNT) +
                  ((col_idx + active_first_column_) * BoardCellFactory::PLANE_COUNT) + plane_idx]; }

  // Set the value of the board cell at the given row, column, and
  // plane. Row and column index are relative to the visible board.
  void SetBoardCellValueAt(float value, size_t row_idx, size_t col_idx, size_t plane_idx) {
    assert((row_idx < size_.RowCount()) &&
           (col_idx < size_.ColumnCount()) &&
           (plane_idx < BoardCellFactory::PLANE_COUNT));
    cells_[((row_idx + active_first_row_) * plane_column_cnt_ * BoardCellFactory::PLANE_COUNT) +
           ((col_idx + active_first_column_) * BoardCellFactory::PLANE_COUNT) + plane_idx] = value; }

  void SetUnionAt(float value, size_t row_idx, size_t col_idx) {
    SetBoardCellValueAt(value, row_idx, col_idx, BoardCellFactory::PLANE_IDX_UNION); }
  float UnionAt(size_t row_idx, size_t col_idx) const {
    return BoardCellValueAt(row_idx, col_idx, BoardCellFactory::PLANE_IDX_UNION); }

  void SetConfederateAt(float value, size_t row_idx, size_t col_idx) {
    SetBoardCellValueAt(value, row_idx, col_idx, BoardCellFactory::PLANE_IDX_CONFEDERATE); }
  float ConfederateAt(size_t row_idx, size_t col_idx) const {
    return BoardCellValueAt(row_idx, col_idx, BoardCellFactory::PLANE_IDX_CONFEDERATE); }

  void SetGoalAt(float value, size_t row_idx, size_t col_idx) {
    SetBoardCellValueAt(value, row_idx, col_idx, BoardCellFactory::PLANE_IDX_GOAL); }
  float GoalAt(size_t row_idx, size_t col_idx) const {
    return BoardCellValueAt(row_idx, col_idx, BoardCellFactory::PLANE_IDX_GOAL); }

  void SetMoveAt(float value, size_t row_idx, size_t col_idx) {
    SetBoardCellValueAt(value, row_idx, col_idx, BoardCellFactory::PLANE_IDX_MOVE); }
  float MoveAt(size_t row_idx, size_t col_idx) const {
    return BoardCellValueAt(row_idx, col_idx, BoardCellFactory::PLANE_IDX_MOVE); }

  void SetForestAt(float value, size_t row_idx, size_t col_idx) {
    SetBoardCellValueAt(value, row_idx, col_idx, BoardCellFactory::PLANE_IDX_FOREST); }
  float ForestAt(size_t row_idx, size_t col_idx) const {
    return BoardCellValueAt(row_idx, col_idx, BoardCellFactory::PLANE_IDX_FOREST); }

  void SetHillAt(float value, size_t row_idx, size_t col_idx) {
    SetBoardCellValueAt(value, row_idx, col_idx, BoardCellFactory::PLANE_IDX_HILL); }
  float HillAt(size_t row_idx, size_t col_idx) const {
    return BoardCellValueAt(row_idx, col_idx, BoardCellFactory::PLANE_IDX_HILL); }

private:
  Board& operator=(const Board&) = delete;  // assignment not allowed

  // Copy constructor. WARNING: this does not copy 'cells_flattened_'
  // or 'cells_'. To perform a complete copy, use Copy().
  Board(const Board& other);

  static constexpr char UNION_CHAR = 'u';
  static constexpr char CONFEDERATE_CHAR = 'c';

  Unit* ActiveUnit() const;

  void ReleaseInputPlanes();
  void UpdateInputPlanes();
  void UpdateTerminalStatus();
  void UpdateValidActions();
  void NextActiveUnit();

  std::string DebugCellUpperString(size_t row_idx, size_t col_idx) const;
  std::string DebugCellLowerString(size_t row_idx, size_t col_idx) const;

  BoardCellFactory* cell_factory_;
  BoardCellFactory::CellsKey cells_flattened_factory_key_;

  size_t plane_row_cnt_;
  size_t plane_column_cnt_;

  Unit::ArmyKind active_army_;
  Unit* active_unit_;
  size_t active_unit_idx_;
  size_t active_first_row_;
  size_t active_first_column_;
  float active_move_factor_expended_;

  std::vector<std::unique_ptr<Unit>> union_units_;
  std::vector<std::unique_ptr<Unit>> confederate_units_;
};

}} // namespace cifre::gettysburg
