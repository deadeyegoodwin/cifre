// Copyright 2025, David Goodwin. All rights reserved.

#include "gettysburg_oob.h"

namespace cifre { namespace gettysburg {

namespace {

// Union units for JULY_1 scenario.
std::vector<std::unique_ptr<cifre::gettysburg::Unit>>
UnionUnitsJuly1(size_t turn)
{
  std::vector<std::unique_ptr<cifre::gettysburg::Unit>> units;

  switch (turn) {
  case 0: // 6-8am, start of scenario
    units.emplace_back(
      std::make_unique<Unit>(
        "Reynolds", Unit::ArmyKind::UNION, Unit::UnitKind::HEADQUARTERS, 0 /*CF*/, 10 /*MF*/, 2 /*3*/, 3 /*D*/));
    units.emplace_back(
      std::make_unique<Unit>(
        "Wadsworth", Unit::ArmyKind::UNION, Unit::UnitKind::INFANTRY, 3 /*CF*/, 5 /*MF*/, 2 /*3*/, 3 /*D*/));
    units.emplace_back(
      std::make_unique<Unit>(
        "Buford", Unit::ArmyKind::UNION, Unit::UnitKind::HEADQUARTERS, 0 /*CF*/, 10 /*MF*/, 4 /*5*/, 14 /*O*/));
    units.emplace_back(
      std::make_unique<Unit>(
        "Gamble", Unit::ArmyKind::UNION, Unit::UnitKind::CAVALRY, 1 /*CF*/, 7 /*MF*/, 4 /*5*/, 14 /*O*/));
    units.emplace_back(
      std::make_unique<Unit>(
        "Devin", Unit::ArmyKind::UNION, Unit::UnitKind::CAVALRY, 1 /*CF*/, 7 /*MF*/, 6 /*7*/, 16 /*Q*/));
    break;

  case 1: // 8-10am
    units.emplace_back(
      std::make_unique<Unit>(
        "Howard", Unit::ArmyKind::UNION, Unit::UnitKind::HEADQUARTERS, 0 /*CF*/, 10 /*MF*/, 7 /*8*/, 0 /*A*/));
    break;
  }

  return units;
}

// Confederate units for JULY_1 scenario.
std::vector<std::unique_ptr<cifre::gettysburg::Unit>>
ConfederateUnitsJuly1(size_t turn)
{
  std::vector<std::unique_ptr<cifre::gettysburg::Unit>> units;

  switch (turn) {
  case 0: // 6-8am, start of scenario
    break;

  case 1: // 8-10am
    units.emplace_back(
      std::make_unique<Unit>(
        "Heth", Unit::ArmyKind::CONFEDERATE, Unit::UnitKind::INFANTRY, 5 /*CF*/, 5 /*MF*/, 0 /*1*/, 18 /*S*/));
    units.emplace_back(
      std::make_unique<Unit>(
        "Pegram", Unit::ArmyKind::CONFEDERATE, Unit::UnitKind::ARTILLERY, 2 /*CF*/, 5 /*MF*/, 0 /*1*/, 18 /*S*/));
    break;
  }

  return units;
}

} // namespace

//
// OOB  (Order Of Battle)
//
std::string
OOB::TurnToString(const Scenario s, size_t turn)
{
  // For JULY_1 can use 'turn' directly...
  switch (turn) {
  case 0:
    return "July 1, 6-8am";
  case 1:
    return "July 1, 8-10am";
  case 2:
    return "July 1, 10am-12pm";
  case 3:
    return "July 1, 12-2pm";
  case 4:
    return "July 1, 2-4pm";
  case 5:
    return "July 1, 4-6pm";
  case 6:
    return "July 1, 6-8pm";
  case 7:
    return "July 1, night";
  }

  return "<unknown>";
}

size_t
OOB::LastTurn(Scenario scenario)
{
  switch (scenario) {
  case Scenario::JULY_1:  // last turn is the night of July 1st
    return 7;
  }

  return 0;  // not reached
}

std::vector<std::unique_ptr<Unit>>
OOB::Units(Scenario scenario, Unit::ArmyKind army, size_t turn)
{
  switch (scenario) {
  case Scenario::JULY_1:
    switch (army) {
    case Unit::ArmyKind::UNION:
      return UnionUnitsJuly1(turn);
    case Unit::ArmyKind::CONFEDERATE:
      return ConfederateUnitsJuly1(turn);
    }
  }

  return { };
}

}} // namespace cifre::gettysburg
