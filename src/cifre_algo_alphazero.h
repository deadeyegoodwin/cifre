// Copyright 2025, David Goodwin. All rights reserved.
#pragma once

#include <atomic>
#include <cmath>
#include <deque>
#include <memory>
#include <memory_resource>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cifre_algo.h"
#include "cifre_logger.h"
#include "cifre_state.h"
#include "cifre_triton.h"
#include "common_random.h"

namespace cifre {

class AlphaZeroNode;

//
// AlphaZeroSettings
//
// Settings for alphazero. Default settings are for non-training use
// of alphazero, but the assumption is that these will be changed by
// command-line and specific game requirements.
//
struct AlphaZeroSettings {
  // True if training data should be collected.
  bool training_ {false};

  // Name of the nnet model to use for predictions.
  std::string model_name_;

  // Options for how NextActionId can choose an action ID. This
  // setting takes the place of temperature in the original AlphaZero
  // but is less flexible.
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
  float dirichlet_alpha_ {0.0};

  // Number of Dirichlet samples that will be generated (if
  // 'dirichlet_alpha_' is non-zero). These values are used
  // round-robin to modify the pi values of root nodes.
  size_t dirichlet_sample_count_ {1111};

  // The ratio used to combine the Dirichlet values to the pi
  // probabilities. If d == dirichlet value and x ==
  // 'dirichlet_ratio_', the probability used is (pi * (1 - x)) + (d *
  // x).
  float dirichlet_ratio_ {0.25};

  // Number of searches made to alphazero tree before choosing the
  // next action ID.
  size_t search_count_ {50};

  // Maximum depth alphazero will traverse tree for each search. Once
  // this depth is reached the search stops and the current node value
  // is used to update the tree.
  size_t max_search_depth_ {20};

  // The cpuct value for alphazero QU calculation. The non-training
  // default is 1.0.
  float cpuct_ {1.0};
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

  std::atomic<size_t> total_node_count_ {0};
  std::atomic<size_t> terminal_node_count_ {0};
  std::atomic<size_t> edge_count_ {0};
  std::atomic<size_t> search_count_ {0};
  std::atomic<size_t> search_endby_search_depth_count_ {0};
  std::atomic<size_t> search_endby_game_depth_count_ {0};
  std::atomic<size_t> search_endby_terminal_count_ {0};
  std::atomic<size_t> cycle_avoided_count_ {0};
};

//
// AlphaZeroNode
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
// visit count (because when a max depth is reached, the node that
// reaches that depth is not counted as visited, even though the
// incoming child edge is counted as visited).
//
class AlphaZeroNode {
public:
  // Create node for 'state'.
  AlphaZeroNode(
    std::pmr::memory_resource& arena, StateUniqueConstPtr state,
    std::shared_ptr<const TritonInferResult> infer_result);

  // Is node a terminal?
  bool IsTerminal() const {
    return state_->TerminalStatus().Kind() != StateTerminalStatus::TerminalKind::NON_TERMINAL; }

  // Return the State represented by this node.
  const State& NodeState() const { return *state_; }

  // Return the Triton result object associated with this node.
  const std::shared_ptr<const TritonInferResult>& NodeTritonResult() const { return result_; }

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

  // Return and set 'node's generation.
  size_t Generation() const { return generation_; }
  void SetGeneration(size_t g) { generation_ = g; }

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
    const size_t generation, const float cpuct, const std::vector<double>* dirichlet,
    const float dirichlet_ratio, size_t* cycle_avoided_count = nullptr,
    std::vector<float>* qus = nullptr, std::vector<float>* nocycle_qus = nullptr) const;

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

  // Return string representation of node. If 'cpuct' is non-zero then
  // show the QUs for the node.
  std::string ToString(float cpuct = 0.0) const;

private:
  StateUniqueConstPtr state_;
  std::shared_ptr<const TritonInferResult> result_;
  std::pmr::vector<AlphaZeroNode*> children_;      // fully sized at construction
  std::pmr::vector<size_t> children_visit_counts_; // fully sized at construction
  size_t visit_count_;
  float value_;
  size_t generation_;
};

//
// AlphaZeroTrainingSamples
//
// Collects training samples from one or more AlphaZeroAlgo objects.
//
class AlphaZeroTrainingSamples {
public:
  //
  // A training sample generated by AlphaZero.
  //
  class Sample {
  public:
    Sample(
      StateUniqueConstPtr s, const std::shared_ptr<const TritonInferResult>& tr,
      std::vector<float>&& pi, float azv) :
      state_(std::move(s)), triton_result_(tr), z_value_(0),
      az_value_(azv), target_pi_(std::move(pi)), target_v_(0), count_(1) { }

    Sample(
      StateUniqueConstPtr s, const std::shared_ptr<const TritonInferResult>& tr,
      float z_value, float az_value, std::vector<float>&& pi, float v) :
      state_(std::move(s)), triton_result_(tr), z_value_(z_value), az_value_(az_value),
      target_pi_(std::move(pi)), target_v_(v), count_(1) { }

    const State& SampleState() const { return *state_; }

    // Set the game-outcome value.
    void SetZValue(float f) { z_value_ = f; }

    // Return string representation of Sample.
    std::string ToString() const;

  private:
    friend class AlphaZeroTrainingSamples;

    StateUniqueConstPtr state_;
    std::shared_ptr<const TritonInferResult> triton_result_;

    // Game outcome value. If AggregateKind is AVERAGE, then this is
    // the outcome value for the first instance of this state that is
    // seen.
    float z_value_;

    // Value calculated by alphazero.
    float az_value_;

    // The target pi and v values that should be trained for based on
    // this sample.
    std::vector<float> target_pi_;
    float target_v_;

    // Number of training nodes represented by this sample.
    size_t count_;
  };

public:
  // Options for how training samples for the same node are
  // aggregated.
  //   ALL - A training sample is generated for every node, even if
  //   that results in multiple training samples for the same node.
  //   AVERAGE - The value and probability values for the training
  //   samples for a given node are averaged and a single training
  //   sample for all the nodes is kept using those average value and
  //   probability values.
  enum class AggregateKind { ALL, AVERAGE };

  // Options for how training sample target value is calculated. Z is
  // the game-outcome value (-1, 0, 1) assigned to the node. V is the
  // alphazero calculated value.
  //   Z - Training sample target value is set to game outcome value.
  //   V - Training sample target value is set to alphazero calculated value.
  //   AVERAGE - Training sample value is set to average of Z and V.
  enum class ValueKind { Z, V, AVERAGE };

  static std::string AggregateKindToString(const AggregateKind kind) {
    switch (kind) {
    case AggregateKind::ALL: return "ALL";
    case AggregateKind::AVERAGE: return "AVERAGE";
    }
    return "<none>"; // unreachable
  }

  static std::string ValueKindToString(const ValueKind kind) {
    switch (kind) {
    case ValueKind::Z: return "Z";
    case ValueKind::V: return "V";
    case ValueKind::AVERAGE: return "AVERAGE";
    }
    return "<none>"; // unreachable
  }

public:
  AlphaZeroTrainingSamples(
    const std::shared_ptr<Logger>& logger, AggregateKind aggregate_kind,
    ValueKind value_kind) :
    logger_(logger), aggregate_kind_(aggregate_kind),
    value_kind_(value_kind) { }

  // Transfer ownership of a vector of 'samples' into 'this' set of
  // samples. The 'samples' are aggregated into any existing samples.
  void AcquireAndAggregate(std::deque<Sample>&& samples);

  // Write collected training data to 'dir'.
  void WriteTraining(const std::string& dir) const;

  // Remove all samples.
  void Reset();

  // Return string representation of Samples.
  std::string ToString() const;

private:
  // Return string representation of a shape, using 'batch_count' as
  // the first/batch dimension value if it is non-zero.
  std::string ToJsonArray(const std::vector<int64_t> shape, size_t batch_count) const;

  std::shared_ptr<Logger> logger_;
  const AggregateKind aggregate_kind_;
  const ValueKind value_kind_;

  // Samples are identified by their State, so we need to use a custom
  // hash and equal for the map containing indices into
  // 'train_samples_'. The "State*" key is a pointer to the same state
  // that is in the Sample.
  struct SamplesIndexMapHash {
    size_t operator()(const State* s) const { return s->Hash(); }
  };
  struct SamplesIndexMapEqual {
    bool operator()(const State* a, const State* b) const { return a->Equal(b); }
  };
  std::unordered_map<
     const State*, size_t, SamplesIndexMapHash, SamplesIndexMapEqual> train_samples_idx_;
  std::deque<Sample> train_samples_;
};

//
// AlphaZeroAlgo
//
// Implementation of the alpha-zero algorithm. Not necessarily exactly
// the same, but generally uses the same ideas.
//
class AlphaZeroAlgo : public Algo {
public:
  // Create alphazero object using 'settings'.
  AlphaZeroAlgo(
    const std::shared_ptr<Logger>& logger, std::unique_ptr<TritonModel> triton_model,
    const AlphaZeroSettings& settings);

  // Return the statistics for this alphazero.
  const AlphaZeroStatistics& Statistics() const { return statistics_; }

  // See cifre::Algo. Called to notify that a new game is
  // starting. 'this' alphazero object is reset in preparation of the
  // new game.
  void GameStart() override;

  // See cifre::Algo. Called to notify that game has ended at
  // 'terminal_state' with a terminal 'status'. Note the 'status' may
  // differ from the actual terminal status of 'terminal_state', but
  // any end-game actions should respect 'status'. 'status' of
  // NON_TERMINAL is not allowed and GameEnd should throw exception if
  // that value is received.  If 'samples' is non-nullptr, then the
  // collected AlphaZeroTrainingSamples::Samples are returned. Lastly,
  // 'this' alphazero object is reset.
  void GameEnd(const State& terminal_state, const StateTerminalStatus& status) override {
    GameEnd(terminal_state, status, nullptr); }

  // Version of GameEnd that also collects training samples. If
  // 'samples' is non-nullptr, then the collected
  // AlphaZeroTrainingSamples::Samples are returned. Lastly, 'this'
  // alphazero object is reset.
  void GameEnd(
    const State& terminal_state, const StateTerminalStatus& status,
    std::deque<AlphaZeroTrainingSamples::Sample>* samples);

  // See cifre::Algo. Any search that reaches max-sideturns will be
  // declared a DRAW at that point and will update the tree
  // appropriately.
  uint64_t NextActionId(const State& state, size_t max_game_sideturns_remaining) override;

private:
  struct NodesMapHash {
    size_t operator()(const State* s) const { return s->Hash(); }
  };
  struct NodesMapEqual {
    bool operator()(const State* a, const State* b) const { return a->Equal(b); }
  };
  using NodesIterator = std::unordered_map<
    const State*, AlphaZeroNode, NodesMapHash, NodesMapEqual>::iterator;

  // Return the node representing the given 'state', creating if
  // necessary. The created node, if any, takes ownership of 'state'.
  // If 'created' is not nullptr, returns true if node created, false
  // if already exists.
  NodesIterator FindOrCreateNode(StateUniqueConstPtr, bool* created = nullptr);

  // Search from 'node' and update the tree. Return the 'V' value for
  // the leaf node visited in the search, and the node that terminated
  // the search (could be an actual terminal node or just the last
  // visited node before game or search depth was reached).
  std::pair<float, AlphaZeroNode*> SearchTree(
    AlphaZeroNode* node, const size_t search_depth_remaining,
    const size_t game_depth_remaining, const std::vector<double>* dirichlet);

  // Generate Dirichlet distribution.
  void GenerateDirichletSamples(
    const float alpha, const size_t sample_size, const size_t sample_count);

  std::shared_ptr<Logger> logger_;
  std::unique_ptr<TritonModel> triton_model_;
  const AlphaZeroSettings settings_;

  // Random needed for NextActionKind::WEIGHTED_RANDOM.
  common::Random rnd_;

  // Number of Dirichlet samples (generated if
  // 'settings_.dirichlet_alpha_' is non-zero). These values are used
  // round-robin to modify the pi values of root nodes.
  size_t dirichlet_samples_idx_;
  std::vector<std::vector<double>> dirichlet_samples_;

  // Monotonic arena used for 'nodes_' and the corresponding state
  // created when building the search-tree for NextActionId(). This
  // arena is reset on each call to NextActionId() and so nothing can
  // be allocated to it that needs to live beyond an invocation of
  // that function.
  std::pmr::monotonic_buffer_resource nextactionid_arena_buffer_;
  std::pmr::unsynchronized_pool_resource nextactionid_arena_;

  // Nodes are identified by their State, so we need to use a custom
  // hash and equal for the map containing all the nodes. The "State*"
  // key is a pointer to the same state that is owned by the
  // unique_ptr<State> in the Node itself. 'nodes_' is reset on each
  // call to NextActionId() and so nothing can be allocated to it that
  // needs to live beyond an invocation of that function.
  std::pmr::unordered_map<
    const State*, AlphaZeroNode, NodesMapHash, NodesMapEqual> nodes_;

  // Nodes for which alphazero selected an action in the current game.
  std::deque<AlphaZeroTrainingSamples::Sample> game_action_samples_;

  // Statistics...
  AlphaZeroStatistics statistics_;
};

} // namespace cifre
