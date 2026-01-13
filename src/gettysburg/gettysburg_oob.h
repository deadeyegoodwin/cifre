// Copyright 2026, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "gettysburg_unit.h"

namespace cifre { namespace gettysburg {

//
// OOB  (Order Of Battle)
//
// The order of battle for different scenarios, indicating which
// forces become available at what time and location. The OOB is a
// completely const/immutable object and thus has only static methods.
//
class OOB {
public:
  // The maximum number of units that can be active at any given time
  // for any scenario for either army (i.e. the maximum of union and
  // confederate, not the sum of the maximums).
  static constexpr size_t MAX_UNITS_PER_ARMY_COUNT = 6;

  // Game scenarios.
  enum class Scenario {
    SMALL,   // Single day, July 1st, small board and reduced units
    JULY_1   // Single day, July 1st
  };

  static std::string ScenarioToString(const Scenario s) {
    switch (s) {
    case Scenario::SMALL: return "SMALL";
    case Scenario::JULY_1: return "JULY-1";
    }
    return "<none>"; // unreachable
  }

public:
  // Return a turn's descriptive name for a given scenario.
  static std::string TurnToString(const Scenario s, size_t turn);

  // Return the last turn for a given scenario.
  static size_t LastTurn(Scenario scenario);

  // Return the set of units that become available for the given
  // 'scenario' and 'army' on a given 'turn'. Turn 0 indicates the
  // start of the scenario, turn 1 indicates the start of the next
  // turn, etc.
  static std::vector<Unit> Units(
    Scenario scenario, Unit::ArmyKind army, size_t turn);
};

}} // namespace cifre::gettysburg
