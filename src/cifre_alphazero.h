// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <cmath>
#include <memory>
#include <string>
#include <unordered_map>

#include "cifre_state.h"
#include "cifre_triton.h"
#include "common_logger.h"
#include "common_random.h"

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
    Node(std::unique_ptr<const State> state, TritonInferResult&& infer_result);

    // Return the State represented by this node.
    const State& NodeState() const { return *state_; }

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

    // Get or set the search/update generation of this node.
    size_t Generation() const { return generation_; }
    void SetGeneration(size_t g) { generation_ = g; }

    // Return the best action ID based on the current node state.
    uint64_t BestActionId(const float cpuct) const;

    // Return the action ID corresponding to the child edge that has
    // the highest non-zero visited count and that represents a valid
    // action for this node. Error if no such child.
    uint64_t MaxVisitedValidChild() const;

    // Return the action ID corresponding to the child edge that
    // represents a valid action for this node, and that is chosen
    // randomly from all valid children based on the child visit
    // counts, so that the higher visited children have a higher
    // probability of being chosen randomly. Error if no such child.
    uint64_t RandomWeightedValidChild(common::Random& rnd) const;

  private:
    const std::unique_ptr<const State> state_;
    const TritonInferResult result_;
    std::vector<Node*> children_;
    std::vector<size_t> children_visit_counts_;
    size_t visit_count_;
    float value_;

    // Used to determine when a search/update of the node tree
    // traverses a loop.
    size_t generation_;
  };

public:
  // Options for how NextActionId can choose an action ID.
  //   BEST - choose action ID corresponding to max visited child
  //   WEIGHTED_RANDOM - weighted by child probabilities, choose randomly
  enum class NextActionKind { VISIT, WEIGHTED_RANDOM };

  // Create alphazero object using 'model-name' for predictions.
  AlphaZero(const std::shared_ptr<common::Logger>& logger, const std::shared_ptr<Triton>& triton,
            const std::string& model_name);

  // Return the next predicted action ID, given current 'state'. If is
  // an error if 'state' represents a terminal state.
  uint64_t NextActionId(const State& state, const NextActionKind kind);

  // Return a string representation of the statistics collected for
  // this alphazero object.
  std::string ToStatisticsString() const;

private:
  // Return the node representing the given 'state', creating if
  // necessary. The created node, if any, takes ownership of 'state'.
  // If 'created' is not nullptr, returns true if node created, false
  // if already exists.
  Node* FindOrCreateNode(std::unique_ptr<const State> state, bool* created = nullptr);

  // Search from 'node' and update the tree. Return the 'V' value for
  // the leaf node visited in the search.
  float UpdateTree(
    Node* node, const float cpuct, const size_t generation, const size_t depth_remaining);

  std::shared_ptr<common::Logger> logger_;
  std::shared_ptr<Triton> triton_;
  const std::string model_name_;

  // Random needed for NextActionKind::WEIGHTED_RANDOM.
  common::Random rnd_;

  // Used to determine when a search/update of the node tree
  // traverses a loop.
  size_t node_update_generation_;

  // Nodes are identified by their State, so we need to use a custom
  // hash and equal for the map containing all the nodes. The "State*"
  // key is a pointer to the same state that is owned by the
  // unique_ptr<State> in the Node itself.
  struct NodesMapHash {
    size_t operator()(const State* s) const { return s->Hash(); }
  };
  struct NodesMapEqual {
    bool operator()(const State* a, const State* b) const { return a->Equal(b); }
  };
  std::unordered_map<const State*, Node, NodesMapHash, NodesMapEqual> nodes_;

  // Statistics
  size_t terminal_node_count_ {0};
  size_t edge_count_ {0};
  size_t search_endby_depth_count_ {0};
  size_t search_endby_loop_count_ {0};
  size_t search_endby_terminal_count_ {0};
};

} // namespace cifre
