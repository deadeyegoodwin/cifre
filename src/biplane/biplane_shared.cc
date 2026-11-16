// Copyright 2026, David Goodwin. All rights reserved.

#include <cassert>
#include <tuple>
#include "biplane_shared.h"
#include "cifre_std.h"

namespace cifre { namespace biplane {

//
// Utility
//
namespace {

static const cifre::array<MatrixCoordinate, cifre::biplane::NUM_ABSOLUTE_ORIENTATIONS> unit_vectors_ {
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

MatrixCoordinate
RotateRelativePosition(const MatrixCoordinate& pos, const size_t rot)
{
  if (rot == 0) {
    return pos;
  }

  MatrixCoordinate rot_pos;

  size_t diagonal_count, straight_count;
  AbsoluteOrientation diagonal_dir, straight_dir;
  DecomposeSteps(pos, diagonal_count, diagonal_dir, straight_count, straight_dir);

  // Perform steps along diagonal direction.
  if (diagonal_count > 0) {
    const size_t drot = (size_t(diagonal_dir) + rot + NUM_ABSOLUTE_ORIENTATIONS) % NUM_ABSOLUTE_ORIENTATIONS;
    rot_pos += unit_vectors_[drot] * int64_t(diagonal_count);
  }

  // Perform steps along straight direction.
  if (straight_count > 0) {
    const size_t drot = (size_t(straight_dir) + rot + NUM_ABSOLUTE_ORIENTATIONS) % NUM_ABSOLUTE_ORIENTATIONS;
    rot_pos += unit_vectors_[drot] * int64_t(straight_count);
  }

  return rot_pos;
}

} // namespace

void
RelativePositionToAbsolute(
  const MatrixCoordinate& center_pos, const AbsoluteOrientation& center_orient,
  MatrixCoordinate& pos, AbsoluteOrientation& orient)
{
  // The clockwise rotation required for 'pos'ition and 'orient'ation
  // is the absolute orientation of 'center_orient'.
  const size_t rot = size_t(center_orient);
  const MatrixCoordinate rel_rot = RotateRelativePosition(pos, rot);
  orient = AbsoluteOrientation((size_t(orient) + rot) % NUM_ABSOLUTE_ORIENTATIONS);
  pos = center_pos + rel_rot;
}

void
AbsolutePositionToRelative(
  const MatrixCoordinate& center_pos, const AbsoluteOrientation& center_orient,
  MatrixCoordinate& pos, AbsoluteOrientation& orient)
{
  // The counter-clockwise rotation required for 'pos'ition and
  // 'orient'ation is the absolute orientation of 'center_orient'.
  const size_t rot =
    (center_orient == AbsoluteOrientation::NORTH) ? size_t(0) :
    (NUM_ABSOLUTE_ORIENTATIONS - size_t(center_orient));
  const MatrixCoordinate rel_rot = RotateRelativePosition(pos - center_pos, rot);
  orient = AbsoluteOrientation((size_t(orient) + rot) % NUM_ABSOLUTE_ORIENTATIONS);
  pos = rel_rot;
}

void
DecomposeSteps(
  const MatrixCoordinate& pos,
  size_t& diagonal_count, AbsoluteOrientation& diagonal_dir,
  size_t& straight_count, AbsoluteOrientation& straight_dir)
{
  // 'pos' is assumed to be a relative position. Decompose 'pos' into
  // diagonal and straight steps. Maximize diagonal steps.
  diagonal_count = std::min(abs(pos.Row()), abs(pos.Column()));
  straight_count = std::max(abs(pos.Row()), abs(pos.Column())) - diagonal_count;

  // Determine the direction of the diagonal, rotate to the new
  // direction, and perform the same number of steps along the new
  // direction.
  diagonal_dir = AbsoluteOrientation::NORTH;
  if (diagonal_count > 0) {
    assert((abs(pos.Row()) >= diagonal_count) && (abs(pos.Column()) >= diagonal_count));
    if (pos.Column() >= 0) {
      diagonal_dir = (pos.Row() >= 0) ? AbsoluteOrientation::NORTH_EAST : AbsoluteOrientation::SOUTH_EAST;
    } else {
      diagonal_dir = (pos.Row() >= 0) ? AbsoluteOrientation::NORTH_WEST : AbsoluteOrientation::SOUTH_WEST;
    }

    assert(diagonal_dir != AbsoluteOrientation::NORTH);
  }

  straight_dir = AbsoluteOrientation::NORTH_EAST;
  if (straight_count > 0) {
    assert(abs(pos.Column()) != abs(pos.Row()));
    if (abs(pos.Column()) > abs(pos.Row())) {
      straight_dir = (pos.Column() > 0) ? AbsoluteOrientation::EAST : AbsoluteOrientation::WEST;
    }
    else {
      straight_dir = (pos.Row() > 0) ? AbsoluteOrientation::NORTH : AbsoluteOrientation::SOUTH;
    }

    assert(straight_dir != AbsoluteOrientation::NORTH_EAST);
  }
}

//
// Orientations
//
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

std::string
AbsoluteOrientationToBoardShortString(AbsoluteOrientation o)
{
  switch (o) {
  case AbsoluteOrientation::NORTH:      return "UP";
  case AbsoluteOrientation::NORTH_EAST: return "UR";
  case AbsoluteOrientation::EAST:       return "RT";
  case AbsoluteOrientation::SOUTH_EAST: return "DR";
  case AbsoluteOrientation::SOUTH:      return "DN";
  case AbsoluteOrientation::SOUTH_WEST: return "DL";
  case AbsoluteOrientation::WEST:       return "LT";
  case AbsoluteOrientation::NORTH_WEST: return "UL";
  }

  // unreachable
  return "<unknown>";
}

//
// Actions
//
class BiplaneActionInfo {
public:
  BiplaneActionInfo(
    const std::string n, const MatrixCoordinate pdelta, const int64_t odelta) :
    name(n), position_delta(pdelta), orientation_delta(odelta) { }

  const std::string name;
  const MatrixCoordinate position_delta;
  const int64_t orientation_delta;
};


const cifre::array<BiplaneActionInfo, TOTAL_ACTIONID_COUNT> action_info_ {
    // name                                          [y, x]  dir
  BiplaneActionInfo { std::string("SLOW-STRAIGHT"), {1, 0}, 0 },
  BiplaneActionInfo { std::string("SLOW-45-LEFT"), {1, -1}, -1 },
  BiplaneActionInfo { std::string("SLOW-45-RIGHT"), {1, 1}, 1 },
  BiplaneActionInfo { std::string("SLOW-90-LEFT"), {1, -1}, -2 },
  BiplaneActionInfo { std::string("SLOW-90-RIGHT"), {1, 1}, 2 },
  BiplaneActionInfo { std::string("SLOW-135-LEFT"), {1, -1}, -3 },
  BiplaneActionInfo { std::string("SLOW-135-RIGHT"), {1, 1}, 3 },

  BiplaneActionInfo { std::string("SLOW-45-LSLIP"), {1, 0}, -1 },
  BiplaneActionInfo { std::string("SLOW-45-RSLIP"), {1, 0}, 1 },
  BiplaneActionInfo { std::string("SLOW-90-LSLIP"), {1, 0}, -2 },
  BiplaneActionInfo { std::string("SLOW-90-RSLIP"), {1, 0}, 2 },

  BiplaneActionInfo { std::string("MED-STRAIGHT"), {2, 0}, 0 },
  BiplaneActionInfo { std::string("MED-45-LEFT"), {2, -1}, -1 },
  BiplaneActionInfo { std::string("MED-45-RIGHT"), {2, 1}, 1 },
  BiplaneActionInfo { std::string("MED-90-LEFT"), {2, -1}, -2 },
  BiplaneActionInfo { std::string("MED-90-RIGHT"), {2, 1}, 2 },
  BiplaneActionInfo { std::string("MED-135-LEFT"), {2, -1}, -3 },
  BiplaneActionInfo { std::string("MED-135-RIGHT"), {2, 1}, 3 }
};

std::string
ActionIdToString(uint64_t aid)
{
  assert(aid < TOTAL_ACTIONID_COUNT);
  return action_info_[aid].name;
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
ActionIdToAbsoluteDelta(
  const uint64_t aid, const AbsoluteOrientation orientation,
  MatrixCoordinate& pos_delta, int64_t& orientation_delta)
{
  assert(aid < TOTAL_ACTIONID_COUNT);

  // Relative position delta must be rotated so that it is applied
  // along the forward direction of 'player_id'.
  pos_delta = RotateRelativePosition(action_info_[aid].position_delta, size_t(orientation));
  orientation_delta = action_info_[aid].orientation_delta;
}

}} // namespace cifre::biplane
