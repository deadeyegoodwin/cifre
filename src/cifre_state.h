// Copyright 2026, David Goodwin. All rights reserved.
#pragma once

#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <memory_resource>
#include <vector>

#include "cifre_action.h"
#include "cifre_cell.h"

namespace cifre {

class State;

//
// StateTerminalStatus
//
// Indicates the terminal status of a State. A state is a terminal if
// if represents an end-game state.
//
class StateTerminalStatus {
public:
  enum class TerminalKind { NON_TERMINAL, WINNER, DRAW };

  static std::string TerminalKindToString(const TerminalKind kind) {
    switch (kind) {
    case TerminalKind::NON_TERMINAL: return "non-terminal";
    case TerminalKind::WINNER: return "winner";
    case TerminalKind::DRAW: return "draw";
    }
    return "<none>"; // unreachable
  }

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

  // Return string representation of the terminal status
  std::string ToString() const {
    return(TerminalKindToString(kind_) +
           ((kind_ == TerminalKind::WINNER) ? ("(p" + std::to_string(winner_id_) + ")") : "")); }

private:
  TerminalKind kind_;
  size_t winner_id_;
};

//
// StateDeleter
//
// Needed to correctly delete State objects allocated into a memory
// arena using placement new (as is commonly done in implementations
// of Copy() and CopyAndUpdate()). For a state object allocated using
// using placement-new with memory allocated by an arena,
// StateDeleter<T> must be used. For a state object allocated using
// "new" and not using placement-new, StateDeleterNewDelete<T> must be
// used.
//
class StateDeleterBase {
public:
  // This default constructor results in an invalid deleter...
  StateDeleterBase() : fn_(nullptr), payload_(nullptr) { }
  void operator()(const void* ptr) const { operator()(const_cast<void*>(ptr)); }
  void operator()(void* ptr) const { assert(fn_ != nullptr); fn_(ptr, payload_); }
protected:
  StateDeleterBase(void (*fn)(void*, void*), void* payload) :
    fn_(fn), payload_(payload) { }
private:
  void (*fn_)(void*, void*);
  void* payload_;
};

template <typename T>
class StateDeleterNewDelete : public StateDeleterBase {
public:
  StateDeleterNewDelete() : StateDeleterBase(Deleter, nullptr) { }
private:
  static void Deleter(void* ptr, void* payload) {
    if (ptr != nullptr) {
      delete static_cast<T*>(ptr);
    }
  }
};

template <typename T>
class StateDeleter : public StateDeleterBase {
public:
  StateDeleter(std::pmr::memory_resource* arena) :
    StateDeleterBase(Deleter, arena) { }
private:
  static void Deleter(void* ptr, void* payload) {
    if (ptr != nullptr) {
      std::pmr::memory_resource* arena = static_cast<std::pmr::memory_resource*>(payload);
      static_cast<T*>(ptr)->~T();
      arena->deallocate(ptr, sizeof(T), alignof(T));
    }
  }
};

using StateUniquePtr = std::unique_ptr<State, StateDeleterBase>;
using StateUniqueConstPtr = std::unique_ptr<const State, StateDeleterBase>;

//
// State
//
// Interface for game state. State represents all mutable status,
// properties, etc. of a game. State is assumed to be non-thread-safe.
//
// The representation of a state given as input to a model is the
// FlattenedCells value returned by TritonRepresentation(). A hash and
// equality function used to compare two states is given by Hash() and
// Equal() respectively. Hash() and Equal() are used by AZ to build
// the search tree; they determine when a new node must be created for
// a given game state or when an existing node should be reused. In
// general, two game states can be considered identical by the model
// (because they have identical FlattenedCells representation) but be
// considered different by AZ because their hash/equal functions
// differentiate on some values other than what is in FlattenedCells.
//
// For many games, all the game state will be encoded in the
// FlattenedCells representation of the state, and so Hash() and
// Equal() can be implemented just as hash/equal of FlattenedCells
// (and also ActivePlayerId for the reason explained below). See
// cifre::Board2D implementation for an example.  For other games, it
// can be necessary for hash/equal to include game state that is not
// presented to the model in FlattenedCells. See biplane_board.h
// description for an example.
//
// The Hash/Equal functions for every game must include ActivePlayerId
// for AZ to operate correctly.  This is needed so that otherwise
// identical states that differ only in ActivePlayerId are treated as
// different nodes by AZ. AZ needs Hash/Equal to have this property to
// correctly detect turn changes.
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
  // properties of std::hash. The hash must include ActivePlayerId for
  // AZ to operate correctly.
  virtual size_t Hash() const = 0;

  // Return true if this state is equal to 'other'. Must satisfy all
  // the properties of std::is_equal. The equality must include
  // ActivePlayerId for AZ to operate correctly.
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
  virtual StateUniquePtr Copy(std::pmr::memory_resource* arena) const = 0;

  // Behaves exactly like Copy() followed by Update(). Derived classes
  // may implement if doing so provides a performance benefit. Return
  // the modified copy.
  virtual StateUniquePtr CopyAndUpdate(
    uint64_t action_id, std::pmr::memory_resource* arena) const {
    auto nb = Copy(arena); nb->Update(action_id); return nb; }

  // Called by Algos (like AZ) to prepare the state for the algorithms
  // selection of the next action-id. By default does nothing but
  // devived classes may override.
  virtual void PrepareForAlgoNextActionId() { };

  // Get the representation of the state suitable for feeding to the
  // nnet via triton. This representation does not need to include all
  // of the state, but must include whatever is required by the model
  // being used.
  virtual const FlattenedCells& TritonRepresentation() const = 0;

  // Return string representation of the state.
  virtual std::string ToString() const = 0;

protected:
  // Utility message to validate that a vector of action IDs are
  // unique. Returns true if unique, false if not unique. If 'dup_str'
  // is non-null returns string representation of the duplicate action
  // IDs.
  bool ValidateUniqueActionIds(
    const std::pmr::vector<uint64_t>& vaids, std::string* dup_str = nullptr) const;
};

//
// StateHash
//
// Functor for hashing States.
//
struct StateHash {
  size_t operator()(StateUniqueConstPtr a) const { return a->Hash(); }
  size_t operator()(const State* a) const { return a->Hash(); }
  size_t operator()(const State& a) const { return a.Hash(); }
};

//
// StateEqual
//
// Functor for comparing States.
//
struct StateEqual {
  bool operator()(StateUniqueConstPtr a, StateUniqueConstPtr b) const {
    return a->Equal(b.get()); }
  bool operator()(const State* a, const State* b) const {
    return a->Equal(b); }
  bool operator()(const State& a, const State& b) const {
    return a.Equal(&b); }
};

} // namespace cifre
