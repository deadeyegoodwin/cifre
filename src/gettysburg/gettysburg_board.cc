// Copyright 2025, David Goodwin. All rights reserved.

#include "common_error.h"
#include "common_util.h"
#include "gettysburg_board.h"

namespace cifre { namespace gettysburg {

//
// Board
//
Board::Board(
  BoardCellFactory* cell_factory, const BoardMoveFactory* move_factory,
  const OOB::Scenario scenario, const cifre::Size2D& board_shape,
  const cifre::Size2D& plane_shape, const std::vector<Player*>& players,
  std::pmr::memory_resource* arena) :
  cifre::BoardHex(BoardCellFactory::PLANE_COUNT, board_shape, players,
                  0 /* starting_player_id */, arena),
  cell_factory_(cell_factory), move_factory_(move_factory),
  scenario_(scenario), plane_shape_(plane_shape), cells_flattened_(arena)
{
  valid_action_ids_.reserve(TotalActionIdCount());

  // Initially no vp hexes are captured. Fill the entire array even
  // though will only use BoardCellFactory::CellFeaturesCount(VICTORY_POINT).
  captured_vps_.fill(-1);
  captured_vps_count_ =
    cell_factory_->CellFeaturesCount(BoardCellFactory::CellFeatureKind::VICTORY_POINT);

  // Turn is represented by a integer. Each day has seven turns plus 1
  // for the night. So for JULY_1 scenario turn 0 is the first turn
  // (6-8 am on July 1st), turn 1 is the next turn (8-10 am on July
  // 1st), ..., turn 7 is the night of July 1st, etc.
  turn_ = 0;

  // Create initial units.
  union_units_count_ = confederate_units_count_ = 0;
  ReinforceUnits();

  // Confederates always start, but in some scenarios they don't have
  // units. In that case union starts.
  assert(active_player_id_ == 0);
  if (confederate_units_count_ > 0) {
    active_army_ = Unit::ArmyKind::CONFEDERATE;
  }
  else {
    assert(union_units_count_ > 0);
    active_army_ = Unit::ArmyKind::UNION;
    active_player_id_ = 1;
  }

  active_unit_idx_ = 0;
  active_unit_ = ActiveUnit();
  assert(active_unit_ != nullptr);

  // Initialize board inputs, valid actions, and terminal status for
  // the initial active player.
  UpdateMoveCosts();
  UpdateCapturedVictoryPoints();
  UpdateValidActions();
  UpdateInputPlanes();
  UpdateTerminalStatus();
}

Board::Board(const Board& other, std::pmr::memory_resource* arena) :
  cifre::BoardHex(other, arena),
  cell_factory_(other.cell_factory_), move_factory_(other.move_factory_),
  scenario_(other.scenario_), plane_shape_(other.plane_shape_),
  active_army_(other.active_army_), active_unit_idx_(other.active_unit_idx_),
  active_first_row_(other.active_first_row_), active_first_column_(other.active_first_column_),
  active_move_costs_(other.active_move_costs_), turn_(other.turn_),
  captured_vps_(other.captured_vps_), // copy entire array (but only first captured_vps_count_ used)
  captured_vps_count_(other.captured_vps_count_),
  union_units_count_(other.union_units_count_),
  confederate_units_count_(other.confederate_units_count_),
  cells_flattened_(arena)
{
  // WARNING: this does not copy 'cells_flattened_', 'cells_' or
  // 'cells_flattened_factory_key_'. To perform a complete copy, use
  // Copy().
  cells_ = nullptr;

  for (size_t idx = 0; idx < union_units_count_; ++idx) {
    union_units_[idx] = other.union_units_[idx];
  }
  for (size_t idx = 0; idx < confederate_units_count_; ++idx) {
    confederate_units_[idx] = other.confederate_units_[idx];
  }

  active_unit_ = ActiveUnit();
  assert(active_unit_ != nullptr);
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
  // 'active_army_' and 'active_unit_idx_' indicate which unit is the
  // active unit (and thereby its position on the board) and so don't
  // need 'active_unit_', 'active_first_row_', 'active_first_column_'
  // and 'active_move_costs_'.

  // FIXME likely need to include 'turn_' in the cell state (turn
  // plane?) and then would need to add it to the hash. For now it is
  // not in the hash so board states are not differentiated by turn.

  hash += (uint64_t(active_army_) * mulp) ^ (hash >> 23);
  hash += (active_unit_idx_ * mulp) ^ (hash >> 23);

  for (size_t idx = 0; idx < captured_vps_count_; ++idx) {
    hash += (captured_vps_[idx] * mulp) ^ (hash >> 23);
  }

  for (size_t idx = 0; idx < union_units_count_; ++idx) {
    hash += union_units_[idx].Hash();
  }
  for (size_t idx = 0; idx < confederate_units_count_; ++idx) {
    hash += confederate_units_[idx].Hash();
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
  // 'active_army_' and 'active_unit_idx_' indicate which unit is the
  // active unit (and thereby its position on the board) and so don't
  // need 'active_unit_', 'active_first_row_', 'active_first_column_'
  // and 'active_move_costs_'.

  // FIXME likely need to include turn_ in the cell state (turn
  // plane?) and then would need to add it to the hash. For now it is
  // not in the hash so board states are not differentiated by turn.

  if ((active_player_id_ != ob->active_player_id_) ||
      (active_army_ != ob->active_army_) ||
      (active_unit_idx_ != ob->active_unit_idx_)) {
    return false;
  }

  for (size_t idx = 0; idx < captured_vps_count_; ++idx) {
    if (captured_vps_[idx] != ob->captured_vps_[idx]) {
      return false;
    }
  }

  if (union_units_count_ != ob->union_units_count_) {
    return false;
  }
  for (size_t idx = 0; idx < union_units_count_; ++idx) {
    if (!union_units_[idx].Equal(ob->union_units_[idx])) {
      return false;
    }
  }

  if (confederate_units_count_ != ob->confederate_units_count_) {
    return false;
  }
  for (size_t idx = 0; idx < confederate_units_count_; ++idx) {
    if (!confederate_units_[idx].Equal(ob->confederate_units_[idx])) {
      return false;
    }
  }

  return true;
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
    case 't': return Action(Action::ActionKind::CUSTOM_0);
    case 'v': return Action(Action::ActionKind::CUSTOM_1);
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
Board::ActiveUnit()
{
  Unit* unit = nullptr;
  switch (active_army_) {
  case Unit::ArmyKind::UNION:
    if (active_unit_idx_ < union_units_count_) {
      unit = &union_units_[active_unit_idx_];
    }
    break;
  case Unit::ArmyKind::CONFEDERATE:
    if (active_unit_idx_ < confederate_units_count_) {
      unit = &confederate_units_[active_unit_idx_];
    }
    break;
  }

  return unit;
}

std::vector<const Unit*>
Board::UnitsAt(Unit::ArmyKind kind, size_t row_idx, size_t col_idx) const
{
  // NOTE: same code as non-const version of UnitsAt().

  std::vector<const Unit*> units;
  switch (kind) {
  case Unit::ArmyKind::UNION:
    for (size_t idx = 0; idx < union_units_count_; ++idx) {
      const Unit* u = &union_units_[idx];
      if ((u->Row() == row_idx) && (u->Column() == col_idx)) {
        units.push_back(u);
      }
    }
    break;
  case Unit::ArmyKind::CONFEDERATE:
    for (size_t idx = 0; idx < confederate_units_count_; ++idx) {
      const Unit* u = &confederate_units_[idx];
      if ((u->Row() == row_idx) && (u->Column() == col_idx)) {
        units.push_back(u);
      }
    }
    break;
  }

  return units;
}

std::vector<Unit*>
Board::UnitsAt(Unit::ArmyKind kind, size_t row_idx, size_t col_idx)
{
  // NOTE: same code as const version of UnitsAt().

  std::vector<Unit*> units;
  switch (kind) {
  case Unit::ArmyKind::UNION:
    for (size_t idx = 0; idx < union_units_count_; ++idx) {
      Unit* u = &union_units_[idx];
      if ((u->Row() == row_idx) && (u->Column() == col_idx)) {
        units.push_back(u);
      }
    }
    break;
  case Unit::ArmyKind::CONFEDERATE:
    for (size_t idx = 0; idx < confederate_units_count_; ++idx) {
      Unit* u = &confederate_units_[idx];
      if ((u->Row() == row_idx) && (u->Column() == col_idx)) {
        units.push_back(u);
      }
    }
    break;
  }

  return units;
}

StateUniquePtr
Board::Copy(std::pmr::memory_resource* arena) const
{
  void* buffer = arena->allocate(sizeof(Board), alignof(Board));
  Board* nb = new(buffer) Board(*this, arena);

  // The copy constructor does not copy 'cells_flattened_' or set
  // 'cells_', so we need to do it explicitly.
  nb->cells_flattened_.assign(cells_flattened_.begin(), cells_flattened_.end());
  nb->cells_ = reinterpret_cast<float*>(nb->cells_flattened_.data());
  nb->cells_flattened_factory_key_ = cells_flattened_factory_key_;

  return StateUniquePtr(nb, StateDeleter<Board>(arena));
}

StateUniquePtr
Board::CopyAndUpdate(uint64_t action_id, std::pmr::memory_resource* arena) const
{
  void* buffer = arena->allocate(sizeof(Board), alignof(Board));
  Board* nb = new(buffer) Board(*this, arena);

  // The copy constructor does not copy 'cells_flattened_' or
  // 'cells_flattened_factory_key_' or set 'cells_', which is what we
  // want because those values will be set by Update() and so setting
  // them now would be wasteful as they will be overwritten.
  nb->Update(action_id);

  return StateUniquePtr(nb, StateDeleter<Board>(arena));
}

void
Board::Update(uint64_t action_id)
{
  const Unit::ActionKind action = Unit::ActionKind(action_id);

  active_unit_->ApplyAction(action, active_move_costs_);

  UpdateCapturedVictoryPoints();

  // Change active to the next unit. If all units of a player have
  // completed movement then move to next player (and potentially next
  // turn).
  const size_t active_units_count =
    (active_army_ == Unit::ArmyKind::UNION) ? union_units_count_ : confederate_units_count_;
  std::array<Unit, OOB::MAX_UNITS_PER_ARMY_COUNT>& active_units =
    (active_army_ == Unit::ArmyKind::UNION) ? union_units_ : confederate_units_;

  if (!NextArmyUnit(active_units_count, active_units)) {
    // Reset move-factor-remaining for all units of the current army.
    for (size_t idx = 0; idx < active_units_count; ++idx) {
      active_units[idx].ResetMoveFactorRemaining();
    }

    // Change to other player / army.
    active_player_id_ = (active_player_id_ + 1) % players_.size();
    active_army_ =
      (active_player_id_ == 0) ? Unit::ArmyKind::CONFEDERATE : Unit::ArmyKind::UNION;
    active_unit_idx_ = 0;

    // Confederate army goes first, so when switching (back) to
    // confederate it is a new turn.
    if (active_army_ == Unit::ArmyKind::CONFEDERATE) {
      turn_ += 1;
      ReinforceUnits();
    }

    if (!NextArmyUnit(
          (active_army_ == Unit::ArmyKind::UNION) ? union_units_count_ : confederate_units_count_,
          (active_army_ == Unit::ArmyKind::UNION) ? union_units_ : confederate_units_)) {
      THROW("at least one army should contain moveable units");
    }
  }

  // Update input planes and terminal status for the new active unit.
  UpdateInputPlanes();
  UpdateTerminalStatus();
}

bool
Board::NextArmyUnit(
  size_t units_count, std::array<Unit, OOB::MAX_UNITS_PER_ARMY_COUNT>& units)
{
  assert(units_count > 0);

  // Check every unit in 'units' to see if they can become the active
  // unit. A unit can become active if it has enough move-factor
  // remaining to perform a movement.
  const size_t curr_idx = active_unit_idx_;
  for (size_t idx = 0; idx < units_count; ++idx) {
    active_unit_idx_ = (curr_idx + idx + 1) % units_count;
    active_unit_ = &units[active_unit_idx_];

    if (active_unit_->MoveFactorRemaining() > 0.0) {
      UpdateMoveCosts();
      UpdateValidActions();
      if ((valid_action_ids_.size() > 1) ||
          ((valid_action_ids_.size() == 1) &&
           (valid_action_ids_[0] != uint64_t(Unit::ActionKind::MOVE_END)))) {
        return true;
      }
    }
  }

  return false;
}

void
Board::UpdateMoveCosts()
{
  active_move_costs_ =
    move_factory_->MoveCost(active_unit_->Row(), active_unit_->Column());

  // Cannot move onto an opponent unit.
  const auto& opponent_units_count =
    ((active_army_ == Unit::ArmyKind::UNION) ? confederate_units_count_ : union_units_count_);
  const auto& opponent_units =
    ((active_army_ == Unit::ArmyKind::UNION) ? confederate_units_ : union_units_);
  for (size_t idx = 0; idx < opponent_units_count; ++idx) {
    Direction dir = active_unit_->AdjacentDirection(opponent_units[idx]);
    if (dir != Direction::NONE) {
      active_move_costs_[size_t(dir)] = -1.0; // move not allowed
    }
  }
}

void
Board::UpdateCapturedVictoryPoints()
{
  // Is the active unit capturing a victory point?
  const auto& features =
    cell_factory_->CellFeaturesFromCoordinates(active_unit_->Row(), active_unit_->Column());
  if (features.victory_points_ > 0.0) {
    for (size_t idx = 0; idx < captured_vps_count_; ++idx) {
      const auto& cf =
        cell_factory_->CellFeaturesFromIndex(BoardCellFactory::CellFeatureKind::VICTORY_POINT, idx);
      assert(cf.victory_points_ > 0.0);
      if ((features.row_ == cf.row_) && (features.column_ == cf.column_)) {
        assert(cf.victory_points_ == features.victory_points_);
        captured_vps_[idx] = int32_t(active_army_);
        break;
      }
    }
  }
}

void
Board::UpdateInputPlanes()
{
  // Cells should be released...
  assert(cells_flattened_.empty());

  // The active unit's position determines where on the planes the
  // board is located; location is set to place that unit in the
  // center of the planes.
  active_first_row_ = size_.RowCount() - 1 - active_unit_->Row();
  active_first_column_ = size_.ColumnCount() - 1 - active_unit_->Column();

  cells_flattened_factory_key_ =
    std::make_pair(active_unit_->Row(), active_unit_->Column());
  const auto& fcf = cell_factory_->PlaneSet(cells_flattened_factory_key_);
  cells_flattened_.assign(fcf.begin(), fcf.end());
  cells_ = reinterpret_cast<float*>(cells_flattened_.data());

  // Update planes as required for all units.
  for (size_t idx = 0; idx < union_units_count_; ++idx) {
    union_units_[idx].AddToBoard(this);
  }
  for (size_t idx = 0; idx < confederate_units_count_; ++idx) {
    confederate_units_[idx].AddToBoard(this);
  }

  // Update planes to show captured victory points.
  for (size_t idx = 0; idx < captured_vps_count_; ++idx) {
    if (captured_vps_[idx] >= 0) {
      const auto& features =
        cell_factory_->CellFeaturesFromIndex(BoardCellFactory::CellFeatureKind::VICTORY_POINT, idx);
      assert(features.victory_points_ > 0.0);

      assert(VictoryPointAt(features.row_, features.column_) == features.victory_points_);
      assert(UnionVictoryPointAt(features.row_, features.column_) == 0.0);
      assert(ConfederateVictoryPointAt(features.row_, features.column_) == 0.0);

      const Unit::ArmyKind army = Unit::ArmyKind(captured_vps_[idx]);
      switch (army) {
      case Unit::ArmyKind::UNION:
        SetUnionVictoryPointAt(
          features.victory_points_, features.row_, features.column_);
        break;
      case Unit::ArmyKind::CONFEDERATE:
        SetConfederateVictoryPointAt(
          features.victory_points_, features.row_, features.column_);
        break;
      }
    }
  }
}

static const std::array<std::pair<BoardHex::Direction, Unit::ActionKind>,
                        BoardHex::DIRECTION_COUNT> direction_actions {{
  { BoardHex::Direction::UP, Unit::ActionKind::MOVE_UP },
  { BoardHex::Direction::UP_RT, Unit::ActionKind::MOVE_UPRIGHT },
  { BoardHex::Direction::DN_RT, Unit::ActionKind::MOVE_DOWNRIGHT },
  { BoardHex::Direction::DN, Unit::ActionKind::MOVE_DOWN },
  { BoardHex::Direction::DN_LT, Unit::ActionKind::MOVE_DOWNLEFT },
  { BoardHex::Direction::UP_LT, Unit::ActionKind::MOVE_UPLEFT } }};

void
Board::UpdateValidActions()
{
  valid_action_ids_.clear();
  valid_action_ids_.push_back(uint64_t(Unit::ActionKind::MOVE_END));

  const size_t row = active_unit_->Row();
  const size_t col = active_unit_->Column();

  for (const auto& pr : direction_actions) {
    // For now don't allow unit to move off of the board...
    if (move_factory_->IsAdjacentHex(row, col, pr.first)) {
      const float cost = active_move_costs_[size_t(pr.first)];
      if ((cost >= 0.0) &&
          (active_unit_->MoveFactorExpended() + cost) <= active_unit_->MoveFactor()) {
        valid_action_ids_.push_back(uint64_t(pr.second));
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
    for (size_t idx = 0; idx < captured_vps_count_; ++idx) {
      if (captured_vps_[idx] >= 0) {
        const auto& features =
          cell_factory_->CellFeaturesFromIndex(BoardCellFactory::CellFeatureKind::VICTORY_POINT, idx);
        assert(features.victory_points_ > 0.0);
        assert(VictoryPointAt(features.row_, features.column_) == features.victory_points_);

        const Unit::ArmyKind army = Unit::ArmyKind(captured_vps_[idx]);
        switch (army) {
        case Unit::ArmyKind::UNION:
          assert(UnionVictoryPointAt(features.row_, features.column_) == features.victory_points_);
          uvp += features.victory_points_;
          break;
        case Unit::ArmyKind::CONFEDERATE:
          assert(ConfederateVictoryPointAt(features.row_, features.column_) == features.victory_points_);
          cvp += features.victory_points_;
          break;
        }
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

void
Board::ReinforceUnits()
{
  // Union
  for (auto& u : OOB::Units(scenario_, Unit::ArmyKind::UNION, turn_)) {
    if ((u.Row() < size_.RowCount()) && (u.Column() < size_.ColumnCount())) {
      assert(union_units_count_ < OOB::MAX_UNITS_PER_ARMY_COUNT);
      union_units_[union_units_count_++] = u;
    }
  }

  // Confederate
  for (auto& u : OOB::Units(scenario_, Unit::ArmyKind::CONFEDERATE, turn_)) {
    if ((u.Row() < size_.RowCount()) && (u.Column() < size_.ColumnCount())) {
      assert(confederate_units_count_ < OOB::MAX_UNITS_PER_ARMY_COUNT);
      confederate_units_[confederate_units_count_++] = u;
    }
  }
}

namespace {

enum class ANSIFGColor {
  DEFAULT        = 0,
  BLACK          = 30,
  RED            = 31,
  GREEN          = 32,
  YELLOW         = 33,
  BLUE           = 34,
  MAGENTA        = 35,
  CYAN           = 36,
  WHITE          = 37,
  BRIGHT_BLACK   = 90,
  BRIGHT_RED     = 91,
  BRIGHT_GREEN   = 92,
  BRIGHT_YELLOW  = 93,
  BRIGHT_BLUE    = 94,
  BRIGHT_MAGENTA = 95,
  BRIGHT_CYAN    = 96,
  BRIGHT_WHITE   = 97
};

enum class ANSIBGColor {
  DEFAULT        = 0,
  BLACK          = 40,
  RED            = 41,
  GREEN          = 42,
  YELLOW         = 43,
  BLUE           = 44,
  MAGENTA        = 45,
  CYAN           = 46,
  WHITE          = 47,
  BRIGHT_BLACK   = 100,
  BRIGHT_RED     = 101,
  BRIGHT_GREEN   = 102,
  BRIGHT_YELLOW  = 103,
  BRIGHT_BLUE    = 104,
  BRIGHT_MAGENTA = 105,
  BRIGHT_CYAN    = 106,
  BRIGHT_WHITE   = 107
};

const std::string
PrintColor(ANSIFGColor fg, ANSIBGColor bg)
{
  return
    std::string("\033[") + std::to_string(uint32_t(fg)) + ";" +
    std::to_string(uint32_t(bg)) + "m";
}

} // namespace

std::string
Board::BoardHeader() const
{
  std::stringstream sstr;

  sstr
    << "Turn " << turn_ << ": "
    << OOB::TurnToString(scenario_, turn_) << "\n"
    << "Player " << ActivePlayer()->Name() << " (p" << ActivePlayerId() << ")"
    << ", " << active_unit_->ToString();

  bool first_vp = true;
  for (size_t idx = 0; idx < captured_vps_count_; ++idx) {
    if (captured_vps_[idx] >= 0) {
      if (first_vp) {
        sstr << "\nVictory Points:";
        first_vp = false;
      }

      const auto& features =
        cell_factory_->CellFeaturesFromIndex(BoardCellFactory::CellFeatureKind::VICTORY_POINT, idx);
      assert(features.victory_points_ > 0.0);
      assert(VictoryPointAt(features.row_, features.column_) == features.victory_points_);

      const Unit::ArmyKind army = Unit::ArmyKind(captured_vps_[idx]);
      sstr
        << "\n  " << features.victory_points_
        << " at [" << features.row_ << "," << features.column_ << "] "
        << Unit::ArmyKindToString(army);
    }
  }

  return sstr.str();
}

std::string
Board::BoardTrailer() const
{
  return "";
}

std::vector<std::string>
Board::BoardCell(BoardHex::DisplayKind kind, size_t row_idx, size_t col_idx) const
{
  switch (kind) {
  case DisplayKind::SMALL:
    return SmallBoardCell(row_idx, col_idx);
  case DisplayKind::LARGE:
    return LargeBoardCell(false /* verbose */, row_idx, col_idx);
  case DisplayKind::LARGE_VERBOSE:
    return LargeBoardCell(true /* verbose */, row_idx, col_idx);
  }

  return {}; // unreachable
}

std::vector<std::string>
Board::SmallBoardCell(size_t row_idx, size_t col_idx) const
{
  return { BoardUnitString(row_idx, col_idx), SmallBoardTerrainString(row_idx, col_idx) };
}

std::vector<std::string>
Board::LargeBoardCell(bool verbose, size_t row_idx, size_t col_idx) const
{
  return {
    BoardUnitString(row_idx, col_idx) + "  ",
    "      ",
    LargeBoardVictoryPointString(row_idx, col_idx),
    LargeBoardTerrainString(row_idx, col_idx) };
}

// Return 2-character string representing the unit(s) in the hex.
std::string
Board::BoardUnitString(size_t row_idx, size_t col_idx) const
{
  if (!cells_flattened_.empty()) {
    const bool active_at =
      ((active_unit_!= nullptr) && (active_unit_->Row() == row_idx) &&
       (active_unit_->Column() == col_idx));
    const size_t uc = UnionAt(row_idx, col_idx).size();
    if (uc > 0) {
      std::string str =
        ((!active_at) ?
         PrintColor(ANSIFGColor::BLUE, ANSIBGColor::BLACK) :
         PrintColor(ANSIFGColor::WHITE, ANSIBGColor::BLUE));
      str += UNION_CHAR;
      str += PrintColor(ANSIFGColor::BLUE, ANSIBGColor::BLACK);
      str += (uc >= 10) ? '*' : ((uc > 1) ? ('0' + uc) : ' ');
      str += PrintColor(ANSIFGColor::DEFAULT, ANSIBGColor::DEFAULT);
      return str;
    }

    const size_t cc = ConfederateAt(row_idx, col_idx).size();
    if (cc > 0) {
      std::string str =
        ((!active_at) ?
         PrintColor(ANSIFGColor::YELLOW, ANSIBGColor::BLACK) :
         PrintColor(ANSIFGColor::WHITE, ANSIBGColor::YELLOW));
      str += CONFEDERATE_CHAR;
      str += PrintColor(ANSIFGColor::YELLOW, ANSIBGColor::BLACK);
      str += (cc >= 10) ? '*' :  ((cc > 1) ? ('0' + cc) : ' ');
      str += PrintColor(ANSIFGColor::DEFAULT, ANSIBGColor::DEFAULT);
      return str;
    }
  }

  return "  ";
}

std::string
Board::SmallBoardTerrainString(size_t row_idx, size_t col_idx) const
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
Board::LargeBoardTerrainString(size_t row_idx, size_t col_idx) const
{
  std::string str("____");

  if (!cells_flattened_.empty()) {
    const bool forest = (ForestAt(row_idx, col_idx) == BoardCellFactory::CELL_ACTIVE);
    const bool hill = (HillAt(row_idx, col_idx) == BoardCellFactory::CELL_ACTIVE);
    const float vp = VictoryPointAt(row_idx, col_idx);

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
}

std::string
Board::LargeBoardVictoryPointString(size_t row_idx, size_t col_idx) const
{
  std::string str = "      ";
  if (!cells_flattened_.empty()) {
    const float uvp = UnionVictoryPointAt(row_idx, col_idx);
    const float cvp = ConfederateVictoryPointAt(row_idx, col_idx);
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

}} // namespace cifre::gettysburg
