// Copyright 2026, David Goodwin. All rights reserved.
#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>

#include "cifre_action.h"
#include "cifre_player.h"
#include "cifre_state.h"
#include "cifre_std.h"
#include "cifre_vec.h"
#include "biplane_action.h"

namespace cifre { namespace biplane {

static constexpr size_t PLAYER_COUNT = 2;
static constexpr size_t NUM_PLANES = 8;

static constexpr size_t MAX_DISTANCE = 3;
static constexpr size_t DEFAULT_MAX_DISTANCE = 3;

static constexpr float CELL_EMPTY = 0.0;
static constexpr float CELL_ACTIVE = 1.0;

// Plane orientations.
enum class PlaneOrientation {
  NORTH = 0,
  NORTH_EAST,
  EAST,
  SOUTH_EAST,
  SOUTH,
  SOUTH_WEST,
  WEST,
  NORTH_WEST
};
static constexpr size_t NUM_PLANE_ORIENTATIONS = 8;

// Return the string representation of an orientation.
std::string PlaneOrientationToShortString(PlaneOrientation po);

// Return the string representation for a relative
// orientation. Relative orientations are named up, down, etc. because
// they are relative to a north-oriented board.
std::string PlaneOrientationToShortRelativeString(PlaneOrientation po);

//
// Board
//
// The biplane playing board.
//
// A specialization of State for biplane games. The "board" is a
// limitless 2D grid. Each player has an signed integer position on
// the grid and an orientation (PlaneOrientation). Players position
// and orientation changes based on the move selected by each player.
//
// The model representation of the board uses NUM_PLANES planes of a
// grid of cells that places the opponent player relative to the
// active player (assumed to be in the center of the grid, e.g. at
// position [3, 3]) on the plane that represents the orientation of
// the opponent player relative to the active player (assumed to be
// facing "up" on the grid). Thus for any position and orientation of
// the players, the model representation for the active player is
// always the relative perspective assuming the active player is at
// center of grid facing "up" (toward negative y). The opponent
// position and orientation is represented by a CELL_ACTIVE in the
// appropriate plane and grid position. All other grid positions
// across all planes are CELL_EMPTY.

// Provides a console representation of the opponent relative to the
// active player.
//
// See cifre::State for information about memory management requires
// classes derived from State.
//
class Board : public cifre::State {
public:
  // Create a board based on a given 'max-distance' and 'players',
  // where player ID is the index of a given player in 'players'.
  // 'starting_player_id' indicates the player that will go first.
  Board(
    const size_t max_distance, const std::vector<Player*>& players,
    size_t starting_player_id, std::pmr::memory_resource* arena);

  // Destructor
  virtual ~Board() = default;

  // See cifre::State.
  size_t Hash() const override;

  // See cifre::State.
  bool Equal(const State* other) const override;

  // See cifre::State
  StateUniquePtr Copy(std::pmr::memory_resource* arena) const override;
  void Update(uint64_t action_id) override;
  size_t TotalActionIdCount() const override { return TOTAL_ACTIONID_COUNT; }

  // See cifre::State.
  Action StringToAction(const std::string& str) const override;

  // See cifre::State.
  std::string ActionToString(const Action& action) const override;

  // See cifre::State
  StateTerminalStatus TerminalStatus() const override { return terminal_status_; }

  // See cifre::State
  const std::pmr::vector<uint64_t>& ValidActionIds() const override {
    return valid_action_ids_; }

  // See cifre::State.
  bool IsValidAction(const Action& action) const override;

  // Get the representation of the state suitable for feeding to the
  // model via triton. Returns flattened row-major ordering of the
  // cell values. See cifre::State.
  const FlattenedCells& TritonRepresentation() const override {
    return cells_flattened_;
  }

  // See cifre::State.
  std::string ToString() const override;

  // Return the player ID that represents "no player". By convention
  // this ID is equal to the number of players in the game.
  size_t NoPlayerId() const { return players_.size(); }

  // Return the ID of the active player. See cifre::State.
  size_t ActivePlayerId() const override { return active_player_id_; }

  // Return the active player.
  Player* ActivePlayer() const { return players_[active_player_id_]; }

private:
  // Don't allow default copy constructor or assignment to force the
  // version above that requires an arena. Don't allow move
  // assignment as it does not move the allocator for any pmr
  // containers (move constructor is ok).
  Board(Board&) = delete;
  Board& operator=(Board&) = delete;
  Board(const Board&) = delete;
  Board& operator=(const Board&) = delete;
  Board& operator=(Board&&) = delete;

  // Construct board as a copy of 'other' using 'arena' for memory
  // required by constructed board. This constructor is kept private
  // so that Copy() must be used, see cifre::State.
  Board(const Board& other, std::pmr::memory_resource* arena);

  void UpdateTerminalStatus();
  void UpdateValidActions();

  void ApplyAction(uint64_t action_id);
  void PositionToAbsolute(MatrixCoordinate& pos, PlaneOrientation& orient) const;
  void PositionToRelative(MatrixCoordinate& pos, PlaneOrientation& orient) const;
  void DecomposeSteps(
    const MatrixCoordinate& pos,
    size_t& diagonal_count, PlaneOrientation& diagonal_dir,
    size_t& straight_count, PlaneOrientation& straight_dir) const;
  MatrixCoordinate RotatePosition(const MatrixCoordinate& pos, const size_t rot) const;

  // Maximum distance between planes.
  const size_t max_distance_;

  // The size of the board that is visible to the active player
  // assumed to be in the center of this visible portion of the board.
  const MatrixSize visible_2d_size_;

  // The players. The player's index in this vector is the player ID.
  cifre::array<cifre::Player*, PLAYER_COUNT> players_;

  // For each player the position and orientation of the player's
  // plane. For the active player, position and orientation are actual
  // world values. For all other players position and orientation are
  // relative to the active player.
  cifre::array<MatrixCoordinate, PLAYER_COUNT> player_positions_;
  cifre::array<PlaneOrientation, PLAYER_COUNT> player_orientations_;

  // ID of the currently active player.
  size_t active_player_id_;

  // The status of the board indicating winning condition.
  cifre::StateTerminalStatus terminal_status_;

  // The set of action ids that are valid for the current board.
  std::pmr::vector<uint64_t> valid_action_ids_;

  // 'cells_' is simply a type-casted pointer into 'cells_flattened_'.
  // Size of the board.
  FlattenedCells cells_flattened_;
  float* cells_;
};

}} // namespace cifre::biplane
