// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <cmath>
#include <memory>
#include <string>
#include <unordered_map>

#include "cifre_state.h"
#include "cifre_triton.h"
#include "common_logger.h"

namespace cifre {

//
// AlphaZero
//
// Implementation of the alpha-zero algorithm. Not necessarily exactly
// the same, but generally uses the same ideas.
//
class AlphaZero {
private:
  //
  // Node representing a state. A node can have multiple parents, the
  // visit counts of an edge from this node to each of the children
  // are tracked in 'children_visit_counts_'. The 'visit_count_' value
  // on the node is the sum of visits from all parents.
  //
  class Node {
  public:
    // Create node for state which model predicts 'infer_result'.
    Node(std::unique_ptr<State> state, TritonInferResult&& infer_result);

    // Return the State represented by this node.
    const State* NodeState() const { return state_.get(); }

    // Return the child for a given action ID.
    Node* Child(uint64_t action_id) const { return children_[action_id]; }

    // Set the child corresponding to 'action_id' to 'child' with
    // 'visit_count' visits on the edge between this node and the
    // child.
    void SetChild(uint64_t action_id, Node* child, size_t visit_count);

    // Increment the visit count on the child edge corresponding to
    // 'action_id'.
    void IncrementChildVisitCount(uint64_t action_id) {
      assert(children_[action_id] != nullptr); children_visit_counts_[action_id]++; }

    // Return 'node's value.
    float Value() const { return value_; }

    // Update node's value by averaging in 'v', return updated value.
    float UpdateValue(float v) {
      value_ = ((value_ * visit_count_) + v) / (visit_count_ + 1); return value_; }

    // Increment node's visit count and return new value.
    size_t IncrementVisitCount() { return ++visit_count_; }

    // Return the best action ID based on the current node state.
    uint64_t BestActionId() const;

  private:
    const std::unique_ptr<const State> state_;
    const TritonInferResult result_;
    std::vector<Node*> children_;
    std::vector<size_t> children_visit_counts_;
    size_t visit_count_;
    float value_;
  };

public:
  // Create alphazero object using 'model-name' for predictions.
  AlphaZero(const std::shared_ptr<common::Logger>& logger, const std::shared_ptr<Triton>& triton,
            const std::string& model_name);

  // Return the next predicted action ID, given current
  // 'state'. Ownership of the 'state' object is transferred to this
  // AlphaZero object.
  uint64_t NextActionId(const State& state);

private:
  uint64_t HashState(const State& state) const;

  // Return the node representing the given 'state', creating if
  // necessary. The created node, if any, takes ownership of 'state'.
  // If 'created' is not nullptr, returns true if node created, false
  // if already exists.
  Node* FindOrCreateNode(std::unique_ptr<State> state, bool* created = nullptr);

  // Search from 'node' and update the tree. Return the 'V' value for
  // the leaf node visited in the search.
  float UpdateTree(Node* node, size_t depth_remaining);

  std::shared_ptr<common::Logger> logger_;
  std::shared_ptr<Triton> triton_;
  const std::string model_name_;
  std::unordered_map<uint64_t, Node> nodes_;
};

} // namespace cifre
