// Copyright 2025, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

namespace cifre { namespace gettysburg {

class Board;

//
// Goal
//
// A goal (victory point, etc) in gettysburg game.
//
class Goal {
public:
  // Create goal at a given board position.
  Goal(size_t row_idx, size_t col_idx);

  // Copy constructor.
  Goal(const Goal& other) = default;

  // Hash and equal...
  size_t Hash() const;
  bool Equal(const Goal* other) const;

  // Update 'board' to remove this goal.
  void RemoveFromBoard(Board* board) const;

  // Update 'board' to reflect current status of this goal.
  void AddToBoard(Board* board) const;

private:
  size_t row_idx_;
  size_t column_idx_;
};

}} // namespace cifre::gettysburg
