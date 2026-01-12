// Copyright 2026, David Goodwin. All rights reserved.
#pragma once

#include <cstdint>
#include <string>

#include "cifre_hex.h"

namespace cifre { namespace gettysburg {

//
// Move actions that are allowed are based on the supported
// move-radius. The move action-ids correspond to the same-valued
// HexDirection. Thus move action-ids start at 0.
//
static constexpr size_t MOVE_ACTIONID_COUNT =
  1 +  /* HexDirection::NONE */
  HEX_RADIUS1_DIRECTION_COUNT;
static constexpr size_t TOTAL_ACTIONID_COUNT = MOVE_ACTIONID_COUNT;

// Action-id corresponding to MOVE-END action. This is the action-id
// corresponding to HexDirection::NONE.
static constexpr uint64_t ACTIONID_MOVE_END = uint64_t(HexDirection::NONE);

// Return true is direction 'dir' has a corresponding action-id.
inline bool IsActionIdForHexDirection(HexDirection dir) {
  return (uint64_t(dir) < MOVE_ACTIONID_COUNT); }

// Convert an action-id to the corresponding HexDirection. Error if
// action-id does not correspond to a HexDirection.
inline HexDirection ActionIdToHexDirection(uint64_t aid) {
  assert(aid < MOVE_ACTIONID_COUNT); return HexDirection(aid); }

// Convert a HexDirection to the corresponding action-id.
inline uint64_t HexDirectionToActionId(HexDirection dir) {
  assert(uint64_t(dir) < MOVE_ACTIONID_COUNT); return uint64_t(dir); }

// Return string representation of an action-id.
std::string ActionIdToString(uint64_t aid);

}} // namespace cifre::gettysburg
