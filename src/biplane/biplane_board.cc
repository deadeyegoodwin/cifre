// Copyright 2026, David Goodwin. All rights reserved.

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>

#include "biplane_board.h"
#include "common_error.h"
#include "common_util.h"

namespace cifre { namespace biplane {

//
// Board
//
Board::Board(const size_t max_distance, const std::vector<Player*>& players,
             size_t starting_player_id, std::pmr::memory_resource* arena) :
  max_distance_(max_distance),
  visible_2d_size_((max_distance * 2) + 1, (max_distance * 2) + 1),
  active_player_id_(starting_player_id),
  terminal_status_(cifre::StateTerminalStatus::TerminalKind::NON_TERMINAL),
  valid_action_ids_(arena), cells_flattened_(arena)
{
  THROWIF(max_distance_ > MAX_MAX_DISTANCE,
          "max-distance must be <= " + std::to_string(MAX_MAX_DISTANCE));

  THROWIF(players.size() != PLAYER_COUNT, "exactly 2 players are required");
  THROWIF(NoPlayerId() != PLAYER_COUNT, "exactly 2 players are required");
  THROWIF(active_player_id_ >= NoPlayerId(),
          "invalid starting player index " + std::to_string(active_player_id_));

  // Start at opposite sides of the board. Active player
  // position/orientation are absolute, other players are relative to
  // active player.
  for (size_t idx = 0; idx < PLAYER_COUNT; ++idx) {
    players_[idx] = players[idx];
    if (idx == active_player_id_) {
      player_positions_[idx] = MatrixCoordinate(0, 0);
      player_orientations_[idx] = BiplaneOrientation::NORTH_EAST;
    }
    else {
      player_positions_[idx] = MatrixCoordinate(max_distance_ - 1, max_distance_ - 1);
      player_orientations_[idx] = BiplaneOrientation::WEST;
    }
  }

#ifndef NDEBUG
  for (size_t pidx = 0; pidx < players_.size(); ++pidx) {
    assert(players_[pidx]->Id() == pidx);
  }
#endif

  cells_flattened_.resize(NUM_PLANES * visible_2d_size_.ElementCount() * sizeof(float));
  cells_ = reinterpret_cast<float*>(cells_flattened_.data());

  // Initialize 'valid_action_ids_' and 'terminal_status_'.
  valid_action_ids_.reserve(TotalActionIdCount());
  UpdateValidActions();
  UpdateTerminalStatus();
}

Board::Board(const Board& other, std::pmr::memory_resource* arena) :
  max_distance_(other.max_distance_), visible_2d_size_(other.visible_2d_size_),
  players_(other.players_),
  player_positions_(other.player_positions_), player_orientations_(other.player_orientations_),
  active_player_id_(other.active_player_id_), terminal_status_(other.terminal_status_),
  valid_action_ids_(other.valid_action_ids_.begin(), other.valid_action_ids_.end(), arena),
  cells_flattened_(arena), cells_(nullptr)
{
  // WARNING: this does not copy 'cells_flattened_' or 'cells_'.
}

size_t
Board::Hash() const
{
  constexpr uint64_t mulp = 2654435789;
  uint64_t hash = 104395301;

  hash += (active_player_id_ * mulp) ^ (hash >> 23);

  // The active player is not included in the hash because it is
  // always considered to be the "center". Other player's relative
  // position and orientation are included in the hash.
  //
  // FIXME the order that we visit players should not matter here so
  // once we support more than 2 players we should sort based on
  // relative position.
  for (size_t idx = 0; idx < PLAYER_COUNT; ++idx) {
    if (idx != active_player_id_) {
      hash += MatrixCoordinateHash()(player_positions_[idx]) ^ (hash >> 23);
      hash += (size_t(player_orientations_[idx]) * mulp) ^ (hash >> 23);
    }
  }

  return hash ^ (hash << 37);
}

bool
Board::Equal(const State* other) const
{
  if (active_player_id_ != other->ActivePlayerId()) {
    return false;
  }

  const Board* ob = static_cast<const Board*>(other);

  // The active player is not included in the equality because it is
  // always considered to be the "center". Other player's relative
  // position and orientation are included in the equality.
  //
  // FIXME the order that we visit players should not matter here so
  // once we support more than 2 players we should sort based on
  // relative position.
  for (size_t idx = 0; idx < PLAYER_COUNT; ++idx) {
    if (idx != active_player_id_) {
      if ((player_positions_[idx] != ob->player_positions_[idx]) ||
          (player_orientations_[idx] != ob->player_orientations_[idx])) {
        return false;
      }
    }
  }

  return true;
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

  return StateUniquePtr(nb, StateDeleter<Board>(arena));
}

StateUniquePtr
Board::CopyAndUpdate(uint64_t action_id, std::pmr::memory_resource* arena) const
{
  void* buffer = arena->allocate(sizeof(Board), alignof(Board));
  Board* nb = new(buffer) Board(*this, arena);

  // The copy constructor does not copy 'cells_flattened_' or set
  //  'cells_', which is what we want because those values will be set
  //  by Update() and so setting them now would be wasteful as they
  //  will be overwritten.
  nb->Update(action_id);

  return StateUniquePtr(nb, StateDeleter<Board>(arena));
}

void
Board::Update(uint64_t action_id)
{
  // Move the active player as indicated by 'action_id'.
  //
  // 1) convert non-active players to absolute coordinates, 2) move
  // active player, 3) determine the new active player, 4) convert
  // non-active players to relative coordinates.
  for (size_t idx = 0; idx < PLAYER_COUNT; ++idx) {
    if (idx != active_player_id_) {
      RelativePositionToAbsolute(player_positions_[idx], player_orientations_[idx]);
    }
  }

  ApplyAction(action_id);

  // For now we just make the next player the active player. Update
  // the valid actions for the now-active player.
  active_player_id_ = (active_player_id_ + 1) % players_.size();
  UpdateValidActions();

  for (size_t idx = 0; idx < PLAYER_COUNT; ++idx) {
    if (idx != active_player_id_) {
      AbsolutePositionToRelative(player_positions_[idx], player_orientations_[idx]);
    }
  }

  // Allocate and set the cells representing the gameboard to show
  // where each opponent biplane is relative to the active player.
  cells_flattened_.resize(NUM_PLANES * visible_2d_size_.ElementCount() * sizeof(float));
  cells_ = reinterpret_cast<float*>(cells_flattened_.data());

  for (size_t idx = 0; idx < PLAYER_COUNT; ++idx) {
    if (idx != active_player_id_) {
      // Player may have flown off the board so need to check before
      // attempting to set the cell.
      const MatrixCoordinate& pos = player_positions_[idx];
      if ((abs(pos.Row()) <= max_distance_) && (abs(pos.Column()) <= max_distance_)) {
        SetCellValueAt(
          CELL_ACTIVE, pos + max_distance_, size_t(player_orientations_[idx]));
      }
    }
  }

  UpdateTerminalStatus();
}

bool
Board::IsValidAction(const Action& action) const
{
  switch (action.Kind()) {
  case Action::ActionKind::INVALID:
    return false;

  case Action::ActionKind::HELP:
  case Action::ActionKind::QUIT:
  case Action::ActionKind::SHOW_STATE:
  case Action::ActionKind::QUERY_ALPHAZERO:
  case Action::ActionKind::CUSTOM_0:
  case Action::ActionKind::CUSTOM_1:
  case Action::ActionKind::CUSTOM_2:
  case Action::ActionKind::CUSTOM_3:
    return true;

  case Action::ActionKind::ACTIONID:
    return std::find(
      valid_action_ids_.begin(), valid_action_ids_.end(), action.ActionId()) != valid_action_ids_.end();
  }

  return false;
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
    case 'z': return Action(Action::ActionKind::QUERY_ALPHAZERO);
    default:
      break;
    }
  }

  // Moves can be given either as the full name or as a number.
  const uint64_t aid = StringToActionId(str);
  if (aid < TOTAL_ACTIONID_COUNT) {
    return Action(Action::ActionKind::ACTIONID, aid);
  }

  try {
    int num = std::stoi(str);
    if ((num >= 0) && (uint64_t(num) < TOTAL_ACTIONID_COUNT)) {
      return Action(Action::ActionKind::ACTIONID, uint64_t(num));
    }
  }
  catch (...) {
  }

  return Action(Action::ActionKind::INVALID);
}

std::string
Board::ActionToString(const Action& action) const
{
  if (action.Kind() != Action::ActionKind::ACTIONID) {
    return action.ToString();
  }

  return ActionIdToString(action.ActionId());
}

std::string
Board::ToString() const
{
  std::stringstream sstr;
  const size_t ch_width = (visible_2d_size_.ColumnCount() * 5) - 1;

  // Player key.
  for (size_t idx = 0; idx < PLAYER_COUNT; ++idx) {
    sstr << ActivePlayer()->Name() << " (p" << idx << ")";
    if (idx == active_player_id_) {
      sstr
        << ", absolute position [" << player_positions_[idx].ToString() << "]"
        << ", absolute orientation " << BiplaneOrientationToShortString(player_orientations_[idx])
        << ", relative position [0,0], relative orientation UP, ACTIVE";
    }
    else {
      MatrixCoordinate abs_pos = player_positions_[idx];
      BiplaneOrientation abs_orient = player_orientations_[idx];
      RelativePositionToAbsolute(abs_pos, abs_orient);
      sstr
        << ", absolute position [" << abs_pos.ToString() << "]"
        << ", absolute orientation " << BiplaneOrientationToShortString(abs_orient)
        << ", relative position [" << player_positions_[idx].ToString() << "]"
        << ", relative orientation " << BiplaneOrientationToShortRelativeString(player_orientations_[idx]);
    }
    sstr << "\n";
  }

  // Board.
  sstr << " " << std::string(ch_width, '-') << '\n';
  for (int64_t row_idx = max_distance_; row_idx >= -(int64_t(max_distance_)); --row_idx) {
    sstr << "|";
    for (int64_t col_idx = -(int64_t(max_distance_)); col_idx <= int64_t(max_distance_); ++col_idx) {
      sstr << (((row_idx == 0) && (col_idx == 0)) ? "-" : " ");
      bool found = false;
      for (size_t idx = 0; idx < PLAYER_COUNT; ++idx) {
        if ((idx != active_player_id_) &&
            (player_positions_[idx] == MatrixCoordinate(row_idx, col_idx))) {
          sstr << BiplaneOrientationToShortRelativeString(player_orientations_[idx]);
          found = true;
          break;
        }
      }

      if (!found) {
        sstr << "  ";
      }
      sstr << (((row_idx == 0) && (col_idx == 0)) ? "-" : " ") << "|";
    }
    sstr << '\n';
  }

  return sstr.str();
}

void
Board::UpdateValidActions()
{
  valid_action_ids_.resize(TotalActionIdCount());
  std::iota(valid_action_ids_.begin(), valid_action_ids_.end(), 0);

#ifndef NDEBUG
  {
    std::string dups;
    THROWIF(!ValidateUniqueActionIds(valid_action_ids_, &dups),
            "duplicate valid action IDs: " + dups);
  }
#endif
}

void
Board::UpdateTerminalStatus()
{
  // Check winning conditions and set 'terminal_status_'.

  for (size_t idx = 0; idx < PLAYER_COUNT; ++idx) {
    if (idx != active_player_id_) {
      size_t diagonal_count, straight_count;
      BiplaneOrientation diagonal_dir, straight_dir;
      DecomposeSteps(player_positions_[idx], diagonal_count, diagonal_dir, straight_count, straight_dir);

      // If a player has flown beyond max distance than game is a draw.
      if ((diagonal_count + straight_count) > max_distance_) {
        terminal_status_ =
          cifre::StateTerminalStatus(cifre::StateTerminalStatus::TerminalKind::DRAW);
        return;
      }

      // If there is an opponent directly in front of the active player,
      // then the active player is the winner.
      if ((diagonal_count == 0) && (straight_count != 0) && (straight_dir == BiplaneOrientation::NORTH)) {
        terminal_status_ =
          cifre::StateTerminalStatus(
            cifre::StateTerminalStatus::TerminalKind::WINNER, active_player_id_);
        return;
      }
    }
  }

  // Otherwise make sure the terminal status is NON_TERMINAL.
  THROWIF(
    terminal_status_.Kind() != cifre::StateTerminalStatus::TerminalKind::NON_TERMINAL,
    "expected NON_TERMINAL status");
}

void
Board::ApplyAction(uint64_t action_id)
{
  MatrixCoordinate pos_delta;
  int64_t orientation_delta;
  ActionIdToMove(action_id, pos_delta, orientation_delta);

  // Position delta must be rotated so that it is applied along the
  // forward direction of the active player.
  const MatrixCoordinate rot_pos_delta =
    RotatePosition(pos_delta, size_t(player_orientations_[active_player_id_]));

  player_positions_[active_player_id_] += rot_pos_delta;
  player_orientations_[active_player_id_] =
    BiplaneOrientation(
      (int64_t(player_orientations_[active_player_id_]) + orientation_delta +
       NUM_PLANE_ORIENTATIONS) % NUM_PLANE_ORIENTATIONS);
}

void
Board::RelativePositionToAbsolute(MatrixCoordinate& pos, BiplaneOrientation& orient) const
{
  // The clockwise rotation required for 'pos'ition and 'orient'ation
  // is the absolute orientation of the active player.
  const BiplaneOrientation rot = player_orientations_[active_player_id_];
  const MatrixCoordinate rel_rot = RotatePosition(pos, size_t(rot));
//  std::cerr
//    << "RelativePositionToAbsolute (center [" << player_positions_[active_player_id_].ToString() << "] "
//    << BiplaneOrientationToShortString(player_orientations_[active_player_id_]) << "): "
//    << "[" << pos.ToString() << "] " << BiplaneOrientationToShortRelativeString(orient)
//    << " -> rotated [" << rel_rot.ToString() << "]";
  orient = BiplaneOrientation((size_t(orient) + size_t(rot)) % NUM_PLANE_ORIENTATIONS);
  pos = player_positions_[active_player_id_] + rel_rot;
//  std::cerr << " -> [" << pos.ToString() << "] " << BiplaneOrientationToShortString(orient) << "\n";
}

void
Board::AbsolutePositionToRelative(MatrixCoordinate& pos, BiplaneOrientation& orient) const
{
  // The counter-clockwise rotation required for 'pos'ition and
  // 'orient'ation is the absolute orientation of the active player.
  const BiplaneOrientation rot =
    BiplaneOrientation(NUM_PLANE_ORIENTATIONS - size_t(player_orientations_[active_player_id_]));
  const MatrixCoordinate rel_rot = RotatePosition(pos - player_positions_[active_player_id_], size_t(rot));
//  std::cerr
//    << "AbsolutePositionToRelative (center [" << player_positions_[active_player_id_].ToString() << "] "
//    << BiplaneOrientationToShortString(player_orientations_[active_player_id_]) << "): "
//    << "[" << pos.ToString() << "] " << BiplaneOrientationToShortString(orient);
  orient = BiplaneOrientation((size_t(orient) + size_t(rot)) % NUM_PLANE_ORIENTATIONS);
  pos = rel_rot;
//  std::cerr << " -> [" << pos.ToString() << "] " << BiplaneOrientationToShortRelativeString(orient) << "\n";
}

namespace {

static const cifre::array<MatrixCoordinate, cifre::biplane::NUM_PLANE_ORIENTATIONS> unit_vectors_ {
  //              [y, x]
  MatrixCoordinate(1, 0),    // N
  MatrixCoordinate(1, 1),    // NE
  MatrixCoordinate(0, 1),    // E
  MatrixCoordinate(-1, 1),   // SE
  MatrixCoordinate(-1, 0),   // S
  MatrixCoordinate(-1, -1),  // SW
  MatrixCoordinate(0, -1),   // W
  MatrixCoordinate(1, -1)    // NW
};

} // namespace

void
Board::DecomposeSteps(
  const MatrixCoordinate& pos,
  size_t& diagonal_count, BiplaneOrientation& diagonal_dir,
  size_t& straight_count, BiplaneOrientation& straight_dir) const
{
  // 'pos' is assumed to be a relative position. Decompose 'pos' into
  // diagonal and straight steps. Maximize diagonal steps.
  diagonal_count = std::min(abs(pos.Row()), abs(pos.Column()));
  straight_count = std::max(abs(pos.Row()), abs(pos.Column())) - diagonal_count;

  // Determine the direction of the diagonal, rotate to the new
  // direction, and perform the same number of steps along the new
  // direction.
  diagonal_dir = BiplaneOrientation::NORTH;
  if (diagonal_count > 0) {
    assert((abs(pos.Row()) >= diagonal_count) && (abs(pos.Column()) >= diagonal_count));
    if (pos.Column() >= 0) {
      diagonal_dir = (pos.Row() >= 0) ? BiplaneOrientation::NORTH_EAST : BiplaneOrientation::SOUTH_EAST;
    } else {
      diagonal_dir = (pos.Row() >= 0) ? BiplaneOrientation::NORTH_WEST : BiplaneOrientation::SOUTH_WEST;
    }

    assert(diagonal_dir != BiplaneOrientation::NORTH);
  }

  straight_dir = BiplaneOrientation::NORTH_EAST;
  if (straight_count > 0) {
    assert(abs(pos.Column()) != abs(pos.Row()));
    if (abs(pos.Column()) > abs(pos.Row())) {
      straight_dir = (pos.Column() > 0) ? BiplaneOrientation::EAST : BiplaneOrientation::WEST;
    }
    else {
      straight_dir = (pos.Row() > 0) ? BiplaneOrientation::NORTH : BiplaneOrientation::SOUTH;
    }

    assert(straight_dir != BiplaneOrientation::NORTH_EAST);
  }
}

MatrixCoordinate
Board::RotatePosition(const MatrixCoordinate& pos, const size_t rot) const
{
  if (rot == 0) {
    return pos;
  }

  MatrixCoordinate rot_pos;

  size_t diagonal_count, straight_count;
  BiplaneOrientation diagonal_dir, straight_dir;
  DecomposeSteps(pos, diagonal_count, diagonal_dir, straight_count, straight_dir);

//  std::cerr << "RotatePosition: diagonal "
//            << "diagonal " << diagonal_count << " "
//            << BiplaneOrientationToShortString(diagonal_dir)
//            << ", straight " << straight_count << " "
//            << BiplaneOrientationToShortString(straight_dir) << "\n";

  // Perform steps along diagonal direction.
  if (diagonal_count > 0) {
    const size_t drot = (size_t(diagonal_dir) + rot + NUM_PLANE_ORIENTATIONS) % NUM_PLANE_ORIENTATIONS;
    rot_pos += unit_vectors_[drot] * int64_t(diagonal_count);
  }

  // Perform steps along straight direction.
  if (straight_count > 0) {
    const size_t drot = (size_t(straight_dir) + rot + NUM_PLANE_ORIENTATIONS) % NUM_PLANE_ORIENTATIONS;
    rot_pos += unit_vectors_[drot] * int64_t(straight_count);
  }

  return rot_pos;
}

//
// BiplaneOrientation
//
std::string
BiplaneOrientationToShortString(BiplaneOrientation po)
{
  switch (po) {
  case BiplaneOrientation::NORTH:      return "N";
  case BiplaneOrientation::NORTH_EAST: return "NE";
  case BiplaneOrientation::EAST:       return "E";
  case BiplaneOrientation::SOUTH_EAST: return "SE";
  case BiplaneOrientation::SOUTH:      return "S";
  case BiplaneOrientation::SOUTH_WEST: return "SW";
  case BiplaneOrientation::WEST:       return "W";
  case BiplaneOrientation::NORTH_WEST: return "NW";
  }

  // unreachable
  return "<unknown>";
}

std::string
BiplaneOrientationToShortRelativeString(BiplaneOrientation po)
{
  switch (po) {
  case BiplaneOrientation::NORTH:      return "UP";
  case BiplaneOrientation::NORTH_EAST: return "UR";
  case BiplaneOrientation::EAST:       return "RT";
  case BiplaneOrientation::SOUTH_EAST: return "DR";
  case BiplaneOrientation::SOUTH:      return "DN";
  case BiplaneOrientation::SOUTH_WEST: return "DL";
  case BiplaneOrientation::WEST:       return "LT";
  case BiplaneOrientation::NORTH_WEST: return "UL";
  }

  // unreachable
  return "<unknown>";
}

}} // namespace cifre::biplane
