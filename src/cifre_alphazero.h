// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "cifre_state.h"
#include "cifre_triton.h"
#include "common_logger.h"
#include "common_random.h"

namespace cifre {

//
// AlphaZeroSettings
//
// Settings for alphazero. Default settings are for non-training use
// of alphazero.
//
struct AlphaZeroSettings {
  // True if training data should be collected.
  bool training_ {false};

  // Options for how NextActionId can choose an action ID.
  //   MAX_VISITED - choose action ID corresponding to max visited child
  //   RANDOM_VISIT_WEIGHTED - choose randomly taking into account
  //   child visits so that the more visited children are
  //   proportionally more likely to be selected
  enum class NextActionKind { MAX_VISITED, RANDOM_VISIT_WEIGHTED };
  NextActionKind next_action_kind_ {NextActionKind::MAX_VISITED};

  // Dirichlet alpha value, must be non-negative. Zero indicates
  // that dirichlet distribution should not be used (this is the
  // default because when not training the default is to not use
  // Dirichlet).
  float alphazero_dirichlet_alpha_ {0.0};

  // Number of Dirichlet samples that will be generated (if
  // 'alphazero_dirichlet_alpha_' is non-zero). These values are
  // used round-robin to modify the pi values of root nodes.
  size_t alphazero_dirichlet_sample_count_ {1111};

  // The ratio used to combine the Dirichlet values to the pi
  // probabilities. If d == dirichlet value and x ==
  // alphazero_dirichlet_ratio_, the probability used is (pi * (1 -
  // x)) + (d * x).
  float alphazero_dirichlet_ratio_ {0.25};

  // Number of updates (searches) made to alphazero tree before
  // choosing the next action ID.
  size_t alphazero_update_count_ {50};

  // Maximum depth alphazero will traverse tree for each
  // search/update. Once this depth is reached the game is
  // considered a draw and the tree is updated
  // appropriately. Therefore, this depth should be set to match the
  // rules of the game.
  size_t alphazero_max_depth_ {20};

  // The cpuct value for alphazero QU calculation. The non-training
  // default is 1.0.
  float alphazero_cpuct_ {1.0};
};

//
// AlphaZeroStatistics
//
class AlphaZeroStatistics {
public:
  // Add statistics
  AlphaZeroStatistics& operator+=(const AlphaZeroStatistics& rhs);

  // Reset all statistics.
  void Reset();

  // Return string representation of statistics.
  std::string ToString() const;

  size_t total_node_count_ {0};
  size_t terminal_node_count_ {0};
  size_t edge_count_ {0};
  size_t search_endby_depth_count_ {0};
  size_t search_endby_terminal_count_ {0};
};

//
// AlphaZero
//
// Node representing a state. A node can have multiple parents, the
// visit counts of an edge from this node to each of the children are
// tracked in 'children_visit_counts_'. The 'visit_count_' value on
// the node is the sum of visits from all parents. AlphaZero maintains
// the invariant that (except for terminal nodes that have no
// children) the node's visit count equals 1 + (sum of the visit
// counts of the children). This occurs because when a node is created
// it is given a visit count of 1, but no child are visited.
// Thereafter, every time the node is visited a child must be
// visited. ValidateVisitCounts() can be used to validate this
// property. Note that the "inverse" is not true... namely the sum of
// all children visit counts "entering" a node does not equal a node's
// visit count (because when max depth is reached, the node that
// reaches that depth is not counted as visited, even though the
// incoming child edge is counted as visited).
//
class AlphaZeroNode {
public:
  // Create node for state which model predicts 'infer_result'.
  AlphaZeroNode(std::unique_ptr<const State> state, TritonInferResult&& infer_result);

  // Is node a terminal?
  bool IsTerminal() const {
    return state_->TerminalStatus() != State::TerminalStatusKind::NON_TERMINAL; }

  // Return the State represented by this node.
  const State& NodeState() const { return *state_; }

  // Return the Triton result object associated with this node.
  const TritonInferResult& NodeTritonResult() const { return result_; }

  // Return the child for a given action ID.
  AlphaZeroNode* Child(uint64_t action_id) const { return children_[action_id]; }

  // Set the child corresponding to 'action_id' to 'child' with
  // 'visit_count' visits on the edge between this node and the
  // child.
  void SetChild(uint64_t action_id, AlphaZeroNode* child, size_t visit_count);

  // Increment the visit count on the child edge corresponding to
  // 'action_id'.
  void IncrementChildVisitCount(uint64_t action_id) {
    assert(children_[action_id] != nullptr); children_visit_counts_[action_id]++; }

  // Return the child probabilities, calculated from the node visit
  // count and the children visit counts.
  std::vector<float> ChildProbability() const;

  // Return and set 'node's value.
  float Value() const { return value_; }
  void SetValue(float v) { value_ = v; }

  // Return node's visit count.
  size_t VisitCount() const { return visit_count_; }

  // Increment node's visit count and return new value.
  size_t IncrementVisitCount() { return ++visit_count_; }

  // Validate that the node's visit count agrees with the children
  // visit counts as explained above.
  void ValidateVisitCounts() const;

  // Return the ID of the action that has the highest QU value. If
  // 'qu's is non-nullptr then it will be filled with the QU value
  // for each action/child.
  std::optional<uint64_t> MaxQU(
    const float cpuct, const std::vector<double>* dirichlet,
    const float dirichlet_ratio, std::vector<float>* qus = nullptr) const;

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

  // Return string representation of node.
  std::string ToString(
    float cpuct, common::Logger::Level filter_level = common::Logger::Level::LOW) const;

private:
  const std::unique_ptr<const State> state_;
  const TritonInferResult result_;
  std::vector<AlphaZeroNode*> children_;
  std::vector<size_t> children_visit_counts_;
  size_t visit_count_;
  float value_;
};

//
// AlphaZero
//
// Implementation of the alpha-zero algorithm. Not necessarily exactly
// the same, but generally uses the same ideas.
//
class AlphaZero {
public:
  // Create alphazero object using 'model-name' for predictions.
  AlphaZero(const std::shared_ptr<common::Logger>& logger, const std::shared_ptr<Triton>& triton,
            const AlphaZeroSettings& settings, const std::string& model_name);

  // Return the statistics for this alphazero.
  const AlphaZeroStatistics& Statistics() const { return statistics_; }

  // Return the collected training samples.
  const std::vector<std::pair<std::shared_ptr<AlphaZeroNode>, float>>&
  TrainingSamples() const { return train_nodes_; }

  // Reset the node tree and related statistics. After returning this
  // alphazero object will begin constructing a new tree from
  // scratch.
  void Reset();

  // Called to notify that a new game is starting.
  void GameStart();

  // Called to notify that game has ended at 'terminal_state' with a
  // terminal 'status'. Note the 'status' may differ from the actual
  // terminal status of 'terminal_state', but any end-game actions
  // should respect 'status'. 'status' of NON_TERMINAL is not allowed
  // and GameEnd should throw exception if that value is received.
  void GameEnd(const State* terminal_state, State::TerminalStatusKind status);

  // Return the next predicted action ID, given current 'state'. If is
  // an error if 'state' represents a terminal state.
  uint64_t NextActionId(const State& state);

private:
  // Return the node representing the given 'state', creating if
  // necessary. The created node, if any, takes ownership of 'state'.
  // If 'created' is not nullptr, returns true if node created, false
  // if already exists.
  std::shared_ptr<AlphaZeroNode> FindOrCreateNode(
    std::unique_ptr<const State> state, bool* created = nullptr);

  // Search from 'node' and update the tree. Return the 'V' value for
  // the leaf node visited in the search.
  float UpdateTree(
    AlphaZeroNode* node, const size_t depth_remaining, const std::vector<double>* dirichlet);

  // Generate Dirichlet distribution.
  void GenerateDirichletSamples(
    const float alpha, const size_t sample_size, const size_t sample_count);

  std::shared_ptr<common::Logger> logger_;
  std::shared_ptr<Triton> triton_;
  const AlphaZeroSettings settings_;
  const std::string model_name_;

  // Random needed for NextActionKind::WEIGHTED_RANDOM.
  common::Random rnd_;

  // Number of Dirichlet samples (generated if
  // 'settings_.alphazero_dirichlet_alpha_' is non-zero). These values
  // are used round-robin to modify the pi values of root nodes.
  size_t dirichlet_samples_idx_;
  std::vector<std::vector<double>> dirichlet_samples_;

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
  std::unordered_map<
    const State*, std::shared_ptr<AlphaZeroNode>, NodesMapHash, NodesMapEqual> nodes_;

  // Nodes for which alphazero selected an action in the current game.
  std::vector<std::shared_ptr<AlphaZeroNode>> game_action_nodes_;

  // Aggregated nodes from the 'game_action_nodes_' collected for each
  // game, and the value associated with the node.
  std::vector<std::pair<std::shared_ptr<AlphaZeroNode>, float>> train_nodes_;

  // Statistics, these are reset by a call to Reset().
  AlphaZeroStatistics statistics_;
};

//
// AlphaZero
//
// Implementation of the alpha-zero algorithm. Not necessarily exactly
// the same, but generally uses the same ideas.
//
class AlphaZeroAggregator {
public:
  AlphaZeroAggregator(const std::shared_ptr<common::Logger>& logger) :
    logger_(logger), aggregate_count_(0) { }

  // Include 'alphazero' training and statistics in the aggregate.
  void Aggregate(const AlphaZero& alphazero);

  // Write collected training data to 'dir'.
  void WriteTraining(const std::string& dir) const;

  // Return string representation of aggregated statistics.
  std::string ToString() const;

private:
  // Return string representation of a shape, using 'batch_count' as
  // the first/batch dimension value if it is non-zero.
  std::string ToJsonArray(const std::vector<int64_t> shape, size_t batch_count) const;

  std::shared_ptr<common::Logger> logger_;
  size_t aggregate_count_;
  AlphaZeroStatistics statistics_;
  std::vector<std::pair<std::shared_ptr<AlphaZeroNode>, float>> train_samples_;
};

} // namespace cifre
