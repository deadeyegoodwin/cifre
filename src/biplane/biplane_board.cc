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
  max_distance_(max_distance), active_player_id_(starting_player_id),
  terminal_status_(cifre::StateTerminalStatus::TerminalKind::NON_TERMINAL),
  valid_action_ids_(arena),
  cells_2d_size_((max_distance * 2) + 1, (max_distance * 2) + 1),
  cells_flattened_(arena)
{
  THROWIF((max_distance_ <= 0) || ((max_distance_ % 2) == 0),
          "max-distance must be positive odd integer");
  THROWIF(max_distance_ > MAX_DISTANCE,
          "max-distance must be <= " + std::to_string(MAX_DISTANCE));

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
      player_orientations_[idx] = PlaneOrientation::NORTH_EAST;
    }
    else {
      player_positions_[idx] = MatrixCoordinate(max_distance_, max_distance_);
      player_orientations_[idx] = PlaneOrientation::WEST;
    }
  }

#ifndef NDEBUG
  for (size_t pidx = 0; pidx < players_.size(); ++pidx) {
    assert(players_[pidx]->Id() == pidx);
  }
#endif

  cells_flattened_.resize(NUM_PLANES * cells_2d_size_.ElementCount() * sizeof(float));
  cells_ = reinterpret_cast<float*>(cells_flattened_.data());

  // Initialize 'valid_action_ids_' and 'terminal_status_'.
  valid_action_ids_.reserve(TotalActionIdCount());
  UpdateValidActions();
  UpdateTerminalStatus();
}

Board::Board(const Board& other, std::pmr::memory_resource* arena) :
  max_distance_(other.max_distance_), players_(other.players_),
  player_positions_(other.player_positions_), player_orientations_(other.player_orientations_),
  active_player_id_(other.active_player_id_), terminal_status_(other.terminal_status_),
  valid_action_ids_(other.valid_action_ids_.begin(), other.valid_action_ids_.end(), arena),
  cells_2d_size_(other.cells_2d_size_), cells_flattened_(other.cells_flattened_, arena)
{
  cells_ = reinterpret_cast<float*>(cells_flattened_.data());
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
  return StateUniquePtr(nb, StateDeleter<Board>(arena));
}

void
Board::Update(uint64_t action_id)
{
  // Move the active player as indicated by 'action_id'. 1) convert
  // non-active players to absolute coordinates, 2) move active
  // player, 3) determine the new active player, 4) convert non-active
  // players to relative coordinates.
  for (size_t idx = 0; idx < PLAYER_COUNT; ++idx) {
    if (idx != active_player_id_) {
      PlayerToAbsolute(idx);
    }
  }

  ApplyAction(action_id);

  SwitchActivePlayer();

  for (size_t idx = 0; idx < PLAYER_COUNT; ++idx) {
    if (idx != active_player_id_) {
      PlayerToRelative(idx);
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

  // FIXME

  return Action(Action::ActionKind::INVALID);
}

std::string
Board::ActionToString(const Action& action) const
{
  return "FIXME";
}

std::string
Board::ToString() const
{
  return std::string(); // FIXME
}

void
Board::UpdateValidActions()
{
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
Board::SwitchActivePlayer()
{

  // FIXME

  // Update the valid actions for the now-active player.
  active_player_id_ = (active_player_id_ + 1) % players_.size();
  UpdateValidActions();
}

void
Board::UpdateTerminalStatus()
{
  // Check winning conditions and set 'terminal_status_'.

  // FIXME

  // Otherwise make sure the terminal status is NON_TERMINAL.
  THROWIF(
    terminal_status_.Kind() != cifre::StateTerminalStatus::TerminalKind::NON_TERMINAL,
    "expected NON_TERMINAL status");
}

namespace {

static const cifre::array<MatrixCoordinate, cifre::biplane::NUM_PLANE_ORIENTATIONS> unit_vectors_ {
  MatrixCoordinate(0, 1),    // N
  MatrixCoordinate(1, 1),    // NE
  MatrixCoordinate(1, 0),    // E
  MatrixCoordinate(1, -1),   // SE
  MatrixCoordinate(0, -1),   // S
  MatrixCoordinate(-1, -1),  // SW
  MatrixCoordinate(-1, 0),   // W
  MatrixCoordinate(-1, 1)    // NW
};

} // namespace

void
Board::PlayerToAbsolute(size_t idx)
{
  assert(idx != active_player_id_);

  // The clockwise rotation required for 'idx' (position and
  // orientation) is the absolute orientation of the active player.
  const PlaneOrientation rot = player_orientations_[active_player_id_];
  const MatrixCoordinate rel_rot = RotatePosition(player_positions_[idx], size_t(rot));
  player_orientations_[idx] =
    PlaneOrientation((size_t(player_orientations_[idx]) + size_t(rot)) % NUM_PLANE_ORIENTATIONS);
  player_positions_[idx] = player_positions_[active_player_id_] + rel_rot;
}

void
Board::PlayerToRelative(size_t idx)
{
  assert(idx != active_player_id_);

  // The counter-clockwise rotation required for 'idx' (position and
  // orientation) is the absolute orientation of the active player.
  const PlaneOrientation rot =
    PlaneOrientation(NUM_PLANE_ORIENTATIONS - size_t(player_orientations_[idx]));
  const MatrixCoordinate rel_rot = RotatePosition(player_positions_[idx], size_t(rot));
  player_orientations_[idx] =
    PlaneOrientation((size_t(player_orientations_[idx]) - size_t(rot) +
                      NUM_PLANE_ORIENTATIONS) % NUM_PLANE_ORIENTATIONS);
  player_positions_[idx] = player_positions_[active_player_id_] + rel_rot;
}

MatrixCoordinate
Board::RotatePosition(const MatrixCoordinate& pos, const size_t rot)
{
  if (rot == 0) {
    return pos;
  }

  MatrixCoordinate rot_pos;

  // Decompose 'pos' into diagonal and straight steps. Maximize
  // diagonal steps.
  const int step_x = std::clamp(pos.Row(), int64_t(-1), int64_t(1));
  const int step_y = std::clamp(pos.Column(), int64_t(-1), int64_t(1));

  const size_t diagonal_count = std::min(abs(pos.Row()), abs(pos.Column()));
  const size_t straight_count = std::max(abs(pos.Row()), abs(pos.Column())) - diagonal_count;

  // Determine the direction of the diagonal, rotate to the new
  // direction, and perform the same number of steps along the new
  // direction.
  if (diagonal_count > 0) {
    assert((step_x != 0) && (step_y != 0));
    PlaneOrientation diagonal_dir = PlaneOrientation::NORTH;
    if (step_x >= 0) {
      diagonal_dir = (step_y >= 0) ? PlaneOrientation::NORTH_EAST : PlaneOrientation::SOUTH_EAST;
    } else {
      diagonal_dir = (step_y >= 0) ? PlaneOrientation::NORTH_WEST : PlaneOrientation::SOUTH_WEST;
    }

    const size_t drot = (size_t(diagonal_dir) + rot + NUM_PLANE_ORIENTATIONS) % NUM_PLANE_ORIENTATIONS;
    rot_pos += unit_vectors_[drot] * int64_t(diagonal_count);
  }

  if (straight_count > 0) {
    assert((step_x == 0) != (step_y == 0));
    PlaneOrientation straight_dir = PlaneOrientation::EAST;
    if (step_x == 0) {
      straight_dir = (step_y > 0) ? PlaneOrientation::NORTH : PlaneOrientation::SOUTH;
    }
    else if (step_x < 0) {
      straight_dir = PlaneOrientation::WEST;
    }

    const size_t drot = (size_t(straight_dir) + rot + NUM_PLANE_ORIENTATIONS) % NUM_PLANE_ORIENTATIONS;
    rot_pos += unit_vectors_[drot] * int64_t(straight_count);
  }

  return rot_pos;
}

}} // namespace cifre::biplane
