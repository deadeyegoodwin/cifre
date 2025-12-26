// Copyright 2025, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory_resource>
#include <vector>

#include "cifre_action.h"

namespace cifre {

//
// StateTerminalStatus
//
// Indicates the terminal status of a State. A state is a terminal if
// if represents an end-game state.
//
class StateTerminalStatus {
public:
  enum class TerminalKind { NON_TERMINAL, WINNER, DRAW };

  StateTerminalStatus(TerminalKind kind) : kind_(kind) {
    assert(kind_ != TerminalKind::WINNER); }
  StateTerminalStatus(TerminalKind kind, size_t winner_id) : kind_(kind), winner_id_(winner_id) {
    assert(kind_ == TerminalKind::WINNER); }

  // Indicates if State is a terminal and if so what kind.
  TerminalKind Kind() const { return kind_; }

  // Return the ID of the winning player. Call only if TerminalKind is
  // Kind::WINNER.
  size_t WinnerId() const {
    assert(kind_ == TerminalKind::WINNER); return winner_id_; }

private:
  TerminalKind kind_;
  size_t winner_id_;
};

//
// FlattenedCells
//
// Triton-friendly cell contents. FlattenedCells is just a
// std:vector<uint8_t> protected so that it can only be copied via a
// memory arena.
//
class FlattenedCells : public std::pmr::vector<uint8_t> {
public:
  FlattenedCells(std::pmr::memory_resource* arena) :
    std::pmr::vector<uint8_t>(arena) { }

  // Construct as a copy of 'other' using 'arena' for memory required
  // by flattened cells.
  FlattenedCells(const FlattenedCells& other, std::pmr::memory_resource* arena) :
    std::pmr::vector<uint8_t>(other.begin(), other.end(), arena) { }

  // Allow move
  FlattenedCells(FlattenedCells&& other) = default;
  FlattenedCells& operator=(FlattenedCells&& other) = default;

  // Delete default copy and assign, must use arena version above.
  FlattenedCells(FlattenedCells&) = delete;
  FlattenedCells& operator=(FlattenedCells&) = delete;
  FlattenedCells(const FlattenedCells&) = delete;
  FlattenedCells& operator=(const FlattenedCells&) = delete;
};

//
// State
//
// Interface for game state. State represents all mutable status,
// properties, etc. of a game. State is assumed to be non-thread-safe.
//
// Memory Management: The State interface refers to pmr resources to
// enable player algorithms, like AlphaZero, to efficiently manage
// memory used for game state. An algorithm may use a memory resource
// to allocate state, and it is the state's responsibility to use the
// provided pmr memory_resource for all allocations.
//
class State {
public:
  // Destructor
  virtual ~State() = default;

  // Return the hash of this state. The hash must satisfy all the
  // properties of std::hash.
  virtual size_t Hash() const = 0;

  // Return true if this state is equal to 'other'. Must satisfy all the
  // properties of std::is_equal.
  virtual bool Equal(const State* other) const = 0;

  // Return the total number of action IDs available across all
  // variations of this state. Each action ID is a numeric identifier
  // of type uint64_t in the range [0, total-action-id-count).
  virtual size_t TotalActionIdCount() const = 0;

  // Get the action identifiers that are valid for this state.
  virtual const std::pmr::vector<uint64_t>& ValidActionIds() const = 0;

  // Return true if 'action' is a valid action for this state.
  virtual bool IsValidAction(const Action& action) const = 0;

  // Return the Action associated with a 'str'ing.
  virtual Action StringToAction(const std::string& str) const = 0;

  // Return the string representation for an 'action' that is
  // meaningful for this state.
  virtual std::string ActionToString(const Action& action) const = 0;

  // Return the ID of the player that is active (i.e. that is taking a
  // turn) for this state. Player IDs are derived from the index of
  // the player in the vector of cifre::Player that is provided to the
  // game.
  virtual size_t ActivePlayerId() const = 0;

  // Return the terminal status of the state.
  virtual StateTerminalStatus TerminalStatus() const = 0;

  // Modify this state by applying the action associated with
  // 'action_id'.
  virtual void Update(uint64_t action_id) = 0;

  // Virtual copy constructor. Return an exact copy of this state.
  // Use 'arena' to allocate the memory needed for the copy.
  virtual State* Copy(std::pmr::memory_resource* arena) const = 0;

  // Behaves exactly like Copy() followed by Update(). Derived classes
  // may implement if doing so provides a performance benefit. Return
  // the modified copy.
  virtual State* CopyAndUpdate(
    uint64_t action_id, std::pmr::memory_resource* arena) const {
    State* nb = Copy(arena); nb->Update(action_id); return nb; }

  // Get the representation of the state suitable for feeding to the
  // nnet via triton. This representation does not need to include all
  // of the state, but must include whatever is required by the model
  // being used.
  virtual const FlattenedCells& TritonRepresentation() const = 0;

  // Return string representation of the state.
  virtual std::string ToString() const = 0;
};

} // namespace cifre
