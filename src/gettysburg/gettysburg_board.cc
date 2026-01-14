// Copyright 2026, David Goodwin. All rights reserved.

#include "common_error.h"
#include "common_util.h"
#include "gettysburg_board.h"

namespace cifre { namespace gettysburg {

//
// Board
//
Board::Board(
  BoardCellFactory* cell_factory, const BoardMoveFactory* move_factory,
  const OOB::Scenario scenario, const cifre::MatrixSize& board_shape,
  const cifre::MatrixSize& plane_shape, const std::vector<Player*>& players,
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

  return ActionIdToString(action.ActionId());
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

  // Try parsing as a HexDirection. If success then use corresponding
  // move action-id. NONE indicates "end" action and ends the units
  // movement for the turn.
  auto cdir = StringToHexDirection(posstr);
  if (cdir && IsActionIdForHexDirection(*cdir)) {
    auto aid = HexDirectionToActionId(*cdir);
    assert(aid < MOVE_ACTIONID_COUNT);
    return Action(Action::ActionKind::ACTIONID, aid);
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
Board::UnitsAt(Unit::ArmyKind kind, const MatrixCoordinate& pos) const
{
  std::vector<const Unit*> units;
  switch (kind) {
  case Unit::ArmyKind::UNION:
    for (size_t idx = 0; idx < union_units_count_; ++idx) {
      const Unit* u = &union_units_[idx];
      if (u->Position() == pos) {
        units.push_back(u);
      }
    }
    break;
  case Unit::ArmyKind::CONFEDERATE:
    for (size_t idx = 0; idx < confederate_units_count_; ++idx) {
      const Unit* u = &confederate_units_[idx];
      if (u->Position() == pos) {
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
  active_unit_->ApplyActionId(action_id, active_move_costs_);

  UpdateCapturedVictoryPoints();

  // Change active to the next unit. If all units of a player have
  // completed movement then move to next player (and potentially next
  // turn).
  const size_t active_units_count =
    (active_army_ == Unit::ArmyKind::UNION) ? union_units_count_ : confederate_units_count_;
  cifre::array<Unit, OOB::MAX_UNITS_PER_ARMY_COUNT>& active_units =
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
  size_t units_count, cifre::array<Unit, OOB::MAX_UNITS_PER_ARMY_COUNT>& units)
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
           (valid_action_ids_[0] != ACTIONID_MOVE_END))) {
        // Return true indicating now have next active unit in the
        // same army.
        return true;
      }
    }
  }

  // Return false indicating *no* next unit available in the army.
  return false;
}

cifre::array<cifre::array<bool, Board::MAX_BOARD_COLUMNS>, Board::MAX_BOARD_ROWS>
Board::OpponentZOCPositions(
  const MatrixCoordinate& pos, size_t unit_count,
  const cifre::array<Unit, OOB::MAX_UNITS_PER_ARMY_COUNT>& units) const
{
  cifre::array<cifre::array<
    bool, Board::MAX_BOARD_COLUMNS>, Board::MAX_BOARD_ROWS> zocs;

  for (size_t r = 0; r < size_.RowCount(); ++r) {
    zocs[r].fill(false);
  }

  // For each 'units' show all adjacent hexes as in ZOC.
  for (size_t uidx = 0; uidx < unit_count; ++uidx) {
    const Unit& unit = units[uidx];
    for (size_t idx = HEX_RADIUS1_FIRST_DIRECTION_IDX; idx <= HEX_RADIUS1_LAST_DIRECTION_IDX; ++idx) {
      const MatrixCoordinate pos = HexDirectionToPosition(unit.Position(), HexDirection(idx));
      if ((pos.Row() >= 0) && (pos.Column() >= 0) &&
          (pos.Row() < int64_t(size_.RowCount())) &&
          (pos.Column() < int64_t(size_.ColumnCount()))) {
        zocs[pos.Row()][pos.Column()] = true;
      }
    }
  }

  return zocs;
}

void
Board::UpdateMoveCosts()
{
  // Calculate the move cost to perform each move action. Move actions
  // have names like UP-UL but that is not an indication of the exact
  // path of the move, it means that the destination of the move is
  // the hex that is up-1 and then up-left-1 from the current hex. The
  // move cost for UP-UL will be the lowest total move factor required
  // to get from the current hex to the destination hex along any
  // valid path between the two.
  //
  // We find these costs by iterating over the actions and updating
  // minimum move-costs based on neighbor values, until there are no
  // changes. For the problem size this iterative method is fast
  // enough, and maybe even faster than an all-pairs-shortest-path
  // algorithm.

  // Initialize move-cost for each action-id to be maximum cost,
  // except for action "end" (corresponding to HexDirection::NONE)
  // which has move-cost 0.0.
  active_move_costs_.fill(std::numeric_limits<float>::max());
  active_move_costs_[ACTIONID_MOVE_END] = 0.0;

  // Position of the hex corresponding to each action. May indicate
  // positions that are off the board.
  cifre::array<MatrixCoordinate, MOVE_ACTIONID_COUNT> aid_poses;
  for (uint64_t aid = 0; aid < MOVE_ACTIONID_COUNT; ++aid) {
    aid_poses[aid] =
      HexDirectionToPosition(active_unit_->Position(), ActionIdToHexDirection(aid));
  }

  // Set move-cost to -1 for actions that would move unit onto
  // inaccessible hex. The move-cost will remain -1 even as other move
  // costs are minimized below.

  // Cannot move onto an opponent unit so make inaccessible the
  // actions that move the active unit onto an opponent.
  const auto& opponent_units = OpponentUnits();
  for (size_t idx = 0; idx < OpponentUnitsCount(); ++idx) {
    const HexDirection dir =
      PositionToHexDirection(active_unit_->Position(), opponent_units[idx].Position());
    if ((dir != HexDirection::NONE) && IsActionIdForHexDirection(dir)) {
      active_move_costs_[size_t(dir)] = -1.0; // move not allowed
    }
  }

  // Cannot move onto a friendly unit. Occupying the same hex as
  // friendly unit is allowed at intermediate points in the move, but
  // we don't allow that and instead rely on AZ learning how to avoid
  // such "mid-turn" overlaps.
  const auto& friendly_units = FriendlyUnits();
  for (size_t idx = 0; idx < FriendlyUnitsCount(); ++idx) {
    const HexDirection dir =
      PositionToHexDirection(active_unit_->Position(), friendly_units[idx].Position());
    if ((dir != HexDirection::NONE) && IsActionIdForHexDirection(dir)) {
      active_move_costs_[size_t(dir)] = -1.0; // move not allowed
    }
  }

  for (uint64_t aid = 0; aid < MOVE_ACTIONID_COUNT; ++aid) {
    if (active_move_costs_[aid] >= 0.0) {
      const MatrixCoordinate dest = aid_poses[aid];

      // Don't allow action that move unit off board.
      if ((dest.Row() < 0) || (dest.Column() < 0) ||
          (dest.Row() >= int64_t(size_.RowCount())) ||
          (dest.Column() >= int64_t(size_.ColumnCount()))) {
        active_move_costs_[aid] = -1.0; // move not allowed
      }
      else {
        const auto& dest_features = cell_factory_->PositionToCellFeatures(dest);

        // Don't allow action that move unit to non-accessible parts
        // of the board (e.g. cutouts).
        if (dest_features.not_accessible_) {
          active_move_costs_[aid] = -1.0; // move not allowed
        }
        // Artillery units cannot move onto hexes that are both forest
        // and rocky hill.
        else if ((active_unit_->Kind() == Unit::UnitKind::ARTILLERY) &&
                 dest_features.forest_ && dest_features.rocky_hill_) {
          active_move_costs_[aid] = -1.0; // move not allowed
        }
      }
    }
  }

  // Determine which board positions are in an opponent ZOC.
  const auto opponent_zoc_positions =
    OpponentZOCPositions(active_unit_->Position(), OpponentUnitsCount(), OpponentUnits());

  // Iterate to find the minimum move cost. Actions that have a
  // negative cost (inaccessible) remain negative.
  bool changed = true;
  while (changed) {
    changed = false;

    for (uint64_t aid = 0; aid < MOVE_ACTIONID_COUNT; ++aid) {
      // If 'aid' has move-cost that indicates there is some path from
      // the unit to the 'aid' destination hex, then update the
      // move-cost of all hexes adjacent to the 'aid' destination hex
      // to be 'aid's move-cost plus the cost to move from 'aid' hex
      // to action-id corresponding to the adjacent hex. Effectively
      // this propagates move-cost path length.
      if ((active_move_costs_[aid] >= 0.0) &&
          (active_move_costs_[aid] != std::numeric_limits<float>::max())) {
        HexDirection dir = ActionIdToHexDirection(aid);
        MatrixCoordinate aid_pos = aid_poses[aid];
        assert((aid_pos.Row() >= 0) && (aid_pos.Column() >= 0) &&
               (aid_pos.Row() < int64_t(size_.RowCount())) &&
               (aid_pos.Column() < int64_t(size_.ColumnCount())));

        const auto& move_cost = move_factory_->AdjacentMoveCost(aid_pos);
        const auto& zoc_move_cost = move_factory_->AdjacentZOCMoveCost(aid_pos);

        const bool aid_in_zoc = opponent_zoc_positions[aid_pos.Row()][aid_pos.Column()];

        for (size_t idx = HEX_RADIUS1_FIRST_DIRECTION_IDX; idx <= HEX_RADIUS1_LAST_DIRECTION_IDX; ++idx) {
          HexDirection ndir = HexDirectionConcatenate(dir, HexDirection(idx));
          if ((ndir != HexDirection::NONE) && IsActionIdForHexDirection(ndir)) {
            const uint64_t naid = HexDirectionToActionId(ndir);
            if (active_move_costs_[naid] >= 0.0) {
              const MatrixCoordinate adj_pos = HexDirectionToPosition(aid_pos, HexDirection(idx));
              const bool adj_in_zoc =
                opponent_zoc_positions[adj_pos.Row()][adj_pos.Column()];

              float ncost = active_move_costs_[aid];

              if (aid_in_zoc || adj_in_zoc) {
                // If destination hex corresponding to action is in
                // opponent ZOC, then can't move into adjacent hex also
                // in ZOC. Use max cost for this to indicate that this
                // particular path to 'adj_pos' is not allowed but there
                // could be other paths.
                if (aid_in_zoc && adj_in_zoc) {
                  ncost = std::numeric_limits<float>::max();
                }
                // Otherwise either moving into or out-of ZOC which
                // means that need to use ZOC move cost instead of
                // normal move cost.
                else {
                  ncost += BoardMoveFactory::MOVE_ENEMY_ZOC_COST + zoc_move_cost[idx];
                }
              }
              else {
                // Use normal move cost...
                ncost += move_cost[idx];
              }

              if (ncost < active_move_costs_[naid]) {
                assert(active_move_costs_[naid] >= 0.0);
                active_move_costs_[naid] = ncost;
                changed = true;
              }
            }
          }
        }
      }
    }
  }

  // Any actions that still have max cost are inaccessible.
  for (uint64_t aid = 0; aid < MOVE_ACTIONID_COUNT; ++aid) {
    if (active_move_costs_[aid] == std::numeric_limits<float>::max()) {
      active_move_costs_[aid] = -1.0;
    }
  }
}

void
Board::UpdateCapturedVictoryPoints()
{
  // Is the active unit capturing a victory point?
  const auto& features =
    cell_factory_->PositionToCellFeatures(active_unit_->Position());
  if (features.victory_points_ > 0.0) {
    for (size_t idx = 0; idx < captured_vps_count_; ++idx) {
      const auto& cf =
        cell_factory_->IndexToCellFeatures(BoardCellFactory::CellFeatureKind::VICTORY_POINT, idx);
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
  // The active unit's position determines where on the planes the
  // board is located; location is set to place that unit in the
  // center of the planes.
  active_first_row_ = size_.RowCount() - 1 - active_unit_->Position().Row();
  active_first_column_ = size_.ColumnCount() - 1 - active_unit_->Position().Column();

  cells_flattened_factory_key_ = active_unit_->Position();
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
        cell_factory_->IndexToCellFeatures(BoardCellFactory::CellFeatureKind::VICTORY_POINT, idx);
      assert(features.victory_points_ > 0.0);

      assert(BoardCellValueAt(
               MatrixCoordinate(features.row_, features.column_),
               BoardCellFactory::PLANE_IDX_VICTORY_POINT) == features.victory_points_);
      assert(UnionVictoryPointAt(
               MatrixCoordinate(features.row_, features.column_)) == 0.0);
      assert(ConfederateVictoryPointAt(
               MatrixCoordinate(features.row_, features.column_)) == 0.0);

      const Unit::ArmyKind army = Unit::ArmyKind(captured_vps_[idx]);
      switch (army) {
      case Unit::ArmyKind::UNION:
        SetUnionVictoryPointAt(
          features.victory_points_, MatrixCoordinate(features.row_, features.column_));
        break;
      case Unit::ArmyKind::CONFEDERATE:
        SetConfederateVictoryPointAt(
          features.victory_points_, MatrixCoordinate(features.row_, features.column_));
        break;
      }
    }
  }
}

void
Board::UpdateValidActions()
{
  valid_action_ids_.clear();
  valid_action_ids_.push_back(ACTIONID_MOVE_END);

  for (uint64_t aid = 0; aid < MOVE_ACTIONID_COUNT; ++aid) {
    if (aid != ACTIONID_MOVE_END) {
      const float cost = active_move_costs_[aid];
      if ((cost >= 0.0) &&
          (active_unit_->MoveFactorExpended() + cost) <= active_unit_->MoveFactor()) {
        valid_action_ids_.push_back(aid);
      }
    }
  }
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
          cell_factory_->IndexToCellFeatures(BoardCellFactory::CellFeatureKind::VICTORY_POINT, idx);
        assert(features.victory_points_ > 0.0);
        assert(BoardCellValueAt(
                 MatrixCoordinate(features.row_, features.column_),
                 BoardCellFactory::PLANE_IDX_VICTORY_POINT) == features.victory_points_);

        const Unit::ArmyKind army = Unit::ArmyKind(captured_vps_[idx]);
        switch (army) {
        case Unit::ArmyKind::UNION:
          assert(UnionVictoryPointAt(
                   MatrixCoordinate(features.row_, features.column_)) == features.victory_points_);
          uvp += features.victory_points_;
          break;
        case Unit::ArmyKind::CONFEDERATE:
          assert(ConfederateVictoryPointAt(
                   MatrixCoordinate(features.row_, features.column_)) == features.victory_points_);
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
    if ((u.Position().Row() < int64_t(size_.RowCount())) &&
        (u.Position().Column() < int64_t(size_.ColumnCount()))) {
      assert(union_units_count_ < OOB::MAX_UNITS_PER_ARMY_COUNT);
      union_units_[union_units_count_++] = u;
    }
  }

  // Confederate
  for (auto& u : OOB::Units(scenario_, Unit::ArmyKind::CONFEDERATE, turn_)) {
    if ((u.Position().Row() < int64_t(size_.RowCount())) &&
        (u.Position().Column() < int64_t(size_.ColumnCount()))) {
      assert(confederate_units_count_ < OOB::MAX_UNITS_PER_ARMY_COUNT);
      confederate_units_[confederate_units_count_++] = u;
    }
  }
}

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
        cell_factory_->IndexToCellFeatures(BoardCellFactory::CellFeatureKind::VICTORY_POINT, idx);
      assert(features.victory_points_ > 0.0);
      assert(BoardCellValueAt(
               MatrixCoordinate(features.row_, features.column_),
               BoardCellFactory::PLANE_IDX_VICTORY_POINT) == features.victory_points_);

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

BoardHex::SmallBoardCellContent
Board::PositionToSmallCellContent(
  uint32_t display_version, const MatrixCoordinate& pos) const
{
  SmallBoardCellContent content;
  const auto unit_content = PositionToUnitContent(pos);
  const auto terrain_content = PositionToSmallTerrainContent(pos);

  content[0] = {
    ColorChar(' '), ColorChar('_'), ColorChar('_'), ColorChar(' ') };
  content[1] = {
    ColorChar('/'), unit_content[0], unit_content[1], ColorChar('\\') };
  content[2] = {
    ColorChar('\\'), terrain_content[0], terrain_content[1], ColorChar('/') };
  content[3] = {
    ColorChar(' '), ColorChar('_'), ColorChar('_'), ColorChar(' ') };

  return content;
}

BoardHex::LargeBoardCellContent
Board::PositionToLargeCellContent(
  uint32_t display_version, const MatrixCoordinate& pos) const
{
  LargeBoardCellContent content;
  const auto unit_content = PositionToUnitContent(pos);
  const auto vp_content = PositionToVictoryPointContent(pos);
  const auto terrain_content = PositionToLargeTerrainContent(pos);

  content[0] = {
    ColorChar(' '), ColorChar(' '),
    ColorChar('_'), ColorChar('_'), ColorChar('_'), ColorChar('_'),
    ColorChar(' '), ColorChar(' ') };
  content[1] = {
    ColorChar(' '), ColorChar('/'),
    unit_content[0], unit_content[1], ColorChar(' '), ColorChar(' '),
    ColorChar('\\'), ColorChar(' ') };
  content[2] = {
    ColorChar('/'),
    ColorChar(' '), ColorChar(' '), ColorChar(' '),
    ColorChar(' '), ColorChar(' '), ColorChar(' '),
    ColorChar('\\') };
  content[3] = {
    ColorChar('\\'),
    ColorChar(' '), ColorChar(' '), ColorChar(' '),
    ColorChar(' '), vp_content[0], vp_content[1],
    ColorChar('/') };
  content[4] = {
    ColorChar(' '), ColorChar('\\'),
    terrain_content[0], terrain_content[1], terrain_content[2], terrain_content[3],
    ColorChar('/'), ColorChar(' ') };
  content[5] = {
    ColorChar(' '), ColorChar(' '),
    ColorChar('_'), ColorChar('_'), ColorChar('_'), ColorChar('_'),
    ColorChar(' '), ColorChar(' ') };

  return content;
}

// Return 2-character representing the unit(s) in the hex.
cifre::array<ColorChar, 2>
Board::PositionToUnitContent(const MatrixCoordinate& pos) const
{
  cifre::array<ColorChar, 2> chars { ColorChar(' '), ColorChar(' ') };

  const bool active_at =
    ((active_unit_ != nullptr) && (active_unit_->Position() == pos));
  const size_t uc = UnionAt(pos).size();
  if (uc > 0) {
    chars[0] =
      ColorChar(UNION_CHAR, (active_at) ? FGColor::WHITE : FGColor::BLUE,
                (active_at) ? BGColor::BLUE : BGColor::BLACK);
    chars[1] =
      ColorChar((uc >= 10) ? '*' : ((uc > 1) ? ('0' + uc) : ' '),
                FGColor::BLUE, BGColor::BLACK);
  }

  const size_t cc = ConfederateAt(pos).size();
  if (cc > 0) {
    assert(uc == 0);
    chars[0] =
      ColorChar(CONFEDERATE_CHAR, (active_at) ? FGColor::WHITE : FGColor::YELLOW,
                (active_at) ? BGColor::YELLOW : BGColor::BLACK);
    chars[1] =
      ColorChar((cc >= 10) ? '*' : ((cc > 1) ? ('0' + cc) : ' '),
                FGColor::YELLOW, BGColor::BLACK);
  }

  return chars;
}

cifre::array<ColorChar, 2>
Board::PositionToSmallTerrainContent(const MatrixCoordinate& pos) const
{
  cifre::array<ColorChar, 2> chars { ColorChar(' '), ColorChar(' ') };

  const auto& features = cell_factory_->PositionToCellFeatures(pos);
  if (features.forest_ && features.hill_) {
    chars[0] = ColorChar('b');
  }
  else if (features.forest_ && features.rocky_hill_) {
    chars[0] = ColorChar('B');
  }
  else if (features.forest_) {
    chars[0] = ColorChar('f');
  }
  else if (features.rocky_hill_) {
    chars[0] = ColorChar('r');
  }
  else if (features.hill_) {
    chars[0] = ColorChar('h');
  }
  else if (features.town_) {
    chars[0] = ColorChar('t');
  }

  const float vp = features.victory_points_;
  if (vp > 0) {
    if (vp == std::floor(vp)) {
      std::string vps = std::to_string(uint64_t(std::floor(vp)));
      chars[1] = ColorChar((vps.size() == 1) ? vps[0] : '?');
    }
    else {
      chars[1] = ColorChar('?');
    }
  }

  return chars;
}

cifre::array<ColorChar, 4>
Board::PositionToLargeTerrainContent(const MatrixCoordinate& pos) const
{
  cifre::array<ColorChar, 4> chars {
    ColorChar(' '), ColorChar(' '), ColorChar(' '), ColorChar(' ') };

  const auto& features = cell_factory_->PositionToCellFeatures(pos);

  chars[0] = ColorChar((features.forest_) ? 'f' : '_');
  chars[1] = ColorChar((features.hill_) ? 'h' : '_');
  chars[2] = ColorChar((features.rocky_hill_) ? 'r' : '_');
  chars[3] = ColorChar((features.town_) ? 't' : '_');

  return chars;
}

cifre::array<ColorChar, 2>
Board::PositionToVictoryPointContent(const MatrixCoordinate& pos) const
{
  cifre::array<ColorChar, 2> chars { ColorChar(' '), ColorChar(' ') };

  const float uvp = UnionVictoryPointAt(pos);
  const float cvp = ConfederateVictoryPointAt(pos);
  assert((uvp == 0.0) || (cvp == 0.0));

  chars[0] = ColorChar((uvp > 0.0) ? UNION_CHAR : (cvp > 0.0) ? CONFEDERATE_CHAR : ' ');

  const auto& features = cell_factory_->PositionToCellFeatures(pos);
  const float vp = features.victory_points_;
  if (vp > 0.0) {
    if (vp == std::floor(vp)) {
      std::string vps = std::to_string(uint64_t(std::floor(vp)));
      chars[1] = ColorChar((vps.size() == 1) ? vps[0] : '?');
    }
    else {
      chars[1] = ColorChar('?');
    }
  }

  return chars;
}

}} // namespace cifre::gettysburg
