// Copyright 2025, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <map>

#include "cifre_action.h"
#include "cifre_board2d_hex.h"
#include "gettysburg_board_cell_factory.h"
#include "gettysburg_board_move_factory.h"
#include "gettysburg_oob.h"
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
  static constexpr char UNION_CHAR = 'u';
  static constexpr char CONFEDERATE_CHAR = 'c';

public:
  // Create board...
  Board(
    const std::shared_ptr<BoardCellFactory>& cell_factory,
    const std::shared_ptr<BoardMoveFactory>& move_factory,
    const OOB::Scenario scenario,
    const cifre::BoardSize2D& board_shape, const cifre::BoardSize2D& plane_shape,
    const std::vector<Player*>& players);

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
  std::string BoardHeader() const override;
  std::string BoardTrailer() const override;
  std::vector<std::string> BoardCell(
    DisplayKind kind, size_t row_idx, size_t col_idx) const override;

public:
  // Return the value of the plane cell at the given row, column, and
  // plane. Row and column index are relative to the plane.
  float PlaneCellValueAt(size_t row_idx, size_t col_idx, size_t plane_idx) const {
    assert((row_idx < plane_shape_.RowCount()) &&
           (col_idx < plane_shape_.ColumnCount()) &&
           (plane_idx < BoardCellFactory::PLANE_COUNT));
    return cells_[(row_idx * plane_shape_.ColumnCount() * BoardCellFactory::PLANE_COUNT) +
                  (col_idx * BoardCellFactory::PLANE_COUNT) + plane_idx]; }

  // Set the value of the plane cell at the given row, column, and
  // plane. Row and column index are relative to the plane.
  void SetPlaneCellValueAt(float value, size_t row_idx, size_t col_idx, size_t plane_idx) {
    assert((row_idx < plane_shape_.RowCount()) &&
           (col_idx < plane_shape_.ColumnCount()) &&
           (plane_idx < BoardCellFactory::PLANE_COUNT));
    cells_[(row_idx * plane_shape_.ColumnCount() * BoardCellFactory::PLANE_COUNT) +
           (col_idx * BoardCellFactory::PLANE_COUNT) + plane_idx] = value; }

  // Return the value of the board cell at the given row, column, and
  // plane. Row and column index are relative to the visible board.
  float BoardCellValueAt(size_t row_idx, size_t col_idx, size_t plane_idx) const {
    assert((row_idx < size_.RowCount()) &&
           (col_idx < size_.ColumnCount()) &&
           (plane_idx < BoardCellFactory::PLANE_COUNT));
    return cells_[((row_idx + active_first_row_) * plane_shape_.ColumnCount() * BoardCellFactory::PLANE_COUNT) +
                  ((col_idx + active_first_column_) * BoardCellFactory::PLANE_COUNT) + plane_idx]; }

  // Set the value of the board cell at the given row, column, and
  // plane. Row and column index are relative to the visible board.
  void SetBoardCellValueAt(float value, size_t row_idx, size_t col_idx, size_t plane_idx) {
    assert((row_idx < size_.RowCount()) &&
           (col_idx < size_.ColumnCount()) &&
           (plane_idx < BoardCellFactory::PLANE_COUNT));
    cells_[((row_idx + active_first_row_) * plane_shape_.ColumnCount() * BoardCellFactory::PLANE_COUNT) +
           ((col_idx + active_first_column_) * BoardCellFactory::PLANE_COUNT) + plane_idx] = value; }

  void SetUnionVictoryPointAt(float value, size_t row_idx, size_t col_idx) {
    SetBoardCellValueAt(value, row_idx, col_idx, BoardCellFactory::PLANE_IDX_UNION_VP); }
  float UnionVictoryPointAt(size_t row_idx, size_t col_idx) const {
    return BoardCellValueAt(row_idx, col_idx, BoardCellFactory::PLANE_IDX_UNION_VP); }

  void SetConfederateVictoryPointAt(float value, size_t row_idx, size_t col_idx) {
    SetBoardCellValueAt(value, row_idx, col_idx, BoardCellFactory::PLANE_IDX_CONFEDERATE_VP); }
  float ConfederateVictoryPointAt(size_t row_idx, size_t col_idx) const {
    return BoardCellValueAt(row_idx, col_idx, BoardCellFactory::PLANE_IDX_CONFEDERATE_VP); }

  void SetVictoryPointAt(float value, size_t row_idx, size_t col_idx) {
    SetBoardCellValueAt(value, row_idx, col_idx, BoardCellFactory::PLANE_IDX_VICTORY_POINT); }
  float VictoryPointAt(size_t row_idx, size_t col_idx) const {
    return BoardCellValueAt(row_idx, col_idx, BoardCellFactory::PLANE_IDX_VICTORY_POINT); }

  float ForestAt(size_t row_idx, size_t col_idx) const {
    return BoardCellValueAt(row_idx, col_idx, BoardCellFactory::PLANE_IDX_FOREST); }
  float HillAt(size_t row_idx, size_t col_idx) const {
    return BoardCellValueAt(row_idx, col_idx, BoardCellFactory::PLANE_IDX_HILL); }

private:
  Board& operator=(const Board&) = delete;  // assignment not allowed

  // Copy constructor. WARNING: this does not copy 'cells_flattened_'
  // , 'cells_' and some other members. To perform a complete copy,
  // use Copy().
  Board(const Board& other);

  // Utility method to get the active unit from active_army_ and
  // active_unit_idx_. Return nullptr if active army does not have any
  // units.
  Unit* ActiveUnit() const;

  // Return union or confederate UNITs at a given board position.
  std::vector<Unit*> UnitsAt(Unit::ArmyKind kind, size_t row_idx, size_t col_idx) const;
  std::vector<Unit*> UnionAt(size_t row_idx, size_t col_idx) const {
    return UnitsAt(Unit::ArmyKind::UNION, row_idx, col_idx); }
  std::vector<Unit*> ConfederateAt(size_t row_idx, size_t col_idx) const {
    return UnitsAt(Unit::ArmyKind::CONFEDERATE, row_idx, col_idx); }

  void ReleaseInputPlanes();
  void UpdateInputPlanes();
  void UpdateMoveCosts();
  void UpdateTerminalStatus();
  void UpdateValidActions();
  void ReinforceUnits();
  bool NextArmyUnit(const std::vector<std::unique_ptr<Unit>>& units);

  std::vector<std::string> SmallBoardCell(size_t row_idx, size_t col_idx) const;
  std::vector<std::string> LargeBoardCell(
    bool verbose, size_t row_idx, size_t col_idx) const;
  std::string BoardUnitString(size_t row_idx, size_t col_idx) const;
  std::string SmallBoardTerrainString(size_t row_idx, size_t col_idx) const;
  std::string LargeBoardTerrainString(size_t row_idx, size_t col_idx) const;
  std::string LargeBoardVictoryPointString(size_t row_idx, size_t col_idx) const;

  std::shared_ptr<BoardCellFactory> cell_factory_;
  BoardCellFactory::CellsKey cells_flattened_factory_key_;

  std::shared_ptr<BoardMoveFactory> move_factory_;

  const OOB::Scenario scenario_;
  const cifre::BoardSize2D plane_shape_;

  Unit::ArmyKind active_army_;
  Unit* active_unit_;
  size_t active_unit_idx_;
  size_t active_first_row_;
  size_t active_first_column_;

  // Cost to move in each direction for the active unit. A negative
  // cost indicates that movement in that direction is not possible.
  std::array<float, Board2DHex::DIRECTION_COUNT> active_move_costs_;

  // Integer representing the game turn.
  size_t turn_;

  // Map of board positions that contain a captured victory points. A
  // victory point is captured by the last unit to occupy the hex. Map
  // indicates the capturing army.
  std::map<std::pair<size_t, size_t>, Unit::ArmyKind> captured_vps_;

  std::vector<std::unique_ptr<Unit>> union_units_;
  std::vector<std::unique_ptr<Unit>> confederate_units_;
};

}} // namespace cifre::gettysburg
