// Copyright 2025, David Goodwin. All rights reserved.

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>

#include "gettysburg_board.h"
#include "common_error.h"
#include "common_util.h"

namespace cifre { namespace gettysburg {

//
// Board
//
Board::Board(const cifre::BoardSize2D& size, const std::vector<Player*>& players,
             size_t starting_player_id) :
  cifre::Board2DHex(BoardCellFactory::PLANE_COUNT, size, players, starting_player_id)
{
  // Player 0 is 'u' (union) and player 1 is 'c' (confederate).
  players[0]->SetChar('u');
  players[1]->SetChar('c');

  // The input planes are based on the size of the board. Because the
  // model is trained/used by always having the unit currently being
  // controlled at the center of the plane, there must be a border
  // around the map to allow any actual map hex to be at the center of
  // the plane.
  plane_row_cnt_ = ((size_.RowCount() - 1) * 2) + 1;
  plane_column_cnt_ = ((size_.ColumnCount() - 1) * 2) + 1;

  cell_factory_ = BoardCellFactory::Instance(
    size_.RowCount(), size_.ColumnCount(), plane_row_cnt_, plane_column_cnt_);

  // Initially set union unit at board position [0, 0] and
  // confederate at board position [row-1, col-1].
  auto union_unit = std::make_unique<Unit>(Unit::ArmyKind::UNION, 1.0, 0, 0);
  union_units_.push_back(std::move(union_unit));

  auto confederate_unit = std::make_unique<Unit>(
    Unit::ArmyKind::CONFEDERATE, 1.0, size_.RowCount() - 1, size_.ColumnCount() - 1);
  confederate_units_.push_back(std::move(confederate_unit));

  // Set starting army and unit. The active unit's position determines
  // where on the planes the board is located; location is set to
  // place that unit in the center of the planes.
  active_army_ =
    (active_player_id_ == 0) ? Unit::ArmyKind::UNION : Unit::ArmyKind::CONFEDERATE;
  active_unit_idx_ = 0;
  active_unit_ = ActiveUnit();
  active_first_row_ = size_.RowCount() - 1 - active_unit_->Row();
  active_first_column_ = size_.ColumnCount() - 1 - active_unit_->Column();
  active_move_factor_expended_ = 0.0;

  // Initialize board inputs, valid actions, and terminal status for
  // the initial active player.
  UpdateInputPlanes();
  UpdateValidActions();
  UpdateTerminalStatus();
}

Board::Board(const Board& other) :
  cifre::Board2DHex(other), cell_factory_(other.cell_factory_),
  plane_row_cnt_(other.plane_row_cnt_), plane_column_cnt_(other.plane_column_cnt_),
  active_army_(other.active_army_), active_unit_idx_(other.active_unit_idx_),
  active_first_row_(other.active_first_row_), active_first_column_(other.active_first_column_),
  active_move_factor_expended_(other.active_move_factor_expended_),
  captured_vps_(other.captured_vps_)
{
  // WARNING: this does not copy 'cells_flattened_', 'cells_' or
  // 'cells_flattened_factory_key_'. To perform a complete copy, use
  // Copy().
  cells_ = nullptr;

  for (const auto& u : other.union_units_) {
    union_units_.push_back(std::make_unique<Unit>(*u));
  }
  for (const auto& u : other.confederate_units_) {
    confederate_units_.push_back(std::make_unique<Unit>(*u));
  }

  active_unit_ = ActiveUnit();
}

Board::~Board()
{
  ReleaseInputPlanes();
}

size_t
Board::Hash() const
{
  constexpr uint64_t mulp = 2654435789;
  uint64_t hash = 104395301;

  // The input planes/cells ('cells_flattened_', 'cells_',
  // 'cells_flattened_factory_key_') themselves are not hashed because
  // they are derived from the other members.
  //
  // active_unit_ is not hashed because it is redundant with
  // active_army_ and active_unit_idx_.

  hash += (plane_row_cnt_ * mulp) ^ (hash >> 23);
  hash += (plane_column_cnt_ * mulp) ^ (hash >> 23);

  hash += (uint64_t(active_army_) * mulp) ^ (hash >> 23);
  hash += (active_unit_idx_ * mulp) ^ (hash >> 23);
  hash += (active_first_row_ * mulp) ^ (hash >> 23);
  hash += (active_first_column_ * mulp) ^ (hash >> 23);
  hash += (uint64_t(active_move_factor_expended_ * 100000.0) * mulp) ^ (hash >> 23);

  if (!captured_vps_.empty()) {
    hash += (captured_vps_.size() * mulp) ^ (hash >> 23);
    for (const auto& cg : captured_vps_) {
      hash += (cg.first.first * mulp) ^ (hash >> 23);
      hash += (cg.first.second * mulp) ^ (hash >> 23);
      hash += (uint64_t(cg.second) * mulp) ^ (hash >> 23);
    }
  }

  for (const auto& u : union_units_) {
    hash += u->Hash();
  }
  for (const auto& u : confederate_units_) {
    hash += u->Hash();
  }

  return hash ^ (hash << 37);
}

bool
Board::Equal(const State* other) const
{
  const Board* ob = static_cast<const Board*>(other);

  // The input planes/cells ('cells_flattened_', 'cells_',
  // 'cells_flattened_factory_key_') themselves are not compared
  // because they are derived from the other members.
  //
  // active_unit_ is not compared because it is redundant with
  // active_army_ and active_unit_idx_.

  if ((plane_row_cnt_ != ob->plane_row_cnt_) ||
      (plane_column_cnt_ != ob->plane_column_cnt_) ||
      (active_army_ != ob->active_army_) ||
      (active_unit_idx_ != ob->active_unit_idx_) ||
      (active_first_row_ != ob->active_first_row_) ||
      (active_first_column_ != ob->active_first_column_) ||
      (active_move_factor_expended_ != ob->active_move_factor_expended_) ||
      (captured_vps_ != ob->captured_vps_)) {
    return false;
  }

  if (union_units_.size() != ob->union_units_.size()) {
    return false;
  }
  for (size_t idx = 0; idx < union_units_.size(); ++idx) {
    if (!union_units_[idx]->Equal(ob->union_units_[idx].get())) {
      return false;
    }
  }

  if (confederate_units_.size() != ob->confederate_units_.size()) {
    return false;
  }
  for (size_t idx = 0; idx < confederate_units_.size(); ++idx) {
    if (!confederate_units_[idx]->Equal(ob->confederate_units_[idx].get())) {
      return false;
    }
  }

  return true;
}

size_t
Board::TotalActionIdCount() const
{
  return size_t(Unit::ActionKind::ACTION_COUNT);
}

std::string
Board::ActionToString(const Action& action) const
{
  if (action.Kind() != Action::ActionKind::ACTIONID) {
    return action.ToString();
  }

  return Unit::ActionKindToString((Unit::ActionKind)action.ActionId());
}

Action
Board::StringToAction(const std::string& str) const
{
  std::string posstr(str);
  common::Trim(posstr);

  // Parse 1 character strings
  if (posstr.size() == 1) {
    switch (posstr[0]) {
    case 'h':
    case '?': return Action(Action::ActionKind::HELP);
    case 'q': return Action(Action::ActionKind::QUIT);
    case 's': return Action(Action::ActionKind::SHOW_STATE);
    case '!': return Action(Action::ActionKind::SHOW_DEBUG_STATE);
    default:
      break;
    }
  }

  common::ToLower(posstr);

  // Moves
  if ((posstr == "up") || (posstr == "u")) {
    return Action(Action::ActionKind::ACTIONID, (uint64_t)Unit::ActionKind::MOVE_UP);
  }
  if ((posstr == "up-right") || (posstr == "upright") || (posstr == "ur")) {
    return Action(Action::ActionKind::ACTIONID, (uint64_t)Unit::ActionKind::MOVE_UPRIGHT);
  }
  if ((posstr == "down-right") || (posstr == "downright") || (posstr == "dr")) {
    return Action(Action::ActionKind::ACTIONID, (uint64_t)Unit::ActionKind::MOVE_DOWNRIGHT);
  }
  if ((posstr == "down") || (posstr == "d")) {
    return Action(Action::ActionKind::ACTIONID, (uint64_t)Unit::ActionKind::MOVE_DOWN);
  }
  if ((posstr == "down-left") || (posstr == "downleft") || (posstr == "dl")) {
    return Action(Action::ActionKind::ACTIONID, (uint64_t)Unit::ActionKind::MOVE_DOWNLEFT);
  }
  if ((posstr == "up-left") || (posstr == "upleft") || (posstr == "ul")) {
    return Action(Action::ActionKind::ACTIONID, (uint64_t)Unit::ActionKind::MOVE_UPLEFT);
  }
  if (posstr == "end") {
    return Action(Action::ActionKind::ACTIONID, (uint64_t)Unit::ActionKind::MOVE_END);
  }

  return Action(Action::ActionKind::INVALID);
}

Unit*
Board::ActiveUnit() const
{
  Unit* unit = nullptr;
  switch (active_army_) {
  case Unit::ArmyKind::UNION:
    unit = union_units_[active_unit_idx_].get();
    break;
  case Unit::ArmyKind::CONFEDERATE:
    unit = confederate_units_[active_unit_idx_].get();
    break;
  }

  return unit;
}

Board*
Board::Copy() const
{
  // The copy constructor does not copy 'cells_flattened_' or set
  // 'cells_', so we need to do it explicitly.
  Board* nb = new Board(*this);
  nb->cells_flattened_.CopyFrom(cells_flattened_);
  nb->cells_ = reinterpret_cast<float*>(nb->cells_flattened_.data());
  nb->cells_flattened_factory_key_ = cells_flattened_factory_key_;
  return nb;
}

Board*
Board::CopyAndUpdate(uint64_t action_id) const
{
  // The copy constructor does not copy 'cells_flattened_' or
  // 'cells_flattened_factory_key_' or set 'cells_', which is what we
  // want because those values will be set by Update() and so setting
  // them now would be wasteful as they will be overwritten.
  Board* nb = new Board(*this);
  nb->Update(action_id);
  return nb;
}

void
Board::Update(uint64_t action_id)
{
  ReleaseInputPlanes();

  Unit::ActionKind action = Unit::ActionKind(action_id);
  if (action == Unit::ActionKind::MOVE_END) {
    NextActiveUnit();
    active_move_factor_expended_ = 0.0;
  }
  else {
    const float move_cost = 1.0; // FIXME

    active_unit_->ApplyAction(action);

    assert((active_move_factor_expended_ + move_cost) <= active_unit_->MoveFactor());
    active_move_factor_expended_ += move_cost;
  }

  active_first_row_ = size_.RowCount() - 1 - active_unit_->Row();
  active_first_column_ = size_.ColumnCount() - 1 - active_unit_->Column();
  UpdateInputPlanes();

  // Check terminal status...
  UpdateTerminalStatus();
  if (terminal_status_.Kind() == cifre::StateTerminalStatus::TerminalKind::NON_TERMINAL) {
    // Not at terminal... if movement is completely exhausted then we
    // want to automatically perform the MOVE_END.
    if (active_move_factor_expended_ >= active_unit_->MoveFactor()) {
      Update(uint64_t(Unit::ActionKind::MOVE_END));
      return;
    }

    UpdateValidActions();
  }
}

void
Board::NextActiveUnit()
{
  // If currently active unit is the last for the current player, then
  // change to the next player.
  bool switch_active_army = false;
  switch (active_army_) {
  case Unit::ArmyKind::UNION:
    switch_active_army = (active_unit_idx_ == (union_units_.size() - 1));
    break;
  case Unit::ArmyKind::CONFEDERATE:
    switch_active_army = (active_unit_idx_ == (confederate_units_.size() - 1));
    break;
  }

  if (switch_active_army) {
    active_player_id_ = (active_player_id_ + 1) % players_.size();
    active_army_ =
      (active_player_id_ == 0) ? Unit::ArmyKind::UNION : Unit::ArmyKind::CONFEDERATE;
    active_unit_idx_ = 0;
  }
  else {
    // Otherwise just move to the next unit in the same army.
    active_unit_idx_ += 1;
  }

  active_unit_ = ActiveUnit();
}

void
Board::UpdateInputPlanes()
{
  // Cells should be released...
  assert(cells_flattened_.empty());

  cells_flattened_factory_key_ =
    std::make_tuple(active_unit_->Row(), active_unit_->Column(),
                    active_unit_->MoveFactor() - active_move_factor_expended_);
  cells_flattened_ = cell_factory_->AcquireCells(cells_flattened_factory_key_);
  cells_ = reinterpret_cast<float*>(cells_flattened_.data());

  // Update planes as required for all units.
  for (const auto& u : union_units_) {
    u->AddToBoard(this);
  }
  for (const auto& u : confederate_units_) {
    u->AddToBoard(this);
  }

  // Update planes for captured victory points.
  for (const auto& cg : captured_vps_) {
    const size_t r = cg.first.first;
    const size_t c = cg.first.second;
    const float vp = VictoryPointAt(r, c);

    assert(vp > 0.0);
    assert(UnionVictoryPointAt(r, c) == 0.0);
    assert(ConfederateVictoryPointAt(r, c) == 0.0);

    switch (cg.second) {
    case Unit::ArmyKind::UNION:
      SetUnionVictoryPointAt(vp, r, c);
      break;
    case Unit::ArmyKind::CONFEDERATE:
      SetConfederateVictoryPointAt(vp, r, c);
      break;
    }
  }
}

void
Board::ReleaseInputPlanes()
{
  if (!cells_flattened_.empty()) {
    // Update planes to remove all units.
    for (const auto& u : union_units_) {
      u->RemoveFromBoard(this);
    }
    for (const auto& u : confederate_units_) {
      u->RemoveFromBoard(this);
    }

    // Update planes to remove captured victory points.
    for (const auto& cg : captured_vps_) {
      const size_t r = cg.first.first;
      const size_t c = cg.first.second;

      switch (cg.second) {
      case Unit::ArmyKind::UNION:
        assert(UnionVictoryPointAt(r, c) > 0.0);
        assert(ConfederateVictoryPointAt(r, c) == 0.0);
        SetUnionVictoryPointAt(0.0, r, c);
        break;
      case Unit::ArmyKind::CONFEDERATE:
        assert(ConfederateVictoryPointAt(r, c) > 0.0);
        assert(UnionVictoryPointAt(r, c) == 0.0);
        SetConfederateVictoryPointAt(0.0, r, c);
        break;
      }
    }

    cell_factory_->ReleaseCells(cells_flattened_factory_key_, std::move(cells_flattened_));
  }
}

void
Board::UpdateValidActions()
{
  valid_action_ids_.clear();
  valid_action_ids_.insert(uint64_t(Unit::ActionKind::MOVE_END));

  const float move_cost = 1.0; // FIXME

  // There must be movement factor available to perform a move.
  if ((active_move_factor_expended_ + move_cost) <= active_unit_->MoveFactor()) {
    const size_t row = active_unit_->Row();
    const size_t col = active_unit_->Column();

    if ((row == 0) && ((col % 2) == 1)) {
      if (col > 0) {
        valid_action_ids_.insert(uint64_t(Unit::ActionKind::MOVE_UPLEFT));
      }
      if (col < (size_.ColumnCount() - 1)) {
        valid_action_ids_.insert(uint64_t(Unit::ActionKind::MOVE_UPRIGHT));
      }
    }
    else if (row > 0) {
      valid_action_ids_.insert(uint64_t(Unit::ActionKind::MOVE_UP));
      if (col > 0) {
        valid_action_ids_.insert(uint64_t(Unit::ActionKind::MOVE_UPLEFT));
      }
      if (col < (size_.ColumnCount() - 1)) {
        valid_action_ids_.insert(uint64_t(Unit::ActionKind::MOVE_UPRIGHT));
      }
    }

    if ((row == (size_.RowCount() - 1)) && ((col % 2) == 0)) {
      if (col > 0) {
        valid_action_ids_.insert(uint64_t(Unit::ActionKind::MOVE_DOWNLEFT));
      }
      if (col < (size_.ColumnCount() - 1)) {
        valid_action_ids_.insert(uint64_t(Unit::ActionKind::MOVE_DOWNRIGHT));
      }
    }
    else if (row < (size_.RowCount() - 1)) {
      valid_action_ids_.insert(uint64_t(Unit::ActionKind::MOVE_DOWN));
      if (col > 0) {
        valid_action_ids_.insert(uint64_t(Unit::ActionKind::MOVE_DOWNLEFT));
      }
      if (col < (size_.ColumnCount() - 1)) {
        valid_action_ids_.insert(uint64_t(Unit::ActionKind::MOVE_DOWNRIGHT));
      }
    }
  }

//  std::cerr
//    << "\np" << active_player_id_
//    << "[" << active_unit_->Row() << "," << active_unit_->Column()
//    << "] valid actions: ";
//  for (auto vid : valid_action_ids_) {
//    std::cerr << vid << ", ";
//  }
//  std::cerr << std::endl;
}

void
Board::UpdateTerminalStatus()
{
  // FIXME, must have some way to set terminal status when game ends
  // due to turns expiring.
  //terminal_status_ =
  //  cifre::StateTerminalStatus(
  //    cifre::StateTerminalStatus::TerminalKind::WINNER, active_player_id_);

  // Otherwise make sure the terminal status is NON_TERMINAL.
  THROWIF(
    terminal_status_.Kind() != cifre::StateTerminalStatus::TerminalKind::NON_TERMINAL,
    "expected NON_TERMINAL status");
}

std::string
Board::BoardCellUpperString(size_t row_idx, size_t col_idx) const
{
  if (!cells_flattened_.empty()) {
    if (UnionAt(row_idx, col_idx) == BoardCellFactory::CELL_ACTIVE) {
      return std::string(1, UNION_CHAR) + " ";
    }
    if (ConfederateAt(row_idx, col_idx) == BoardCellFactory::CELL_ACTIVE) {
      return std::string(1, CONFEDERATE_CHAR) + " ";
    }
  }

  return "  ";
}

std::string
Board::BoardCellLowerString(size_t row_idx, size_t col_idx) const
{
  std::string str("__");

  if (!cells_flattened_.empty()) {
    const bool forest = (ForestAt(row_idx, col_idx) == BoardCellFactory::CELL_ACTIVE);
    const bool hill = (HillAt(row_idx, col_idx) == BoardCellFactory::CELL_ACTIVE);
    const float vp = VictoryPointAt(row_idx, col_idx);

    if (forest && hill) {
      str[0] = 'b';
    }
    else if (forest) {
      str[0] = 'f';
    }
    else if (hill) {
      str[0] = 'h';
    }

    if (vp > 0) {
      if ((vp == std::floor(vp)) && (vp < 10.0)) {
        std::string fs = std::to_string(uint32_t(vp));
        str[1] = fs[0];
      }
      else {
        str[1] = '*';
      }
    }
  }

  return str;
}

std::string
Board::ToDebugString() const
{
  std::stringstream sstr;

  // Show all planes...

  // Top of first row of hex cells.
  for (size_t col_idx = 0; col_idx < plane_column_cnt_; col_idx += 2) {
    if (col_idx == 0) {
      sstr << " __";
    }
    else {
      sstr << "    __";
    }
  }
  sstr << "\n";

  // Rows...
  for (size_t row_idx = 0; row_idx < plane_row_cnt_; ++row_idx) {
    for (size_t col_idx = 0; col_idx < plane_column_cnt_; ++col_idx) {
      if ((col_idx % 2) == 0) {
        sstr << "/" << DebugCellUpperString(row_idx, col_idx) << "\\";
      }
      else if (row_idx > 0) {
        sstr << DebugCellLowerString(row_idx - 1, col_idx);
      }
      else {
        sstr << "__";
      }
    }
    if ((row_idx != 0) && ((plane_column_cnt_ % 2) == 0)) {
      sstr << "/";
    }
    sstr << "\n";

    for (size_t col_idx = 0; col_idx < plane_column_cnt_; ++col_idx) {
      if ((col_idx % 2) == 0) {
        sstr << "\\" << DebugCellLowerString(row_idx, col_idx) << "/";
      }
      else {
        sstr << DebugCellUpperString(row_idx, col_idx);
      }
    }
    if ((plane_column_cnt_ % 2) == 0) {
      sstr << "\\";
    }
    sstr << "\n";
  }

  // Lower half of last row.
  sstr << " ";
  for (size_t col_idx = 0; col_idx < plane_column_cnt_; ++col_idx) {
    if ((col_idx % 2) == 1) {
      sstr << "  \\" << DebugCellLowerString(plane_row_cnt_ - 1, col_idx) << "/";
    }
  }
  sstr << "\n";

  return sstr.str();
}

std::string
Board::DebugCellUpperString(size_t row_idx, size_t col_idx) const
{
  if (!cells_flattened_.empty()) {
    if (PlaneCellValueAt(
          row_idx, col_idx, BoardCellFactory::PLANE_IDX_UNION_UNIT) == BoardCellFactory::CELL_ACTIVE) {
      return std::string(1, UNION_CHAR) + " ";
    }
    if (PlaneCellValueAt(
          row_idx, col_idx, BoardCellFactory::PLANE_IDX_CONFEDERATE_UNIT) == BoardCellFactory::CELL_ACTIVE) {
      return std::string(1, CONFEDERATE_CHAR) + " ";
    }
  }

  return "  ";
}

std::string
Board::DebugCellLowerString(size_t row_idx, size_t col_idx) const
{
  std::string str("__");

  if (!cells_flattened_.empty()) {
    const bool forest =
      (PlaneCellValueAt(
        row_idx, col_idx, BoardCellFactory::PLANE_IDX_FOREST) == BoardCellFactory::CELL_ACTIVE);
    const bool hill =
      (PlaneCellValueAt(
        row_idx, col_idx, BoardCellFactory::PLANE_IDX_HILL) == BoardCellFactory::CELL_ACTIVE);
    const float vp =
      PlaneCellValueAt(row_idx, col_idx, BoardCellFactory::PLANE_IDX_VICTORY_POINT);

    if (forest && hill) {
      str[0] = 'b';
    }
    else if (forest) {
      str[0] = 'f';
    }
    else if (hill) {
      str[0] = 'h';
    }

    if (vp > 0) {
      if ((vp == std::floor(vp)) && (vp < 10.0)) {
        std::string fs = std::to_string(uint32_t(vp));
        str[1] = fs[0];
      }
      else {
        str[1] = '*';
      }
    }
  }

  return str;
}

}} // namespace cifre::gettysburg
