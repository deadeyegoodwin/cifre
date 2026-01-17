// Copyright 2026, David Goodwin. All rights reserved.
#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>

#include "cifre_action.h"
#include "cifre_boardhex.h"
#include "cifre_std.h"
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

  static constexpr size_t MAX_BOARD_ROWS = 11;
  static constexpr size_t MAX_BOARD_COLUMNS = 21;

  static constexpr size_t BOARD_CELL_CONTENT_VERSION_COUNT = 1;

public:
  // Create board...
  Board(
    BoardCellFactory* cell_factory, const BoardMoveFactory* move_factory,
    const OOB::Scenario scenario, const cifre::MatrixSize& board_shape,
    const cifre::MatrixSize& plane_shape, const std::vector<Player*>& players,
    std::pmr::memory_resource* arena);

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
  size_t TotalActionIdCount() const override { return TOTAL_ACTIONID_COUNT; }

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

  // See cifre::State.
  std::string ToString() const override;

public:
  // Return the value of the plane cell at the given row, column, and
  // plane. Row and column index are relative to the plane.
  float PlaneCellValueAt(const MatrixCoordinate& pos, size_t plane_idx) const {
    assert((pos.Row() < int64_t(plane_shape_.RowCount())) &&
           (pos.Column() < int64_t(plane_shape_.ColumnCount())) &&
           (plane_idx < BoardCellFactory::PLANE_COUNT));
    return cells_[(pos.Row() * plane_shape_.ColumnCount() * BoardCellFactory::PLANE_COUNT) +
                  (pos.Column() * BoardCellFactory::PLANE_COUNT) + plane_idx]; }

  // Set the value of the plane cell at the given row, column, and
  // plane. Row and column index are relative to the plane.
  void SetPlaneCellValueAt(float value, const MatrixCoordinate& pos, size_t plane_idx) {
    assert((pos.Row() < int64_t(plane_shape_.RowCount())) &&
           (pos.Column() < int64_t(plane_shape_.ColumnCount())) &&
           (plane_idx < BoardCellFactory::PLANE_COUNT));
    cells_[(pos.Row() * plane_shape_.ColumnCount() * BoardCellFactory::PLANE_COUNT) +
           (pos.Column() * BoardCellFactory::PLANE_COUNT) + plane_idx] = value; }

  // Return the value of the board cell at the given row, column, and
  // plane. Row and column index are relative to the visible board.
  float BoardCellValueAt(const MatrixCoordinate& pos, size_t plane_idx) const {
    assert((pos.Row() < int64_t(size_.RowCount())) &&
           (pos.Column() < int64_t(size_.ColumnCount())) &&
           (plane_idx < BoardCellFactory::PLANE_COUNT));
    return cells_[((pos.Row() + active_first_row_) * plane_shape_.ColumnCount() * BoardCellFactory::PLANE_COUNT) +
                  ((pos.Column() + active_first_column_) * BoardCellFactory::PLANE_COUNT) + plane_idx]; }

  // Set the value of the board cell at the given row, column, and
  // plane. Row and column index are relative to the visible board.
  void SetBoardCellValueAt(float value, const MatrixCoordinate& pos, size_t plane_idx) {
    assert((pos.Row() < int64_t(size_.RowCount())) &&
           (pos.Column() < int64_t(size_.ColumnCount())) &&
           (plane_idx < BoardCellFactory::PLANE_COUNT));
    cells_[((pos.Row() + active_first_row_) * plane_shape_.ColumnCount() * BoardCellFactory::PLANE_COUNT) +
           ((pos.Column() + active_first_column_) * BoardCellFactory::PLANE_COUNT) + plane_idx] = value; }

  void SetUnionVictoryPointAt(float value, const MatrixCoordinate& pos) {
    SetBoardCellValueAt(value, pos, BoardCellFactory::PLANE_IDX_UNION_VP); }
  float UnionVictoryPointAt(const MatrixCoordinate& pos) const {
    return BoardCellValueAt(pos, BoardCellFactory::PLANE_IDX_UNION_VP); }

  void SetConfederateVictoryPointAt(float value, const MatrixCoordinate& pos) {
    SetBoardCellValueAt(value, pos, BoardCellFactory::PLANE_IDX_CONFEDERATE_VP); }
  float ConfederateVictoryPointAt(const MatrixCoordinate& pos) const {
    return BoardCellValueAt(pos, BoardCellFactory::PLANE_IDX_CONFEDERATE_VP); }

private:
  // Don't allow default copy constructor or assignment to force the
  // version below that requires an arena. Don't allow move assignment
  // as it does not move the allocator for any pmr containers (move
  // constructor is ok).
  Board(Board&) = delete;
  Board& operator=(Board&) = delete;
  Board(const Board&) = delete;
  Board& operator=(const Board&) = delete;
  Board& operator=(Board&&) = delete;

  // Copy constructor. WARNING: this does not copy 'cells_flattened_'
  // , 'cells_' and some other members. To perform a complete copy,
  // use Copy(). This constructor is kept private so that Copy() must
  // be used, see cifre::State.
  Board(const Board& other, std::pmr::memory_resource* arena);

  // Utility method to get the active unit from active_army_ and
  // active_unit_idx_. Return nullptr if active army does not have any
  // units.
  Unit* ActiveUnit();

  // Return the unit-count for the friendly army of the active unit.
  size_t FriendlyUnitsCount() const {
    return ((active_army_ == Unit::ArmyKind::UNION) ? union_units_count_ : confederate_units_count_); }
  const cifre::array<Unit, OOB::MAX_UNITS_PER_ARMY_COUNT>& FriendlyUnits() const {
    return ((active_army_ == Unit::ArmyKind::UNION) ? union_units_ : confederate_units_); }

  // Return the unit-count for the opponent army of the active unit.
  size_t OpponentUnitsCount() const {
    return ((active_army_ == Unit::ArmyKind::UNION) ? confederate_units_count_ : union_units_count_); }
  const cifre::array<Unit, OOB::MAX_UNITS_PER_ARMY_COUNT>& OpponentUnits() const {
    return ((active_army_ == Unit::ArmyKind::UNION) ? confederate_units_ : union_units_); }

  cifre::array<cifre::array<bool, MAX_BOARD_COLUMNS>, MAX_BOARD_ROWS> OpponentZOCPositions(
    const MatrixCoordinate& pos, size_t unit_count,
    const cifre::array<Unit, OOB::MAX_UNITS_PER_ARMY_COUNT>& units) const;

  // Return union or confederate UNITs at a given board position.
  std::vector<const Unit*> UnitsAt(Unit::ArmyKind kind, const MatrixCoordinate& pos) const;
  std::vector<const Unit*> UnionAt(const MatrixCoordinate& pos) const {
    return UnitsAt(Unit::ArmyKind::UNION, pos); }
  std::vector<const Unit*> ConfederateAt(const MatrixCoordinate& pos) const {
    return UnitsAt(Unit::ArmyKind::CONFEDERATE, pos); }

  void UpdateInputPlanes();
  void UpdateMoveCosts();
  void UpdateCapturedVictoryPoints();
  void UpdateTerminalStatus();
  void UpdateValidActions();
  void ReinforceUnits();
  bool NextArmyUnit(
    size_t units_count, cifre::array<Unit, OOB::MAX_UNITS_PER_ARMY_COUNT>& units);

  // See cifre::BoardHex
  SmallBoardCellContent PositionToSmallCellContent(
    uint32_t display_version, const MatrixCoordinate& pos) const override;
  LargeBoardCellContent PositionToLargeCellContent(
    uint32_t display_version, const MatrixCoordinate& pos) const override;

  static constexpr FGColor STREAM_FG_COLOR = FGColor::BRIGHT_BLUE;
  static constexpr BGColor STREAM_BG_COLOR = BGColor::BLACK;

  cifre::array<ColorChar, HEX_RADIUS1_LAST_DIRECTION_IDX + 1> PositionToEdgeContent(
    const MatrixCoordinate& pos) const;
  cifre::array<ColorChar, 2> PositionToUnitContent(const MatrixCoordinate& pos) const;
  cifre::array<ColorChar, 2> PositionToSmallTerrainContent(const MatrixCoordinate& pos) const;
  cifre::array<ColorChar, 4> PositionToLargeTerrainContent(const MatrixCoordinate& pos) const;
  cifre::array<ColorChar, 2> PositionToVictoryPointContent(const MatrixCoordinate& pos) const;

  BoardCellFactory* cell_factory_;
  BoardCellFactory::CellsKey cells_flattened_factory_key_;

  const BoardMoveFactory* move_factory_;

  const OOB::Scenario scenario_;
  const cifre::MatrixSize plane_shape_;

  Unit::ArmyKind active_army_;
  Unit* active_unit_;
  size_t active_unit_idx_;
  size_t active_first_row_;
  size_t active_first_column_;

  // Cost to move in each direction for the active unit. A negative
  // cost indicates that movement in that direction is not possible.
  cifre::array<float, MOVE_ACTIONID_COUNT> active_move_costs_;

  // Integer representing the game turn.
  size_t turn_;

  // Array indicating which army, if any, has captured each
  // victory-point hex. Index 'n' in the array corresponds to
  // BoardCellFactory::Features(VICTORY_POINT, n). A negative value
  // indicates no army owns the victory point, a positive value should
  // be cast to Unit::ArmyKind to determine army.
  cifre::array<int32_t, BoardCellFactory::MAX_VICTORY_POINT_HEX_COUNT> captured_vps_;
  size_t captured_vps_count_;

  // Union and confederate units.
  size_t union_units_count_, confederate_units_count_;
  cifre::array<Unit, OOB::MAX_UNITS_PER_ARMY_COUNT> union_units_;
  cifre::array<Unit, OOB::MAX_UNITS_PER_ARMY_COUNT> confederate_units_;

  // 'cells_' is simply a type-casted pointer into 'cells_flattened_'.
  FlattenedCells cells_flattened_;
  float* cells_;
};

}} // namespace cifre::gettysburg
