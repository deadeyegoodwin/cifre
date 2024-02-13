// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <cstddef>
#include <cstdint>

#include "celcath_board.h"
#include "celcath_player.h"
#include "common_logger.h"

namespace celcath {

class Stepper;

//
// Stepper
//
// Base class for a generation stepper.
//
class Stepper {
public:
  Stepper(common::Logger* logger) : logger_(logger) { }

  // Return the name of the stepper.
  virtual const std::string& Name() const = 0;

  // Step 'board' one generation.
  virtual void Step(Board* board, const std::vector<Player*>& players) = 0;

protected:
  common::Logger* logger_;
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
  B3S23Stepper(common::Logger* logger) : Stepper(logger) { }

  virtual const std::string& Name() const override;
  virtual void Step(Board* board, const std::vector<Player*>& players) override;

private:
  std::string NeighborValuesToString(
    const std::vector<Player*>& players,
    const std::vector<std::vector<std::vector<int32_t>>>& nv) const;
};



} // celcath
