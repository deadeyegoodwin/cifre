// Copyright 2024, David Goodwin. All rights reserved.

#include <iomanip>
#include <limits>
#include <sstream>

#include "cifre_alphazero.h"
#include "common_error.h"

namespace cifre {

//
// AlphaZero
//
AlphaZero::AlphaZero(
  const std::shared_ptr<common::Logger>& logger, const std::shared_ptr<Triton>& triton,
  const std::string& model_name) :
  logger_(logger), triton_(triton), model_name_(model_name), node_update_generation_(0)
{
  THROWIF(model_name_.empty(), "model must be specified for alpha-zero");

  // Load model and make sure it is ready.
  THROWIF(!triton_->LoadModel(model_name_),
          std::string("triton model '" + model_name_ + "' failed to load"));
  THROWIF(!triton_->IsModelReady(model_name_),
          std::string("triton model '" + model_name_ + "' is not ready"));
}

uint64_t
AlphaZero::NextActionId(const State& state)
{
  THROWIF(state.IsTerminal(), "alpha-zero cannot find next action for terminal state");

  // Find, creating if necessary, tree node that represents 'state'.
  auto node = FindOrCreateNode(std::unique_ptr<const State>(state.Clone()));
  assert(node != nullptr);

  // FIXME parameters...
  const size_t search_per_action_count = 50;
  const size_t max_depth = 10;
  const float cpuct = 4.0;

  // Update the tree starting from 'node'.
  for (size_t idx = 0; idx < search_per_action_count; ++idx) {
    UpdateTree(node, cpuct, ++node_update_generation_, max_depth);
  }

  // Choose the action that has the highest visit count. Because
  // 'state' is not a terminal state, there must be at least one
  // action with a non-zero visit count.
  return node->MaxVisitedValidChild();
}

AlphaZero::Node*
AlphaZero::FindOrCreateNode(std::unique_ptr<const State> state, bool* created)
{
  Node* new_node;
  bool new_created;

  auto node_itr = nodes_.find(state.get());
  if (node_itr != nodes_.end()) {
    new_node = &(node_itr->second);
    new_created = false;
  }
  else {
    auto result = triton_->RunModel(model_name_, state->TritonRepresentation());
    THROWIF(result.PiEntryCount() != state->TotalActionIdCount(), "model returned incorrect pi");

    if (state->IsTerminal()) {
      terminal_node_count_++;
    }

    auto created_node_itr =
      nodes_.emplace(std::piecewise_construct,
                     std::forward_as_tuple(state.get()),
                     std::forward_as_tuple(std::move(state), std::move(result)));
    assert(created_node_itr.second);
    new_node = &(created_node_itr.first->second);
    new_created = true;
  }

  if (created != nullptr) {
    *created = new_created;
  }

  assert(new_node != nullptr);
  return new_node;
}

float
AlphaZero::UpdateTree(
  AlphaZero::Node* node, const float cpuct, const size_t generation, const size_t depth_remaining)
{
  // If 'node's generation equals 'generation', then 'node' has
  // already been visited in this search/update so don't update its
  // visit count (it will be updated as the UpdateTree unwinds) and
  // return its value.
  assert(node->Generation() <= generation);
  if (node->Generation() == generation) {
    search_endby_loop_count_++;
    return node->Value();
  }
  node->SetGeneration(generation);

  // If 'node' is the last node that should be visited (because max
  // depth has been reached, or node is a terminal node), then just
  // return value.
  if ((depth_remaining == 0) || node->NodeState().IsTerminal()) {
    if (depth_remaining == 0) {
      search_endby_depth_count_++;
    } else {
      search_endby_terminal_count_++;
    }

    node->IncrementVisitCount();
    return node->Value();
  }

  float v = 0.0;

  // Move down tree following best actions until perform an action
  // that doesn't yet have a child node.
  uint64_t best_action_id = node->BestActionId(cpuct);
  Node* next = node->Child(best_action_id);
  if (next == nullptr) {
    // Find or create the node representing the state that results
    // when 'best_action_id' is applied to 'node'.
    State* next_state = node->NodeState().Clone();
    next_state->Update(best_action_id);
    next = FindOrCreateNode(std::unique_ptr<const State>(next_state));
    assert(next != nullptr);

    // Add the edge to connect 'node' to 'next', then continue search.
    node->SetChild(best_action_id, next, 0 /* visit_count */);
    edge_count_++;
  }

  v = UpdateTree(next, cpuct, generation, depth_remaining - 1);

  node->UpdateValue(v);
  node->IncrementVisitCount();
  node->IncrementChildVisitCount(best_action_id);

  return v;
}

std::string
AlphaZero::ToStatisticsString() const
{
  std::stringstream ss;
  ss << "nodes: " << nodes_.size() << ", internal " << (nodes_.size() - terminal_node_count_)
     << ", terminal " << terminal_node_count_ << '\n';
  ss << "edges: " << edge_count_ << ", avg outgoing per node "
     << std::fixed << std::setprecision(2) << ((float)edge_count_ / nodes_.size()) << '\n';

  const size_t search_count =
    search_endby_depth_count_ + search_endby_loop_count_ + search_endby_terminal_count_;
  ss << "searchs: " << search_count << ", end by depth " << search_endby_depth_count_
     << ", end by loop " << search_endby_loop_count_
     << ", end by terminal " << search_endby_terminal_count_;

  return ss.str();
}

//
// AlphaZero::Node
//
AlphaZero::Node::Node(std::unique_ptr<const State> state, TritonInferResult&& infer_result) :
  state_(std::move(state)), result_(std::move(infer_result)),
  children_(result_.PiEntryCount(), nullptr), children_visit_counts_(result_.PiEntryCount(), 0),
  visit_count_(0), generation_(0)
{
  // Is node is a terminal, then its value is based on the state,
  // otherwise it comes from the model.
  if (state_->IsTerminal()) {
    value_ = state_->TerminalValue();
  }
  else {
    assert(result_.VEntryCount() == 1);
    value_ = result_.V()[0];
  }
}

void
AlphaZero::Node::SetChild(uint64_t action_id, Node* child, size_t visit_count)
{
  assert(child != nullptr);
  assert(children_[action_id] == nullptr);
  children_[action_id] = child;
  children_visit_counts_[action_id] = visit_count;
}

uint64_t
AlphaZero::Node::BestActionId(const float cpuct) const
{
  const float *pi = result_.Pi();

  float max_qu = -std::numeric_limits<float>::max();
  uint64_t best_action_id = std::numeric_limits<uint64_t>::max();

  for (uint64_t aid : state_->ValidActionIds()) {
    float qu;
    Node* child = children_[aid];
    if (child == nullptr) {
      assert(children_visit_counts_[aid] == 0);
      qu = std::sqrt(visit_count_) * pi[aid] * cpuct;
    }
    else {
      qu = (child->value_ +
            (std::sqrt(visit_count_) * pi[aid] * cpuct / children_visit_counts_[aid]));
    }

    if (qu > max_qu) {
      max_qu = qu;
      best_action_id = aid;
    }
  }

  THROWIF(best_action_id == std::numeric_limits<uint64_t>::max(),
          "alpha-zero unable to find action ID");
  return best_action_id;
}

uint64_t
AlphaZero::Node::MaxVisitedValidChild() const
{
  uint64_t max_visit_count = 0;
  uint64_t max_action_id;

  for (uint64_t aid : state_->ValidActionIds()) {
    uint64_t cnt = children_visit_counts_[aid];
    if (cnt > max_visit_count) {
      max_visit_count = cnt;
      max_action_id = aid;
    }
  }

  THROWIF(max_visit_count == 0,
          "alpha-zero unable to find valid child with non-zero visits");

  return max_action_id;
}

} // namespace cifre
