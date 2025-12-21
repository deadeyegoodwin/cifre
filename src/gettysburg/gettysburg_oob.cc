// Copyright 2025, David Goodwin. All rights reserved.

#include "gettysburg_oob.h"

namespace cifre { namespace gettysburg {

namespace {

// Union units for SMALL scenario.
std::vector<cifre::gettysburg::Unit>
UnionUnitsSmall(size_t turn)
{
  // Small scenario is based on JULY_1 but has fewer units and they
  // are constrained to appear in a 5x7 area as that is the smallest
  // supported board.
  std::vector<cifre::gettysburg::Unit> units;

  switch (turn) {
  case 0: // 6-8am, start of scenario
    units.emplace_back(
      "Reynolds", Unit::ArmyKind::UNION, Unit::UnitKind::HEADQUARTERS, 0 /*CF*/, 10 /*MF*/, 0, 0);
    units.emplace_back(
      "Wadsworth", Unit::ArmyKind::UNION, Unit::UnitKind::INFANTRY, 3 /*CF*/, 5 /*MF*/, 0, 0);
    break;

  case 1: // 8-10am
    units.emplace_back(
      "Devin", Unit::ArmyKind::UNION, Unit::UnitKind::CAVALRY, 1 /*CF*/, 7 /*MF*/, 4, 1);
    break;
  }

  return units;
}

// Confederate units for SMALL scenario.
std::vector<cifre::gettysburg::Unit>
ConfederateUnitsSmall(size_t turn)
{
  // Small scenario is based on JULY_1 but has fewer units and they
  // are constrained to appear in a 5x7 area as that is the smallest
  // supported board.
  std::vector<cifre::gettysburg::Unit> units;

  switch (turn) {
  case 0: // 6-8am, start of scenario
    break;

  case 1: // 8-10am
    units.emplace_back(
      "Heth", Unit::ArmyKind::CONFEDERATE, Unit::UnitKind::INFANTRY, 5 /*CF*/, 5 /*MF*/, 0, 6);
    units.emplace_back(
      "Pegram", Unit::ArmyKind::CONFEDERATE, Unit::UnitKind::ARTILLERY, 2 /*CF*/, 5 /*MF*/, 0, 6);
    break;

  case 2: // 10am-12pm
    units.emplace_back(
      "Hill", Unit::ArmyKind::CONFEDERATE, Unit::UnitKind::HEADQUARTERS, 0 /*CF*/, 10 /*MF*/, 4, 6);
  }

  return units;
}

// Union units for JULY_1 scenario.
std::vector<cifre::gettysburg::Unit>
UnionUnitsJuly1(size_t turn)
{
  std::vector<cifre::gettysburg::Unit> units;

  switch (turn) {
  case 0: // 6-8am, start of scenario
    units.emplace_back(
      "Reynolds", Unit::ArmyKind::UNION, Unit::UnitKind::HEADQUARTERS, 0 /*CF*/, 10 /*MF*/, 2 /*3*/, 3 /*D*/);
    units.emplace_back(
      "Wadsworth", Unit::ArmyKind::UNION, Unit::UnitKind::INFANTRY, 3 /*CF*/, 5 /*MF*/, 2 /*3*/, 3 /*D*/);
    units.emplace_back(
      "Buford", Unit::ArmyKind::UNION, Unit::UnitKind::HEADQUARTERS, 0 /*CF*/, 10 /*MF*/, 4 /*5*/, 14 /*O*/);
    units.emplace_back(
      "Gamble", Unit::ArmyKind::UNION, Unit::UnitKind::CAVALRY, 1 /*CF*/, 7 /*MF*/, 4 /*5*/, 14 /*O*/);
    units.emplace_back(
      "Devin", Unit::ArmyKind::UNION, Unit::UnitKind::CAVALRY, 1 /*CF*/, 7 /*MF*/, 6 /*7*/, 16 /*Q*/);
    break;

  case 1: // 8-10am
    units.emplace_back(
      "Howard", Unit::ArmyKind::UNION, Unit::UnitKind::HEADQUARTERS, 0 /*CF*/, 10 /*MF*/, 7 /*8*/, 0 /*A*/);
    break;
  }

  return units;
}

// Confederate units for JULY_1 scenario.
std::vector<cifre::gettysburg::Unit>
ConfederateUnitsJuly1(size_t turn)
{
  std::vector<cifre::gettysburg::Unit> units;

  switch (turn) {
  case 0: // 6-8am, start of scenario
    break;

  case 1: // 8-10am
    units.emplace_back(
      "Heth", Unit::ArmyKind::CONFEDERATE, Unit::UnitKind::INFANTRY, 5 /*CF*/, 5 /*MF*/, 0 /*1*/, 18 /*S*/);
    units.emplace_back(
      "Pegram", Unit::ArmyKind::CONFEDERATE, Unit::UnitKind::ARTILLERY, 2 /*CF*/, 5 /*MF*/, 0 /*1*/, 18 /*S*/);
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
  // For SMALL and JULY_1 can use 'turn' directly...
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
  case Scenario::SMALL:
  case Scenario::JULY_1:  // last turn is the night of July 1st
    return 7;
  }

  return 0;  // not reached
}

std::vector<Unit>
OOB::Units(Scenario scenario, Unit::ArmyKind army, size_t turn)
{
  switch (scenario) {
  case Scenario::SMALL:
    switch (army) {
    case Unit::ArmyKind::UNION:
      return UnionUnitsSmall(turn);
    case Unit::ArmyKind::CONFEDERATE:
      return ConfederateUnitsSmall(turn);
    }
    break;

  case Scenario::JULY_1:
    switch (army) {
    case Unit::ArmyKind::UNION:
      return UnionUnitsJuly1(turn);
    case Unit::ArmyKind::CONFEDERATE:
      return ConfederateUnitsJuly1(turn);
    }
    break;
  }

  return { };
}

}} // namespace cifre::gettysburg
