// Copyright 2025, David Goodwin. All rights reserved.
#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>

#include "cifre_action.h"
#include "cifre_boardhex.h"
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
// See cifre::State for information about memory management requires
// classes derived from State. 'cells_flattened_' are handled
// separately from the memory resource arena used for other parts of
// the state. 'cells_flattened_' are handled globally by
// BoardCellFactory as they must persist across player algorithm
// invocations. 'cells_flattened_' is therefore kept in the default
// new/delete memory.
//
class Board : public cifre::BoardHex {
public:
  static constexpr char UNION_CHAR = 'u';
  static constexpr char CONFEDERATE_CHAR = 'c';

public:
  // Create board...
  Board(
    BoardCellFactory* cell_factory, const BoardMoveFactory* move_factory,
    const OOB::Scenario scenario, const cifre::Size2D& board_shape,
    const cifre::Size2D& plane_shape, const std::vector<Player*>& players,
    std::pmr::memory_resource* arena);

  // Destructor
  virtual ~Board();

  // See cifre::State.
  // Hash/compare all members that uniquely define this board.
  virtual size_t Hash() const override;
  virtual bool Equal(const State* other) const override;

  // See cifre::State
  void Update(uint64_t action_id) override;
  StateUniquePtr Copy(std::pmr::memory_resource* arena) const override;
  StateUniquePtr CopyAndUpdate(
    uint64_t action_id, std::pmr::memory_resource* arena) const override;

  // See cifre::State
  size_t TotalActionIdCount() const override {
    return size_t(Unit::ActionKind::ACTION_COUNT); }

  // See cifre::State
  std::string ActionToString(const Action& action) const override;

  // See cifre::State
  Action StringToAction(const std::string& str) const override;

  // Get the representation of the state suitable for feeding to the
  // model via triton. Returns flattened row-major ordering of the
  // cell values. See cifre::State.
  const FlattenedCells& TritonRepresentation() const override {
    return cells_flattened_;
  }

  // See cifre::BoardHex
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
  // Don't allow default copy constructor or assignment to force the
  // version below that requires an arena. Don't allow move assignment
  // as it does not move the allocator for any pmr containers (move
  // constructor is ok).
  Board(Board& other) = delete;
  Board& operator=(Board&) = delete;
  Board(const Board& other) = delete;
  Board& operator=(const Board&) = delete;
  Board& operator=(Board&& other) = delete;

  // Copy constructor. WARNING: this does not copy 'cells_flattened_'
  // , 'cells_' and some other members. To perform a complete copy,
  // use Copy(). This constructor is kept private so that Copy() must
  // be used, see cifre::State.
  Board(const Board& other, std::pmr::memory_resource* arena);

  // Utility method to get the active unit from active_army_ and
  // active_unit_idx_. Return nullptr if active army does not have any
  // units.
  Unit* ActiveUnit();

  // Return union or confederate UNITs at a given board position.
  std::vector<Unit*> UnitsAt(Unit::ArmyKind kind, size_t row_idx, size_t col_idx);
  std::vector<const Unit*> UnitsAt(Unit::ArmyKind kind, size_t row_idx, size_t col_idx) const;

  std::vector<Unit*> UnionAt(size_t row_idx, size_t col_idx) {
    return UnitsAt(Unit::ArmyKind::UNION, row_idx, col_idx); }
  std::vector<const Unit*> UnionAt(size_t row_idx, size_t col_idx) const {
    return UnitsAt(Unit::ArmyKind::UNION, row_idx, col_idx); }

  std::vector<Unit*> ConfederateAt(size_t row_idx, size_t col_idx) {
    return UnitsAt(Unit::ArmyKind::CONFEDERATE, row_idx, col_idx); }
  std::vector<const Unit*> ConfederateAt(size_t row_idx, size_t col_idx) const {
    return UnitsAt(Unit::ArmyKind::CONFEDERATE, row_idx, col_idx); }

  void ReleaseInputPlanes();
  void UpdateInputPlanes();
  void UpdateMoveCosts();
  void UpdateCapturedVictoryPoints();
  void UpdateTerminalStatus();
  void UpdateValidActions();
  void ReinforceUnits();
  bool NextArmyUnit(
    size_t units_count, std::array<Unit, OOB::MAX_UNITS_PER_ARMY_COUNT>& units);

  std::vector<std::string> SmallBoardCell(size_t row_idx, size_t col_idx) const;
  std::vector<std::string> LargeBoardCell(
    bool verbose, size_t row_idx, size_t col_idx) const;
  std::string BoardUnitString(size_t row_idx, size_t col_idx) const;
  std::string SmallBoardTerrainString(size_t row_idx, size_t col_idx) const;
  std::string LargeBoardTerrainString(size_t row_idx, size_t col_idx) const;
  std::string LargeBoardVictoryPointString(size_t row_idx, size_t col_idx) const;

  BoardCellFactory* cell_factory_;
  BoardCellFactory::CellsKey cells_flattened_factory_key_;

  const BoardMoveFactory* move_factory_;

  const OOB::Scenario scenario_;
  const cifre::Size2D plane_shape_;

  Unit::ArmyKind active_army_;
  Unit* active_unit_;
  size_t active_unit_idx_;
  size_t active_first_row_;
  size_t active_first_column_;

  // Cost to move in each direction for the active unit. A negative
  // cost indicates that movement in that direction is not possible.
  std::array<float, BoardHex::DIRECTION_COUNT> active_move_costs_;

  // Integer representing the game turn.
  size_t turn_;

  // Array indicating which army, if any, has captured each
  // victory-point hex. Index 'n' in the array corresponds to
  // BoardCellFactory::Features(VICTORY_POINT, n). A negative value
  // indicates no army owns the victory point, a positive value should
  // be cast to Unit::ArmyKind to determine army.
  std::array<int32_t, BoardCellFactory::MAX_VICTORY_POINT_HEX_COUNT> captured_vps_;
  size_t captured_vps_count_;

  // Union and confederate units.
  size_t union_units_count_, confederate_units_count_;
  std::array<Unit, OOB::MAX_UNITS_PER_ARMY_COUNT> union_units_;
  std::array<Unit, OOB::MAX_UNITS_PER_ARMY_COUNT> confederate_units_;

  // 'cells_' is simply a type-casted pointer into 'cells_flattened_'.
  FlattenedCells cells_flattened_;
  float* cells_;
};

}} // namespace cifre::gettysburg
