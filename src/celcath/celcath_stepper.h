// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <cstddef>
#include <cstdint>

#include "celcath_player.h"
#include "cifre_board2d.h"
#include "common_logger.h"

namespace cifre { namespace celcath {

class Board;

//
// Stepper
//
// Base class for a generation stepper.
//
class Stepper {
public:
  Stepper(const std::shared_ptr<common::Logger>& logger) : logger_(logger) { }
  virtual ~Stepper() = default;

  // Return the name of the stepper.
  virtual const std::string& Name() const = 0;

  // Step 'board' one generation.
  virtual void Step(Board* board, const std::vector<Player*>& players) const = 0;

protected:
  std::shared_ptr<common::Logger> logger_;
};


//
// B3S23Stepper
//
// "Game of Life" stepper. Each cell sums the neighbor value of 8
// heighbors. Cell is "born" if sum == 3, stays the same state if sum
// is 2 or 3, and dies otherwise.
//
class B3S23Stepper : public Stepper {
public:
  B3S23Stepper(const std::shared_ptr<common::Logger>& logger) : Stepper(logger) { }

  const std::string& Name() const override;
  void Step(Board* board, const std::vector<Player*>& players) const override;

private:
  std::string NeighborValuesToString(
    const cifre::BoardSize2D& board_size,
    const std::vector<Player*>& players,
    const std::vector<std::vector<int32_t>>& nv) const;
};

}} // namespace cifre::celcath
