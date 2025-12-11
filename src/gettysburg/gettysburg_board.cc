// Copyright 2025, David Goodwin. All rights reserved.

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>

#include "common_error.h"
#include "common_util.h"
#include "gettysburg_board.h"

namespace cifre { namespace gettysburg {

//
// Board
//
Board::Board(
  const std::shared_ptr<BoardCellFactory>& factory, const OOB::Scenario scenario,
  const cifre::BoardSize2D& board_shape, const cifre::BoardSize2D& plane_shape,
  const std::vector<Player*>& players) :
  cifre::Board2DHex(BoardCellFactory::PLANE_COUNT, board_shape, players, 0 /* starting_player_id */),
  cell_factory_(factory), scenario_(scenario), plane_shape_(plane_shape)
{
  // Turn is represented by a integer. Each day has seven turns plus 1
  // for the night. So for JULY_1 scenario turn 0 is the first turn
  // (6-8 am on July 1st), turn 1 is the next turn (8-10 am on July
  // 1st), ..., turn 7 is the night of July 1st, etc.
  turn_ = 0;

  // Create initial units.
  UpdateUnits();

  // Confederates always start, but in some scenarios they don't have
  // units. In that case union starts.
  assert(active_player_id_ == 0);
  if (!confederate_units_.empty()) {
    active_army_ = Unit::ArmyKind::CONFEDERATE;
  }
  else {
    assert(!union_units_.empty());
    active_army_ = Unit::ArmyKind::UNION;
    active_player_id_ = 1;
  }

  active_unit_idx_ = 0;
  active_unit_ = ActiveUnit();
  assert(active_unit_ != nullptr);

  // The active unit's position determines where on the planes the
  // board is located; location is set to place that unit in the
  // center of the planes.
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
  scenario_(other.scenario_), plane_shape_(other.plane_shape_),
  active_army_(other.active_army_), active_unit_idx_(other.active_unit_idx_),
  active_first_row_(other.active_first_row_), active_first_column_(other.active_first_column_),
  active_move_factor_expended_(other.active_move_factor_expended_),
  turn_(other.turn_), captured_vps_(other.captured_vps_)
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
  assert(active_unit_ != nullptr);
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

  // Must hash 'active_player_id_' from base class (we are not calling
  // base-class Hash because that would include the 'cells_'.
  hash += (active_player_id_ * mulp) ^ (hash >> 23);

  // The input planes/cells ('cells_flattened_', 'cells_',
  // 'cells_flattened_factory_key_') themselves are not hashed because
  // they are derived from the other members.
  //
  // 'scenario_' is not hashed because it is equal for all boards.
  // 'plane_shape_' is not hashed because it is equal for all boards.
  //
  // active_unit_ is not hashed because it is redundant with
  // active_army_ and active_unit_idx_.

  // FIXME likely need to include turn_ in the cell state (turn
  // plane?) and then would need to add it to the hash. For now it is
  // not in the hash so board states are not differentiated by turn.

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
  // 'scenario_' is not compared because it is equal for all boards.
  // 'plane_shape_' is not compared because it is same for all boards.
  //
  // active_unit_ is not compared because it is redundant with
  // active_army_ and active_unit_idx_.

  // FIXME likely need to include turn_ in the cell state (turn
  // plane?) and then would need to add it to the hash. For now it is
  // not in the hash so board states are not differentiated by turn.

  if ((active_player_id_ != ob->active_player_id_) ||
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
    if (active_unit_idx_ < union_units_.size()) {
      unit = union_units_[active_unit_idx_].get();
    }
    break;
  case Unit::ArmyKind::CONFEDERATE:
    if (active_unit_idx_ < confederate_units_.size()) {
      unit = confederate_units_[active_unit_idx_].get();
    }
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
    assert(!union_units_.empty());
    switch_active_army = (active_unit_idx_ == (union_units_.size() - 1));
    break;
  case Unit::ArmyKind::CONFEDERATE:
    assert(!confederate_units_.empty());
    switch_active_army = (active_unit_idx_ == (confederate_units_.size() - 1));
    break;
  }

  if (switch_active_army) {
    active_player_id_ = (active_player_id_ + 1) % players_.size();
    active_army_ =
      (active_player_id_ == 0) ? Unit::ArmyKind::CONFEDERATE : Unit::ArmyKind::UNION;
    active_unit_idx_ = 0;

    // Confederate army goes first, so when switching (back) to
    // confederate it is a new turn.
    if (active_army_ == Unit::ArmyKind::CONFEDERATE) {
      turn_ += 1;
      UpdateUnits();
    }
  }
  else {
    // Otherwise just move to the next unit in the same army.
    active_unit_idx_ += 1;
  }

  // If there are no units in the newly active army, then switch back
  // to other army. Avoid circular loop by making sure both armies
  // aren't empty (should never happen).
  active_unit_ = ActiveUnit();
  if (active_unit_ == nullptr) {
    THROWIF(union_units_.empty() && confederate_units_.empty(),
            "at least one army should contain units");
    NextActiveUnit();
  }
}

void
Board::UpdateUnits()
{
  // Union
  for (auto& u : OOB::Units(scenario_, Unit::ArmyKind::UNION, turn_)) {
    if ((u->Row() < size_.RowCount()) && (u->Column() < size_.ColumnCount())) {
      union_units_.emplace_back(std::move(u));
    }
  }

  // Confederate
  for (auto& u : OOB::Units(scenario_, Unit::ArmyKind::CONFEDERATE, turn_)) {
    if ((u->Row() < size_.RowCount()) && (u->Column() < size_.ColumnCount())) {
      confederate_units_.emplace_back(std::move(u));
    }
  }
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

  // Is the active unit capturing a victory point?
  if (VictoryPointAt(active_unit_->Row(), active_unit_->Column()) > 0.0) {
    captured_vps_[std::make_pair(active_unit_->Row(), active_unit_->Column())] = active_army_;
  }

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
  // If scenario is complete, then set terminal status based on the
  // number of victory points accrued by each army.
  if (turn_ > OOB::LastTurn(scenario_)) {
    turn_ = OOB::LastTurn(scenario_);

    float cvp = 0.0, uvp = 0.0;
    for (const auto& cg : captured_vps_) {
      switch (cg.second) {
      case Unit::ArmyKind::UNION:
        assert(UnionVictoryPointAt(cg.first.first, cg.first.second) > 0.0);
        uvp += UnionVictoryPointAt(cg.first.first, cg.first.second);
        break;
      case Unit::ArmyKind::CONFEDERATE:
        assert(ConfederateVictoryPointAt(cg.first.first, cg.first.second) > 0.0);
        cvp += ConfederateVictoryPointAt(cg.first.first, cg.first.second);
        break;
      }
    }

    if (cvp == uvp) {
      terminal_status_ =
        cifre::StateTerminalStatus(cifre::StateTerminalStatus::TerminalKind::DRAW);
    }
    else {
      terminal_status_ =
        cifre::StateTerminalStatus(
          cifre::StateTerminalStatus::TerminalKind::WINNER,
          (cvp > uvp) ? 0 : 1 /* player_id */);
    }
  }
  else {
    // Otherwise make sure the terminal status is NON_TERMINAL.
    THROWIF(
      terminal_status_.Kind() != cifre::StateTerminalStatus::TerminalKind::NON_TERMINAL,
      "expected NON_TERMINAL status");
  }
}

std::string
Board::PreBoardString() const
{
  std::stringstream sstr;

  sstr
    << "Turn " << turn_ << ": "
    << OOB::TurnToString(scenario_, turn_) << "\n"
    << "Player " << ActivePlayer()->Name() << " (p" << ActivePlayerId() << ")"
    << ", " << active_unit_->ToString();

  if (!captured_vps_.empty()) {
    sstr << "\nVictory Points:";
    for (const auto& cg : captured_vps_) {
      sstr
        << "\n  " << VictoryPointAt(cg.first.first, cg.first.second)
        << " at [" << cg.first.first << "," << cg.first.second << "] "
        << Unit::ArmyKindToString(cg.second);
    }
  }

  return sstr.str();
}

std::string
Board::PostBoardString() const
{
  return "";
}

std::string
Board::BoardCellUpperString(size_t row_idx, size_t col_idx) const
{
  std::string str("  ");
  if (!cells_flattened_.empty()) {
    uint32_t uc = uint32_t(UnionAt(row_idx, col_idx));
    if (uc > 0) {
      str[0] = UNION_CHAR;
      str[1] = (uc >= 10) ? '*' : ((uc > 1) ? ('0' + uc) : ' ');
    }

    uint32_t cc = uint32_t(ConfederateAt(row_idx, col_idx));
    if (cc > 0) {
      str[0] = CONFEDERATE_CHAR;
      str[1] = (cc >= 10) ? '*' :  ((cc > 1) ? ('0' + cc) : ' ');
    }
  }

  return str;
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
      if (vp == std::floor(vp)) {
        std::string vps = std::to_string(uint64_t(std::floor(vp)));
        str[1] = (vps.size() == 1) ? vps[0] : '?';
      }
      else {
        str[1] = '?';
      }
    }
  }

  return str;
}

std::string
Board::ToDebugString() const
{
  std::stringstream sstr;

  // Bigger hexes so can show more information...
  //   ____        ____
  //  /    \      /    \     <- loc 0 row
  // /      \____/      \    <- loc 1 row
  // \      /    \      /    <- loc 2 row
  //  \____/      \____/     <- loc 3 row
  //       \      /
  //        \____/

  // Top of first row of hex cells.
  for (size_t col_idx = 0; col_idx < plane_shape_.ColumnCount(); col_idx += 2) {
    if (col_idx == 0) {
      sstr << "  ____";
    }
    else {
      sstr << "        ____";
    }
  }
  sstr << "\n";

  // Rows...
  for (size_t row_idx = 0; row_idx < plane_shape_.RowCount(); ++row_idx) {
    // Loc 0...
    for (size_t col_idx = 0; col_idx < plane_shape_.ColumnCount(); ++col_idx) {
      if ((col_idx % 2) == 0) {
        if (col_idx == 0) {
          sstr << " ";
        }
        sstr << "/" << DebugCellString(0 /* loc */, row_idx, col_idx) << "\\";

      }
      else if (row_idx > 0) {
        sstr << DebugCellString(2 /* loc */, row_idx - 1, col_idx);
      }
      else {
        sstr << "      ";
      }
    }
    if ((row_idx != 0) && ((plane_shape_.ColumnCount() % 2) == 0)) {
      sstr << "/";
    }
    sstr << "\n";

    // Loc 1...
    for (size_t col_idx = 0; col_idx < plane_shape_.ColumnCount(); ++col_idx) {
      if ((col_idx % 2) == 0) {
        sstr << "/" << DebugCellString(1 /* loc */, row_idx, col_idx) << "\\";
      }
      else if (row_idx > 0) {
        sstr << DebugCellString(3 /* loc */, row_idx - 1, col_idx);
      }
      else {
        sstr << "____";
      }
    }
    if ((row_idx != 0) && ((plane_shape_.ColumnCount() % 2) == 0)) {
      sstr << "/";
    }
    sstr << "\n";

    // Loc 2...
    for (size_t col_idx = 0; col_idx < plane_shape_.ColumnCount(); ++col_idx) {
      if ((col_idx % 2) == 0) {
        sstr << "\\" << DebugCellString(2 /* loc */, row_idx, col_idx) << "/";
      }
      else {
        sstr << DebugCellString(0 /* loc */, row_idx, col_idx);
      }
    }
    if ((plane_shape_.ColumnCount() % 2) == 0) {
      sstr << "\\";
    }
    sstr << "\n";

    // Loc 3...
    for (size_t col_idx = 0; col_idx < plane_shape_.ColumnCount(); ++col_idx) {
      if ((col_idx % 2) == 0) {
        if (col_idx == 0) {
          sstr << " ";
        }
        sstr << "\\" << DebugCellString(3 /* loc */, row_idx, col_idx) << "/";
      }
      else {
        sstr << DebugCellString(1 /* loc */, row_idx, col_idx);
      }
    }
    if ((plane_shape_.ColumnCount() % 2) == 0) {
      sstr << "\\";
    }
    sstr << "\n";
  }

  // Lower half of last row.
  sstr << "  ";
  for (size_t col_idx = 0; col_idx < plane_shape_.ColumnCount(); ++col_idx) {
    if ((col_idx % 2) == 1) {
      sstr << "    \\" << DebugCellString(2 /* loc */, plane_shape_.RowCount() - 1, col_idx) << "/";
    }
  }
  sstr << "\n ";
  for (size_t col_idx = 0; col_idx < plane_shape_.ColumnCount(); ++col_idx) {
    if ((col_idx % 2) == 1) {
      sstr << "      \\" << DebugCellString(3 /* loc */, plane_shape_.RowCount() - 1, col_idx) << "/";
    }
  }
  sstr << "\n";

  return sstr.str();
}

std::string
Board::DebugCellString(size_t loc, size_t row_idx, size_t col_idx) const
{
  switch (loc) {
  case 0: {
    std::string str("    ");
    if (!cells_flattened_.empty()) {
      uint32_t uc =
        uint32_t(PlaneCellValueAt(row_idx, col_idx, BoardCellFactory::PLANE_IDX_UNION_UNIT));
      if (uc > 0) {
        str[0] = UNION_CHAR;
        str[1] = (uc >= 10) ? '*' : ((uc > 1) ? ('0' + uc) : ' ');
      }

      uint32_t cc =
        uint32_t(PlaneCellValueAt(row_idx, col_idx, BoardCellFactory::PLANE_IDX_CONFEDERATE_UNIT));
      if (cc > 0) {
        str[0] = CONFEDERATE_CHAR;
        str[1] = (cc >= 10) ? '*' : ((cc > 1) ? ('0' + cc) : ' ');
      }
    }
    return str;
  }

  case 1: {
    std::string str = "      ";
    return str;
  }

  case 2: {
    std::string str = "      ";
    if (!cells_flattened_.empty()) {
      const float uvp = PlaneCellValueAt(row_idx, col_idx, BoardCellFactory::PLANE_IDX_UNION_VP);
      const float cvp = PlaneCellValueAt(row_idx, col_idx, BoardCellFactory::PLANE_IDX_CONFEDERATE_VP);
      assert((uvp == 0.0) || (cvp == 0.0));

      str[4] = ((uvp > 0.0) ? UNION_CHAR : (cvp > 0.0) ? CONFEDERATE_CHAR : ' ');
      const float vp = std::max(uvp, cvp);
      if (vp > 0.0) {
        if (vp == std::floor(vp)) {
          std::string vps = std::to_string(uint64_t(std::floor(vp)));
          str[5] = (vps.size() == 1) ? vps[0] : '?';
        }
        else {
          str[5] = '?';
        }
      }
    }
    return str;
  }

  case 3: {
    std::string str = "____";
    if (!cells_flattened_.empty()) {
      const bool forest =
        (PlaneCellValueAt(
          row_idx, col_idx, BoardCellFactory::PLANE_IDX_FOREST) == BoardCellFactory::CELL_ACTIVE);
      const bool hill =
        (PlaneCellValueAt(
          row_idx, col_idx, BoardCellFactory::PLANE_IDX_HILL) == BoardCellFactory::CELL_ACTIVE);
      const float vp =
        PlaneCellValueAt(row_idx, col_idx, BoardCellFactory::PLANE_IDX_VICTORY_POINT);

      str[0] = ((forest) ? 'f' : '_');
      str[1] = ((hill) ? 'h' : '_');

      if (vp > 0) {
        if (vp == std::floor(vp)) {
          std::string vps = std::to_string(uint64_t(std::floor(vp)));
          str[3] = (vps.size() == 1) ? vps[0] : '?';
        }
        else {
          str[3] = '?';
        }
      }
    }
    return str;
  }}

  THROW("unexpected loc value");
  return "";
}

}} // namespace cifre::gettysburg
