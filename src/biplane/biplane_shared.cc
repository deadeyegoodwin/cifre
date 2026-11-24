// Copyright 2026, David Goodwin. All rights reserved.

#include <cassert>
#include <tuple>
#include "biplane_shared.h"
#include "cifre_std.h"

namespace cifre { namespace biplane {

//
// Normalization
//
bool
HasNSEWNormalization(AbsoluteOrientation orientation)
{
  return ((orientation == AbsoluteOrientation::NORTH) ||
          (orientation == AbsoluteOrientation::SOUTH) ||
          (orientation == AbsoluteOrientation::EAST) ||
          (orientation == AbsoluteOrientation::WEST));
}

void
AbsolutePositionToRelative(
  const MatrixCoordinate& center_pos, const AbsoluteOrientation& center_orient,
  MatrixCoordinate& pos, AbsoluteOrientation& orient)
{
  const MatrixCoordinate pos_delta = pos - center_pos;

  // Adjust 'pos'ition and 'orient'ation as required to normalize
  // center.
  switch (center_orient) {
  case AbsoluteOrientation::NORTH:
  case AbsoluteOrientation::NORTH_EAST:
    pos = pos_delta;
    // orient has no change
    break;

  case AbsoluteOrientation::WEST:
  case AbsoluteOrientation::NORTH_WEST:
    // Rotate 90 degrees clockwise.
    pos = MatrixCoordinate(-pos_delta.Column(), pos_delta.Row());
    orient = IndexToAbsoluteOrientation(int64_t(orient) + 2);  // 2 == 90 degrees
    break;

  case AbsoluteOrientation::SOUTH:
  case AbsoluteOrientation::SOUTH_WEST:
    // Rotate 180 degrees clockwise.
    pos = -pos_delta;
    orient = IndexToAbsoluteOrientation(int64_t(orient) + 4);  // 4 == 180 degrees
    break;
  case AbsoluteOrientation::EAST:
  case AbsoluteOrientation::SOUTH_EAST:
    // Rotate 270 degrees clockwise.
    pos = MatrixCoordinate(pos_delta.Column(), -pos_delta.Row());
    orient = IndexToAbsoluteOrientation(int64_t(orient) + 6);  // 6 == 270 degrees
    break;
  }
}

void
RelativePositionToAbsolute(
  const MatrixCoordinate& center_pos, const AbsoluteOrientation& center_orient,
  MatrixCoordinate& pos, AbsoluteOrientation& orient)
{
  switch (center_orient) {
  case AbsoluteOrientation::NORTH:
  case AbsoluteOrientation::NORTH_EAST:
    break;

  case AbsoluteOrientation::EAST:
  case AbsoluteOrientation::SOUTH_EAST:
    // Rotate 90 degrees clockwise.
    pos = MatrixCoordinate(-pos.Column(), pos.Row());
    orient = IndexToAbsoluteOrientation(int64_t(orient) + 2);  // 2 == 90 degrees
    break;

  case AbsoluteOrientation::SOUTH:
  case AbsoluteOrientation::SOUTH_WEST:
    // Rotate 180 degrees clockwise.
    pos = -pos;
    orient = IndexToAbsoluteOrientation(int64_t(orient) + 4);  // 4 == 180 degrees
    break;
  case AbsoluteOrientation::WEST:
  case AbsoluteOrientation::NORTH_WEST:
    // Rotate 270 degrees clockwise.
    pos = MatrixCoordinate(pos.Column(), -pos.Row());
    orient = IndexToAbsoluteOrientation(int64_t(orient) + 6);  // 6 == 270 degrees
    break;
  }

  pos += center_pos;
}

//
// Orientations
//
AbsoluteOrientation
IndexToAbsoluteOrientation(int64_t idx)
{
  while (idx < 0) {
    idx += ABSOLUTE_ORIENTATION_COUNT;
  }

  return AbsoluteOrientation(idx % ABSOLUTE_ORIENTATION_COUNT);
}

std::string
AbsoluteOrientationToString(AbsoluteOrientation o)
{
  switch (o) {
  case AbsoluteOrientation::NORTH:      return "NORTH";
  case AbsoluteOrientation::NORTH_EAST: return "NORTH-EAST";
  case AbsoluteOrientation::EAST:       return "EAST";
  case AbsoluteOrientation::SOUTH_EAST: return "SOUTH-EAST";
  case AbsoluteOrientation::SOUTH:      return "SOUTH";
  case AbsoluteOrientation::SOUTH_WEST: return "SOUTH-WEST";
  case AbsoluteOrientation::WEST:       return "WEST";
  case AbsoluteOrientation::NORTH_WEST: return "NORTH-WEST";
  }

  // unreachable
  return "<unknown>";
}

std::string
AbsoluteOrientationToShortString(AbsoluteOrientation o)
{
  switch (o) {
  case AbsoluteOrientation::NORTH:      return "N";
  case AbsoluteOrientation::NORTH_EAST: return "NE";
  case AbsoluteOrientation::EAST:       return "E";
  case AbsoluteOrientation::SOUTH_EAST: return "SE";
  case AbsoluteOrientation::SOUTH:      return "S";
  case AbsoluteOrientation::SOUTH_WEST: return "SW";
  case AbsoluteOrientation::WEST:       return "W";
  case AbsoluteOrientation::NORTH_WEST: return "NW";
  }

  // unreachable
  return "<unknown>";
}

//
// Actions
//

// For each absolute or relative orientation, the position delta for a
// single 3:2 grid step in that direction.
constexpr cifre::array<MatrixCoordinate, cifre::biplane::ABSOLUTE_ORIENTATION_COUNT> unit_vectors_ {
  //              [y, x]
  MatrixCoordinate(ACTION_STEP_STRAIGHT, 0),                       // N / F
  MatrixCoordinate(ACTION_STEP_DIAGONAL, ACTION_STEP_DIAGONAL),    // NE / FR
  MatrixCoordinate(0, ACTION_STEP_STRAIGHT),                       // E / R
  MatrixCoordinate(-ACTION_STEP_DIAGONAL, ACTION_STEP_DIAGONAL),   // SE / BR
  MatrixCoordinate(-ACTION_STEP_STRAIGHT, 0),                      // S / B
  MatrixCoordinate(-ACTION_STEP_DIAGONAL, -ACTION_STEP_DIAGONAL),  // SW / BL
  MatrixCoordinate(0, -ACTION_STEP_STRAIGHT),                      // W / L
  MatrixCoordinate(ACTION_STEP_DIAGONAL, -ACTION_STEP_DIAGONAL)    // NW / FL
};

enum RelativeStep : uint8_t {
  F, FR, R, BR, B, BL, L, FL
};

// The information for each action.
struct BiplaneActionInfo {
  static constexpr size_t MAX_STEPS_PER_ACTION = 3;
  const std::string_view name;
  const size_t step_count;
  const std::array<RelativeStep, MAX_STEPS_PER_ACTION> steps;
  const int64_t orientation_delta;
};

constexpr cifre::array<BiplaneActionInfo, TOTAL_ACTIONID_COUNT> action_info_ {{{
  // === Debug actions ===
  //{ "0", 0, {F, F, F}, 0},
  //{ "F", 1, {F, F, F}, 0},
  //{ "L", 0, {F, F, F}, -1 },
  //{ "R", 0, {F, F, F}, +1 },
  // === Slow speed actions ===                // [x,y] N / NE
  { "SLOW-EARLY-90-LEFT", 2, {F, BL, F}, -2 }, // (-2, 1) / (-1, 2)
  { "SLOW-EARLY-45-LEFT", 2, {F, BL, F}, -1 }, // (-2, 1) / (-1, 2)
  { "SLOW-90-LEFT", 1, {FL, F, F}, -2},        // (-2, 2) / (0, 3)
  { "SLOW-45-LEFT", 1, {FL, F, F}, -1},        // (-2, 2) / (0, 3)
  { "SLOW-LATE-90-LEFT", 2, {L, FR, F}, -2},   // (-1, 2) / (1, 2)
  { "SLOW-LATE-45-LEFT", 2, {L, FR, F}, -1},   // (-1, 2) / (1, 2)
  { "SLOW-STRAIGHT", 1, {F, F, F}, 0},         // (0, 3) / (2, 2)
  { "SLOW-LATE-45-RIGHT", 2, {R, FL, F}, +1},  // (1, 2) / (2, 1)
  { "SLOW-LATE-90-RIGHT", 2, {R, FL, F}, +2},  // (1, 2) / (2, 1)
  { "SLOW-45-RIGHT", 1, {FR, F, F}, +1},       // (2, 2) / (3, 0)
  { "SLOW-90-RIGHT", 1, {FR, F, F}, +2},       // (2, 2) / (3, 0)
  { "SLOW-EARLY-45-RIGHT", 2, {F, BR, F}, +1}, // (2, 1) / (2,-1)
  { "SLOW-EARLY-90-RIGHT", 2, {F, BR, F}, +2}, // (2, 1) / (2,-1)
  // === Medium speed actions ===                 // [x,y] N / NE
  { "MEDIUM-EARLY-90-LEFT", 2, {L, FL, F}, -2 }, // (-5, 2) / (-2, 5)
  { "MEDIUM-EARLY-45-LEFT", 2, {L, FL, F}, -1 }, // (-5, 2) / (-2, 5)
  { "MEDIUM-90-LEFT", 2, {FL, FL, F}, -2},       // (-4, 4) / (0, 6)
  { "MEDIUM-45-LEFT", 2, {FL, FL, F}, -1},       // (-4, 4) / (0, 6)
  { "MEDIUM-LATE-90-LEFT", 2, {F, FL, F}, -2},   // (-2, 5) / (2, 5)
  { "MEDIUM-LATE-45-LEFT", 2, {F, FL, F}, -1},   // (-2, 5) / (2, 5)
  { "MEDIUM-STRAIGHT", 2, {F, F, F}, 0},         // (0, 6) / (4, 4)
  { "MEDIUM-LATE-45-RIGHT", 2, {F, FR, F}, +1},  // (2, 5) / (5, 2)
  { "MEDIUM-LATE-90-RIGHT", 2, {F, FR, F}, +2},  // (2, 5) / (5, 2)
  { "MEDIUM-45-RIGHT", 2, {FR, FR, F}, +1},      // (4, 4) / (6, 0)
  { "MEDIUM-90-RIGHT", 2, {FR, FR, F}, +2},      // (4, 4) / (6, 0)
  { "MEDIUM-EARLY-45-RIGHT", 2, {R, FR, F}, +1}, // (5, 2) / (5,-2)
  { "MEDIUM-EARLY-90-RIGHT", 2, {R, FR, F}, +2}, // (5, 2) / (5,-2)
}}};
    // // ================= FAST TIER =================
    // {3, {L, FL, FL}, -2}, // FAST_EARLY_90_L (-7, 4) / (-2, 8)
    // {3, {L, FL, FL}, -1}, // FAST_EARLY_45_L (-7, 4) / (-2, 8)
    // {3, {F, FL, FL}, -2}, // FAST_MID_90_L   (-4, 7) / ( 2, 8)
    // {3, {F, FL, FL}, -1}, // FAST_MID_45_L   (-4, 7) / ( 2, 8)
    // {3, {F, F, FL}, -2}, // FAST_LATE_90_L  (-2, 8) / ( 4, 7)
    // {3, {F, F, FL}, -1}, // FAST_LATE_45_L  (-2, 8) / ( 4, 7)
    // {3, {F, F, F }, 0}, // FAST_STRAIGHT   ( 0, 9) / ( 6, 6)
    // {3, {F, F, FR}, +1}, // FAST_LATE_45_R  ( 2, 8) / ( 7, 4)
    // {3, {F, F, FR}, +2}, // FAST_LATE_90_R  ( 2, 8) / ( 7, 4)
    // {3, {F, FR, FR}, +1}, // FAST_MID_45_R   ( 4, 7) / ( 8, 2)
    // {3, {F, FR, FR}, +2}, // FAST_MID_90_R   ( 4, 7) / ( 8, 2)
    // {3, {R, FR, FR}, +1}, // FAST_EARLY_45_R ( 7, 4) / ( 8,-2)
    // {3, {R, FR, FR}, +2}  // FAST_EARLY_90_R ( 7, 4) / ( 8,-2)

struct BiplaneActionOutcome {
  MatrixCoordinate position_delta;
  AbsoluteOrientation new_orientation;
};

using BiplaneActionOutcomeForOrientation =
  cifre::array<cifre::array<BiplaneActionOutcome, TOTAL_ACTIONID_COUNT>,
               ABSOLUTE_ORIENTATION_COUNT>;

constexpr BiplaneActionOutcomeForOrientation init_action_outcomes_()
{
  BiplaneActionOutcomeForOrientation outcomes { };
  for (size_t oidx = 0; oidx < ABSOLUTE_ORIENTATION_COUNT; ++oidx) {
    for (size_t aid = 0; aid < TOTAL_ACTIONID_COUNT; ++aid) {
      const auto& ainfo = action_info_[aid];
      MatrixCoordinate position_delta(0, 0);
      for (size_t sidx = 0; sidx < ainfo.step_count; ++sidx) {
        const size_t step_abs_dir =
          (oidx + ainfo.steps[sidx] + ABSOLUTE_ORIENTATION_COUNT) % ABSOLUTE_ORIENTATION_COUNT;
        position_delta += unit_vectors_[step_abs_dir];
      }

      const AbsoluteOrientation new_orientation =
        AbsoluteOrientation((oidx + ainfo.orientation_delta + ABSOLUTE_ORIENTATION_COUNT) %
                            ABSOLUTE_ORIENTATION_COUNT);
      outcomes[oidx][aid] =
        BiplaneActionOutcome { position_delta, new_orientation };
    }
  }

  return outcomes;
}

// For each action and active player orientation calculate the
// position delta and new orientation. This is done using the
// constexpr information above so it is all performed at compile time.
constexpr BiplaneActionOutcomeForOrientation action_outcome_from_orientation_ = init_action_outcomes_();

// Compile-time sanity checking for action outcome calculations.
static_assert(
  action_outcome_from_orientation_[
    size_t(AbsoluteOrientation::SOUTH_WEST)][6 /*SLOW-STRAIGHT*/].position_delta == MatrixCoordinate(-2, -2));
static_assert(
  action_outcome_from_orientation_[
    size_t(AbsoluteOrientation::SOUTH_WEST)][6 /*SLOW-STRAIGHT*/].new_orientation == AbsoluteOrientation::SOUTH_WEST);
static_assert(
  action_outcome_from_orientation_[
    size_t(AbsoluteOrientation::WEST)][10 /*SLOW-90-RIGHT*/].position_delta == MatrixCoordinate(2, -2));
static_assert(
  action_outcome_from_orientation_[
    size_t(AbsoluteOrientation::WEST)][10 /*SLOW-90-RIGHT*/].new_orientation == AbsoluteOrientation::NORTH);
static_assert(
  action_outcome_from_orientation_[
    size_t(AbsoluteOrientation::NORTH_EAST)][21 /*MEDIUM-LATE-90-RIGHT*/].position_delta == MatrixCoordinate(2, 5));
static_assert(
  action_outcome_from_orientation_[
    size_t(AbsoluteOrientation::NORTH_EAST)][21 /*MEDIUM-LATE-90-RIGHT*/].new_orientation == AbsoluteOrientation::SOUTH_EAST);


std::string
ActionIdToString(uint64_t aid)
{
  assert(aid < TOTAL_ACTIONID_COUNT);
  return std::string(action_info_[aid].name);
}

uint64_t
StringToActionId(const std::string& str)
{
  for (size_t idx = 0; idx < action_info_.size(); ++idx) {
    if (str == action_info_[idx].name) {
      return idx;
    }
  }

  return TOTAL_ACTIONID_COUNT;
}

void
ActionIdToAbsoluteOutcome(
  const uint64_t aid, const AbsoluteOrientation orientation,
  MatrixCoordinate& position_delta, AbsoluteOrientation& new_orientation)
{
  assert(aid < TOTAL_ACTIONID_COUNT);
  const auto& outcome = action_outcome_from_orientation_[size_t(orientation)][aid];
  position_delta = outcome.position_delta;
  new_orientation = outcome.new_orientation;
}

}} // namespace cifre::biplane
