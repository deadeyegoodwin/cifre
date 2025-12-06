// Copyright 2025, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "cifre_action.h"
#include "cifre_player.h"
#include "cifre_state.h"
#include "common_util.h"

namespace cifre {

//
// BoardSize2D
//
// Represents the size of a 2D board, in rows and columns.
//
class BoardSize2D {
public:
  // A square board size with 'cnt' rows and columns.
  BoardSize2D(size_t cnt) : BoardSize2D(cnt, cnt) {}

  // A rectangular board size with 'rcnt' rows and 'ccnt' columns.
  BoardSize2D(size_t rcnt, size_t ccnt) : row_cnt_(rcnt), column_cnt_(ccnt) {}

  // Create a BoardSize2D by parsing a string with the format
  // "<rows>x<columns>", for example "4x3"; or format
  // "<rows>,<columns>", for example "4,3".
  BoardSize2D(const std::string& str);

  // Return the number of rows in the board size.
  size_t RowCount() const { return row_cnt_; }

  // Return the number of columns in the board size.
  size_t ColumnCount() const { return column_cnt_; }

  // Return the number of cells (rows * cols) in the board size.
  size_t CellCount() const { return row_cnt_ * column_cnt_; }

  // Return the board size as a std::pair { row-count, column-count }.
  std::pair<size_t, size_t> Size() const { return { row_cnt_, column_cnt_ }; }

  // Return string representation of the board size.
  std::string ToString() const;

private:
  size_t row_cnt_;
  size_t column_cnt_;
};

//
// Board2D
//
// A 2D game board with 1 or more players.
//
class Board2D : public State {
public:
  // Create a board based on a given 'size' and 'players', where
  // player ID is the index of a given player in 'players'.
  // 'starting_player_id' indicates the player that will go
  // first. 'plane_cnt' indicates the number of planes, each of
  // 'size' required to hold the state for the board.
  Board2D(const size_t plane_cnt, const cifre::BoardSize2D& size,
          const std::vector<Player*>& players, size_t starting_player_id);

  // Copy constructor.
  Board2D(const Board2D& other);

  // Destructor
  virtual ~Board2D() = default;

  // See cifre::State. Hash is based on on 'cells_'.
  virtual size_t Hash() const override;

  // See cifre::State. Equally is determined by 'cells_'.
  virtual bool Equal(const State* other) const override;

  // Return the number of planes required for the board state. Each 2D
  // plane's size is Size().
  size_t PlaneCount() const { return plane_cnt_; }

  // Return the board size.
  const BoardSize2D& Size() const { return size_; }

  // Return the player ID that represents "no player". By convention
  // this ID is equal to the number of players in the game.
  size_t NoPlayerId() const { return players_.size(); }

  // Return the ID of the active player. See cifre::State.
  size_t ActivePlayerId() const override { return active_player_id_; }

  // Return the active player.
  Player* ActivePlayer() const { return players_[active_player_id_]; }

  // See cifre::State
  StateTerminalStatus TerminalStatus() const override { return terminal_status_; }

  // See cifre::State
  const std::unordered_set<uint64_t>& ValidActionIds() const override {
    return valid_action_ids_; }

  // See cifre::State.
  virtual bool IsValidAction(const Action& action) const override;

  // Get the representation of the state suitable for feeding to the
  // model via triton. Returns flattened row-major ordering of the
  // cell values. See cifre::State.
  const std::vector<uint8_t>& TritonRepresentation() const override {
    return cells_flattened_;
  }

protected:
  // Number of planes.
  const size_t plane_cnt_;

  // Size of the board.
  const BoardSize2D size_;

  // The players. The player's index in this vector is the player ID.
  const std::vector<cifre::Player*> players_;

  // ID of the currently active player.
  size_t active_player_id_;

  // The status of the board indicating winning condition.
  cifre::StateTerminalStatus terminal_status_;

  // The set of action ids that are valid for the current board.
  std::unordered_set<uint64_t> valid_action_ids_;

public:
  //
  // FlattenedCells
  //
  // Triton-friendly cell contents. FlattenedCells is just a
  // std:vector<uint8_t> protected so that it is not unintentionally
  // copied. Must us Copy() explicitly to copy.
  //
  class FlattenedCells : public std::vector<uint8_t> {
  public:
    // Default constructor
    FlattenedCells() = default;

    // Default move constructor and operator
    FlattenedCells(FlattenedCells&& other) = default;
    FlattenedCells& operator=(FlattenedCells&& other) = default;

    // Copy contents of 'c' into this FlattenedCells.
    void CopyFrom(const FlattenedCells& c) {
      *this = c;
    }

  private:
    FlattenedCells(const FlattenedCells&) = default;
    FlattenedCells& operator=(const FlattenedCells&) = default;
  };

protected:
  // 'cells_' is simply a type-casted pointer into 'cells_flattened_'.
  FlattenedCells cells_flattened_;
  float* cells_;
};

} // namespace cifre
