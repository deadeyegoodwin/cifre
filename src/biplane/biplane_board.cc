// Copyright 2026, David Goodwin. All rights reserved.

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>

#include "biplane_board.h"
#include "common_error.h"
#include "common_random.h"
#include "common_util.h"

namespace cifre { namespace biplane {

//
// Board
//
Board::Board(
  const std::shared_ptr<Logger>& logger,
  const size_t max_distance, const std::vector<Player*>& players,
  size_t starting_player_id, BiplaneScenario scenario,
  std::pmr::memory_resource* arena) :
  logger_(logger), max_distance_(max_distance),
  visible_2d_size_((max_distance * 2) + 1, (max_distance * 2) + 1),
  active_player_id_(starting_player_id),
  terminal_status_(cifre::StateTerminalStatus::TerminalKind::NON_TERMINAL),
  valid_action_ids_(arena), cells_flattened_(arena)
{
  THROWIF(max_distance_ < MIN_MAX_DISTANCE,
          "max-distance must be >= " + std::to_string(MIN_MAX_DISTANCE));
  THROWIF(max_distance_ > MAX_MAX_DISTANCE,
          "max-distance must be <= " + std::to_string(MAX_MAX_DISTANCE));

  THROWIF(players.size() != PLAYER_COUNT, "exactly 2 players are required");
  THROWIF(NoPlayerId() != PLAYER_COUNT, "exactly 2 players are required");
  THROWIF(active_player_id_ >= NoPlayerId(),
          "invalid starting player index " + std::to_string(active_player_id_));

  for (size_t idx = 0; idx < PLAYER_COUNT; ++idx) {
    players_[idx] = players[idx];
  }

  // Set players initial absolute positions, orientations, etc. based
  // on the chosen 'scenario'.
  while (true) {
    InitializeScenario(scenario);

    // Update terminal status for the initial position using absolute
    // positions/orientations. Use the initial position/orientation as
    // the previous position/orientation as well.
    terminal_status_ = cifre::StateTerminalStatus::TerminalKind::NON_TERMINAL;
    UpdateTerminalStatus(player_positions_, player_orientations_);
    if ((scenario != BiplaneScenario::RANDOM) ||
        (terminal_status_.Kind() == cifre::StateTerminalStatus::TerminalKind::NON_TERMINAL)) {
      break;
    }
  }

  THROWIF(terminal_status_.Kind() != cifre::StateTerminalStatus::TerminalKind::NON_TERMINAL,
          "scenario initial state is a game ending position");

  // Convert non-active players to relative...
  for (size_t idx = 0; idx < PLAYER_COUNT; ++idx) {
    if (idx != active_player_id_) {
      AbsolutePositionToRelative(
        player_positions_[active_player_id_], player_orientations_[active_player_id_],
        player_positions_[idx], player_orientations_[idx]);
    }
  }

#ifndef NDEBUG
  for (size_t pidx = 0; pidx < players_.size(); ++pidx) {
    assert(players_[pidx]->Id() == pidx);
  }
#endif

  // Initialize cell planes based on players initial state.
  cells_flattened_.resize(CELL_PLANE_COUNT * visible_2d_size_.ElementCount() * sizeof(float));
  cells_ = reinterpret_cast<float*>(cells_flattened_.data());
  UpdateCellPlanes();

  // Initialize 'valid_action_ids_' based on the initial state.
  valid_action_ids_.reserve(TotalActionIdCount());
  UpdateValidActions();

  LOG_DEBUG(*logger_, "==== Initial Board ====\n" + ToString() + "\n========");
  LOG_DEBUG(*logger_, "==== Initial Model Input ====\n" + CellsToString() + "\n========");
}

Board::Board(const Board& other, std::pmr::memory_resource* arena) :
  logger_(other.logger_), max_distance_(other.max_distance_),
  visible_2d_size_(other.visible_2d_size_), players_(other.players_),
  player_positions_(other.player_positions_), player_orientations_(other.player_orientations_),
  player_advantages_(other.player_advantages_), player_actions_(other.player_actions_),
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

  // The active player's position and orientation are not included in
  // the hash because it is always considered to be the
  // "center". Other player's relative position and orientation are
  // included in the hash.
  //
  // FIXME the order that we visit players should not matter here so
  // once we support more than 2 players we should sort based on
  // relative position.
  for (size_t idx = 0; idx < PLAYER_COUNT; ++idx) {
    if (idx != active_player_id_) {
      hash += MatrixCoordinateHash()(player_positions_[idx]) ^ (hash >> 23);
      hash += (size_t(player_orientations_[idx]) * mulp) ^ (hash >> 23);
    }

    hash += (size_t(player_advantages_[idx]) * mulp) ^ (hash >> 23);
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

  // The active player's position and orientation are not included in
  // the hash because it is always considered to be the
  // "center". Other player's relative position and orientation are
  // included in the equality.
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
    if (player_advantages_[idx] != ob->player_advantages_[idx]) {
      return false;
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
  //  by Update() and so copying them would be wasteful as they will
  //  be overwritten.
  nb->cells_flattened_.resize(CELL_PLANE_COUNT * visible_2d_size_.ElementCount() * sizeof(float));
  nb->cells_ = reinterpret_cast<float*>(nb->cells_flattened_.data());

  nb->Update(action_id);

  return StateUniquePtr(nb, StateDeleter<Board>(arena));
}

void
Board::Update(uint64_t action_id)
{
  // The position of players on the board only changes once all
  // players have selected an action and then all those actions are
  // applied simultaneously.
  player_actions_.push_back(std::make_pair(active_player_id_, action_id));

  // Unset the cells on the direction cell planes representing where
  // each opponent biplane is currently at relative to the active
  // player. This should result in all cells of the direction planes
  // being CELL_EMPTY. For performance we do it explicilty instead of
  // just zeroing the entire array.
  for (size_t idx = 0; idx < PLAYER_COUNT; ++idx) {
    if (idx != active_player_id_) {
      const MatrixCoordinate& pos = player_positions_[idx];
      assert((abs(pos.Row()) <= max_distance_) && (abs(pos.Column()) <= max_distance_));
      SetCellValueAt(
        CELL_EMPTY, pos + max_distance_, size_t(player_orientations_[idx]));
    }
  }
  assert(ValidateCellPlanes(0, CELL_PLANE_DIR_COUNT, CELL_EMPTY));

  // Update the board for the new active player.
  //
  // 1) convert non-active players to absolute coordinates, 2) if all
  // players have selected an action then move all the players
  // simultaneously, 3) determine the new active player, 4) convert
  // non-active players to relative coordinates.
  for (size_t idx = 0; idx < PLAYER_COUNT; ++idx) {
    if (idx != active_player_id_) {
      RelativePositionToAbsolute(
        player_positions_[active_player_id_], player_orientations_[active_player_id_],
        player_positions_[idx], player_orientations_[idx]);
    }
  }

  if (player_actions_.size() == PLAYER_COUNT) {
    const auto player_prev_positions = player_positions_;
    const auto player_prev_orientations = player_orientations_;

    for (size_t idx = 0; idx < PLAYER_COUNT; ++idx) {
      const auto& pr = player_actions_[idx];
      ApplyActionToAbsolutePlayer(pr.first, pr.second);
    }

#ifndef NDEBUG
    LOG_DEBUG(*logger_, "-- Post Move --");
    for (size_t idx = 0; idx < PLAYER_COUNT; ++idx) {
      const auto& pr = player_actions_[idx];
      const size_t pidx = pr.first;
      const uint64_t action_id = pr.second;
      LOG_DEBUG(
        *logger_,
        players_[pidx]->Name() + " (p" + std::to_string(pidx) + "), " +
        "moved " + ActionIdToString(action_id) + ", " +
        "abs position [" + player_prev_positions[pidx].ToString() + "] " +
        BiplaneOrientationToShortString(player_prev_orientations[pidx]) + " -> " +
        "[" + player_positions_[pidx].ToString() + "] " +
        BiplaneOrientationToShortString(player_orientations_[pidx]));
    }
#endif

    // Board state changed so update terminal status using absolute
    // positions.
    UpdateTerminalStatus(player_prev_positions, player_prev_orientations);

    player_actions_.clear();
  }

  // For now we just make the next player the active player. Update
  // the valid actions for the now-active player.
  active_player_id_ = (active_player_id_ + 1) % players_.size();
  UpdateValidActions();

  for (size_t idx = 0; idx < PLAYER_COUNT; ++idx) {
    if (idx != active_player_id_) {
      AbsolutePositionToRelative(
        player_positions_[active_player_id_], player_orientations_[active_player_id_],
        player_positions_[idx], player_orientations_[idx]);
    }
  }

  // Set cell planes based on updated state.
  UpdateCellPlanes();

  LOG_DEBUG(*logger_, "==== Next Player Model Input ====\n" + CellsToString() + "\n========");
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

  // Board.
  sstr << " " << std::string(ch_width, '-');
  for (int64_t row_idx = max_distance_; row_idx >= -(int64_t(max_distance_)); --row_idx) {
    sstr << "\n|";
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
  }
  sstr << "\n " << std::string(ch_width, '-');

  if (player_advantages_[active_player_id_] != NoPlayerId()) {
    sstr
      << "\n" << players_[active_player_id_]->Name() << " (p" << active_player_id_ << ")"
      << " has advantage on " << players_[player_advantages_[active_player_id_]]->Name()
      << " (p" << player_advantages_[active_player_id_] << ")";
  }

  return sstr.str();
}

std::string
Board::CellsToString() const
{
  std::stringstream sstr;

  // 'plane_id_chars' is the number of characters in a cell dedicated
  // to showing plane IDs. So a value of 3 indicates 2 plane IDs plus
  // comma (e.g. "1,8").
  const size_t plane_id_chars = 3;
  const size_t ch_width = (visible_2d_size_.ColumnCount() * (plane_id_chars + 3)) - 1;

  sstr << " " << std::string(ch_width, '-');
  for (size_t y = visible_2d_size_.RowCount(); y > 0; --y) {
    sstr << "\n|";
    for (size_t x = 0; x < visible_2d_size_.ColumnCount(); ++x) {
      sstr << ((((y - 1) == max_distance_) && (x == max_distance_)) ? "-" : " ");

      const MatrixCoordinate pos(y - 1, x);

      // Collect the direction plane IDs for [y,x].
      std::string cstr;
      for (size_t idx = 0; idx < CELL_PLANE_DIR_COUNT; ++idx) {
        const float v = CellValueAt(pos, idx);
        assert((v == CELL_ACTIVE) || (v == CELL_EMPTY));
        if (v == CELL_ACTIVE) {
          if (!cstr.empty()) {
            cstr += ",";
          }
          cstr += std::to_string(idx);
        }
      }

      if (cstr.size() > plane_id_chars) {
        cstr = std::string(plane_id_chars, '*');
      } else if (cstr.size() < plane_id_chars) {
        cstr += std::string(plane_id_chars - cstr.size(), ' ');
      }

      sstr << cstr;
      sstr << ((((y - 1) == max_distance_) && (x == max_distance_)) ? "-" : " ") << "|";
    }
  }
  sstr << "\n " << std::string(ch_width, '-');

  sstr
    << "\nAdvantage plane: "
    << ((CellValueAt(MatrixCoordinate(0, 0), CELL_PLANE_ADV_IDX) == CELL_ACTIVE) ? "ACTIVE" : "EMPTY");

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
Board::UpdateTerminalStatus(
  const cifre::array<MatrixCoordinate, PLAYER_COUNT>& player_prev_positions,
  const cifre::array<BiplaneOrientation, PLAYER_COUNT>& player_prev_orientations)
{
  // Check winning conditions and set 'terminal_status_'. Player
  // positions and orientations are assumed to be absolute
  // ('player_positions_' and 'player_orientations_)' and
  // 'player_prev_orientations' and 'player_prev_orientations' must
  // also be absolute.

  // Compare pairs of players...
  for (size_t idx0 = 0; idx0 < (PLAYER_COUNT - 1); ++idx0) {
    for (size_t idx1 = (idx0 + 1); idx1 < PLAYER_COUNT; ++idx1) {
      // Determine relative distance from 'idx0' to 'idx1'.
      MatrixCoordinate rpos1 = player_positions_[idx1];
      BiplaneOrientation rorient1 = player_orientations_[idx1];
      AbsolutePositionToRelative(
        player_positions_[idx0], player_orientations_[idx0], rpos1, rorient1);

      size_t diagonal_count, straight_count;
      BiplaneOrientation diagonal_dir, straight_dir;
      DecomposeSteps(rpos1, diagonal_count, diagonal_dir, straight_count, straight_dir);

      // If relative distance exceeds max distance then the planes are
      // out-of-range and the game is over. Use the previous position
      // of the planes to determine which player caused the planes to
      // move out-of-range. That player is the loser.
      if ((diagonal_count + straight_count) > max_distance_) {
        bool blame_idx0 = false, blame_idx1 = false;

        // Use 'idx0' and then 'idx1' previous positions to see if
        // either of those result in the planes being in range.
        MatrixCoordinate rpos1_prev0 = player_positions_[idx1];
        BiplaneOrientation rorient1_prev0 = player_orientations_[idx1];
        AbsolutePositionToRelative(
          player_prev_positions[idx0], player_prev_orientations[idx0],
          rpos1_prev0, rorient1_prev0);
        DecomposeSteps(rpos1_prev0, diagonal_count, diagonal_dir, straight_count, straight_dir);
        blame_idx1 = ((diagonal_count + straight_count) > max_distance_);

        MatrixCoordinate rpos0_prev1 = player_positions_[idx0];
        BiplaneOrientation rorient0_prev1 = player_orientations_[idx0];
        AbsolutePositionToRelative(
          player_prev_positions[idx1], player_prev_orientations[idx1],
          rpos0_prev1, rorient0_prev1);
        DecomposeSteps(rpos0_prev1, diagonal_count, diagonal_dir, straight_count, straight_dir);
        blame_idx0 = ((diagonal_count + straight_count) > max_distance_);

        if (blame_idx0 && !blame_idx1) {
          terminal_status_ =
            cifre::StateTerminalStatus(
              cifre::StateTerminalStatus::TerminalKind::WINNER, idx1);
        }
        else if (blame_idx1 && !blame_idx0) {
          terminal_status_ =
            cifre::StateTerminalStatus(
              cifre::StateTerminalStatus::TerminalKind::WINNER, idx0);
        }
        else {
          // Both players are responsible for flying out of range, if
          // one player has an advantage then the player they have the advantage on is the winner.
          if ((player_advantages_[idx0] == idx1) && (player_advantages_[idx1] != idx0)) {
            terminal_status_ =
              cifre::StateTerminalStatus(
                cifre::StateTerminalStatus::TerminalKind::WINNER, idx1);
          }
          else if ((player_advantages_[idx1] == idx0) && (player_advantages_[idx0] != idx1)) {
            terminal_status_ =
              cifre::StateTerminalStatus(
                cifre::StateTerminalStatus::TerminalKind::WINNER, idx0);
          }
          else {
            terminal_status_ =
              cifre::StateTerminalStatus(cifre::StateTerminalStatus::TerminalKind::DRAW);
          }
        }

        return;
      }
    }
  }

  // Planes are still in playing area, check for hits.
  cifre::array<size_t, PLAYER_COUNT> hits;
  std::fill(hits.begin(), hits.end(), PLAYER_COUNT);

  for (size_t idx0 = 0; idx0 < PLAYER_COUNT; ++idx0) {
    for (size_t idx1 = 0; idx1 < PLAYER_COUNT; ++idx1) {
      if (idx0 == idx1) {
        continue;
      }

      MatrixCoordinate rpos = player_positions_[idx1];
      BiplaneOrientation rorient = player_orientations_[idx1];
      AbsolutePositionToRelative(
        player_positions_[idx0], player_orientations_[idx0], rpos, rorient);

      size_t diagonal_count, straight_count;
      BiplaneOrientation diagonal_dir, straight_dir;
      DecomposeSteps(rpos, diagonal_count, diagonal_dir, straight_count, straight_dir);

      // Record if 'idx1' is getting hit by 'idx0'.
      if ((diagonal_count == 0) && (straight_count != 0) && (straight_dir == BiplaneOrientation::NORTH)) {
        hits[idx0] = idx1;
      }
    }
  }

  // A single plane having a hit is a win for that plane, multiple
  // planes having a hit is a draw.
  size_t single_hitter = PLAYER_COUNT;
  for (size_t hitter = 0; hitter < PLAYER_COUNT; ++hitter) {
    if (hits[hitter] != PLAYER_COUNT) {
      if ((single_hitter != PLAYER_COUNT) && (single_hitter != hitter)) {
        terminal_status_ =
          cifre::StateTerminalStatus(cifre::StateTerminalStatus::TerminalKind::DRAW);
        return;
      }

      single_hitter = hitter;
    }
  }

  if (single_hitter < PLAYER_COUNT) {
    terminal_status_ =
      cifre::StateTerminalStatus(
        cifre::StateTerminalStatus::TerminalKind::WINNER, single_hitter);
    return;
  }

  // Otherwise make sure the terminal status is NON_TERMINAL.
  THROWIF(
    terminal_status_.Kind() != cifre::StateTerminalStatus::TerminalKind::NON_TERMINAL,
    "expected NON_TERMINAL status, got " + terminal_status_.ToString());
}

void
Board::UpdateCellPlanes()
{
  // Set the cells representing the gameboard to show where each
  // opponent biplane is relative to the active player.
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

  // If the active player has an advantage then set the advantage cell
  // plane to CELL_ACTIVE, otherwise CELL_EMPTY.
  const float adv = (
    (player_advantages_[active_player_id_] == NoPlayerId()) ? CELL_EMPTY : CELL_ACTIVE);
  if (adv != CellValueAt(MatrixCoordinate(0, 0), CELL_PLANE_ADV_IDX)) {
    FillCellPlane(CELL_PLANE_ADV_IDX, adv);
  }
  ValidateCellPlane(CELL_PLANE_ADV_IDX, adv);
}

void
Board::ApplyActionToAbsolutePlayer(const size_t player_id, const uint64_t action_id)
{
  // NOTE: player positions and orientations must be absolute.
  assert(player_id < PLAYER_COUNT);
  assert(action_id < TOTAL_ACTIONID_COUNT);

  MatrixCoordinate pos_delta;
  int64_t orientation_delta;
  ActionIdToMove(action_id, pos_delta, orientation_delta);

  // Position delta must be rotated so that it is applied along the
  // forward direction of 'player_id'.
  const MatrixCoordinate rot_pos_delta =
    RotatePosition(pos_delta, size_t(player_orientations_[player_id]));

  player_positions_[player_id] += rot_pos_delta;
  player_orientations_[player_id] =
    BiplaneOrientation(
      (int64_t(player_orientations_[player_id]) + orientation_delta +
       NUM_PLANE_ORIENTATIONS) % NUM_PLANE_ORIENTATIONS);
}

void
Board::RelativePositionToAbsolute(
  const MatrixCoordinate& center_pos, const BiplaneOrientation& center_orient,
  MatrixCoordinate& pos, BiplaneOrientation& orient) const
{
  // The clockwise rotation required for 'pos'ition and 'orient'ation
  // is the absolute orientation of 'center_orient'.
  const BiplaneOrientation rot = center_orient;
  const MatrixCoordinate rel_rot = RotatePosition(pos, size_t(rot));
  orient = BiplaneOrientation((size_t(orient) + size_t(rot)) % NUM_PLANE_ORIENTATIONS);
  pos = center_pos + rel_rot;
}

void
Board::AbsolutePositionToRelative(
  const MatrixCoordinate& center_pos, const BiplaneOrientation& center_orient,
  MatrixCoordinate& pos, BiplaneOrientation& orient) const
{
  // The counter-clockwise rotation required for 'pos'ition and
  // 'orient'ation is the absolute orientation of 'center_orient'.
  const BiplaneOrientation rot =
    BiplaneOrientation(NUM_PLANE_ORIENTATIONS - size_t(center_orient));
  const MatrixCoordinate rel_rot = RotatePosition(pos - center_pos, size_t(rot));
  orient = BiplaneOrientation((size_t(orient) + size_t(rot)) % NUM_PLANE_ORIENTATIONS);
  pos = rel_rot;
}

void
Board::FillCellPlane(const size_t pidx, const float v)
{
  for (size_t y = 0; y < visible_2d_size_.RowCount(); ++y) {
    for (size_t x = 0; x < visible_2d_size_.ColumnCount(); ++x) {
      SetCellValueAt(v, MatrixCoordinate(y, x), pidx);
    }
  }
}

bool
Board::ValidateCellPlanes(const size_t sidx, const size_t eidx, const float v) const
{
  for (size_t idx = sidx; idx < eidx; ++idx) {
    if (!ValidateCellPlane(idx, v)) {
      return false;
    }
  }

  return true;
}

bool
Board::ValidateCellPlane(const size_t pidx, const float v) const
{
  for (size_t y = 0; y < visible_2d_size_.RowCount(); ++y) {
    for (size_t x = 0; x < visible_2d_size_.ColumnCount(); ++x) {
      if (CellValueAt(MatrixCoordinate(y, x), pidx) != v) {
        return false;
      }
    }
  }

  return true;
}

std::string
BiplaneScenarioToString(BiplaneScenario scenario)
{
  switch (scenario) {
  case BiplaneScenario::RANDOM: return "RANDOM";
  case BiplaneScenario::EQUAL_1V1: return "EQUAL_1V1";
  }

  return "<unknown>";
}

BiplaneScenario
StringToBiplaneScenario(const std::string& str)
{
  std::string lstr(str);
  common::ToUpper(lstr);
  for (size_t idx = 0; idx < NUM_BIPLANE_SCENARIOS; ++idx) {
    if (lstr == BiplaneScenarioToString(BiplaneScenario(idx))) {
      return BiplaneScenario(idx);
    }
  }

  return BIPLANE_SCENARIO_UNKNOWN;
}

void
Board::InitializeScenario(BiplaneScenario scenario)
{
  static common::Random rand;

  switch (scenario) {
    // Position and orientation random for each player. For now never
    // set any advantage. To do this we put the first player at [0,0]
    // NORTH absolute and then randomly place the other players
    // around.
    case BiplaneScenario::RANDOM: {
      player_positions_[0] = MatrixCoordinate(0, 0);
      player_orientations_[0] = BiplaneOrientation::NORTH;
      player_advantages_[0] = NoPlayerId();

      for (size_t idx = 1; idx < PLAYER_COUNT; ++idx) {
        player_positions_[idx] =
          MatrixCoordinate(rand(size_t(0), visible_2d_size_.RowCount() - 1) - max_distance_,
                           rand(size_t(0), visible_2d_size_.ColumnCount() - 1) - max_distance_);
        player_orientations_[idx] = BiplaneOrientation(rand(size_t(0), NUM_PLANE_ORIENTATIONS - 1));
        player_advantages_[idx] = NoPlayerId();
      }
    }
    break;

    // 1v1 planes starting in neutral position relative to each other.
    case BiplaneScenario::EQUAL_1V1: {
      for (size_t idx = 0; idx < PLAYER_COUNT; ++idx) {
        if (idx == active_player_id_) {
          player_positions_[idx] = MatrixCoordinate(0, 0);
          player_orientations_[idx] = BiplaneOrientation::NORTH;
        }
        else {
          player_positions_[idx] = MatrixCoordinate(max_distance_ - 1, max_distance_ - 1);
          player_orientations_[idx] = BiplaneOrientation::WEST;
        }
        player_advantages_[idx] = NoPlayerId();
      }
    }
    break;
  }
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
