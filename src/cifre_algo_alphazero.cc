// Copyright 2025, David Goodwin. All rights reserved.

#include <filesystem>
#include <iomanip>
#include <limits>
#include <sstream>

#include "gsl/gsl_rng.h"
#include "gsl/gsl_randist.h"
#include "cifre_algo_alphazero.h"
#include "common_error.h"
#include "common_util.h"

namespace cifre {

constexpr float DRAW_VALUE = 0.0;
constexpr float WINNER_VALUE = 1.0;
constexpr float LOSER_VALUE = -1.0;

constexpr size_t ARENA_INITIAL_BYTES = 1024 * 1024;

//
// AlphaZeroAlgo
//
AlphaZeroAlgo::AlphaZeroAlgo(
  const std::shared_ptr<Logger>& logger, std::unique_ptr<TritonModel> triton_model,
  const AlphaZeroSettings& settings) :
  logger_(logger), triton_model_(std::move(triton_model)), settings_(settings),
  nextactionid_arena_buffer_(ARENA_INITIAL_BYTES),
  nextactionid_arena_(&nextactionid_arena_buffer_),
  nodes_(&nextactionid_arena_)
{
  THROWIF(triton_model_->Name() != settings_.model_name_,
          "alphazero settings model-name differs from triton model name");

  // Model must be ready...
  THROWIF(!triton_model_->IsReady(),
          std::string("triton model '" + triton_model_->Name() + "' is not ready"));

  // Make sure Dirichlet settings are valid.
  THROWIF(settings_.dirichlet_alpha_ < 0.0, "dirichlet alpha value must be >= 0.0");
  THROWIF(settings_.dirichlet_sample_count_ <= 0, "dirichlet sample count must be > 0");
  THROWIF((settings_.dirichlet_ratio_ <= 0.0) || (settings_.dirichlet_ratio_ >= 1.0),
          "dirichlet ratio must be in exclusive range (0.0, 1.0)");
}

void
AlphaZeroAlgo::GameStart()
{
  game_action_samples_.clear();
  statistics_.Reset();
}

void
AlphaZeroAlgo::GameEnd(const State& terminal_state, const StateTerminalStatus& status,
                   std::deque<AlphaZeroTrainingSamples::Sample>* samples)
{
  THROWIF(status.Kind() == cifre::StateTerminalStatus::TerminalKind::NON_TERMINAL,
          "game ended with NON-TERMINAL status");

  // If 'samples' is non-nullptr, then append all the collected
  // training samples to 'samples'. At this point each training sample
  // consists of just the state and the z-value (game outcome value).
  if (samples != nullptr) {
    for (auto& s : game_action_samples_) {
      // Game outcome value (Z).
      float z_value = DRAW_VALUE;
      switch (status.Kind()) {
      case StateTerminalStatus::TerminalKind::NON_TERMINAL:
      case StateTerminalStatus::TerminalKind::DRAW:
        break;
      case StateTerminalStatus::TerminalKind::WINNER:
        const size_t winner_id = status.WinnerId();
        z_value =
          (s.SampleState().ActivePlayerId() == winner_id) ? WINNER_VALUE : LOSER_VALUE;
        break;
      }

      s.SetZValue(z_value);
      samples->emplace_back(std::move(s));
    }
  }

  game_action_samples_.clear();
}

uint64_t
AlphaZeroAlgo::NextActionId(const State& state, size_t max_game_sideturns_remaining)
{
  THROWIF(state.TerminalStatus().Kind() != StateTerminalStatus::TerminalKind::NON_TERMINAL,
          "alpha-zero cannot find next action for terminal state");

  // For each search performed, the nodes visited in that search are
  // tagged with a generation so that loops/cycles can be
  // detected. Start at 1 so we can use generation==0 to indicate "no
  // generation". We are not worried about wrap-around for a 64-bit
  // value.
  static std::atomic<size_t> global_generation(1);

  // Reset the nodes and the arena... a new node tree is used to
  // choose this action.
  nodes_.clear();
  nextactionid_arena_.release();

  // If Dirichlet distribution is being used but has not yet been
  // generated, then do so now using GSL (GNU Scientific
  // Library). This is done lazily because a State object is needed to
  // get the total number of action IDs.
  if (settings_.dirichlet_alpha_ > 0.0) {
    if (dirichlet_samples_.empty()) {
      GenerateDirichletSamples(settings_.dirichlet_alpha_, state.TotalActionIdCount(),
                               settings_.dirichlet_sample_count_);
    }

    assert(dirichlet_samples_.size() == settings_.dirichlet_sample_count_);
  }

  // Find, creating if necessary, the tree node that represents 'state'.
  auto node_itr = FindOrCreateNode(state.Copy(&nextactionid_arena_));
  auto& node = node_itr->second;

  LOG_ALGO(*logger_, "\nalphazero: starting node");
  LOG_ALGO_IE(
    *logger_, node.ToString(settings_.cpuct_), 2 /* indent */, true /* endline */);

  // Update the tree by performing searches starting from 'node'.
  for (size_t idx = 0; idx < settings_.search_count_; ++idx) {
    // Get a unique generation value for this search. Assign the value
    // to the starting node; in SearchTree we also assign the value to
    // visited nodes and use it to detect cycles.
    const size_t search_generation = global_generation.fetch_add(1);
    node.SetGeneration(search_generation);

    // Use dirichlet if enabled...
    const std::vector<double>* dirichlet = nullptr;
    if (settings_.dirichlet_alpha_ > 0.0) {
      dirichlet = &dirichlet_samples_[dirichlet_samples_idx_];
      dirichlet_samples_idx_ = (dirichlet_samples_idx_ + 1) % dirichlet_samples_.size();
    }

    LOG_ALGO_IE(
      *logger_,
      "search " + std::to_string(idx+1) + '/' + std::to_string(settings_.search_count_) + ':',
      0 /* indent */, false /* endline */);
    auto [v, tnode] = SearchTree(
      &node, settings_.max_search_depth_, max_game_sideturns_remaining, dirichlet);
    LOG_ALGO_IE(*logger_, "\n", 0 /* indent */, false /* endline */);
    if (tnode != nullptr) {
      LOG_ALGO_IE(*logger_, tnode->ToString(settings_.cpuct_),
                  2 /* indent */, true /* endline */);
    }
  }

  statistics_.search_count_ += settings_.search_count_;

  // Based on 'kind', choose the next action. Because 'state' is not a
  // terminal state, there must be at least one action with a non-zero
  // visit count.
  uint64_t action_id;
  switch (settings_.next_action_kind_) {
  case AlphaZeroSettings::NextActionKind::MAX_VISITED:
    action_id = node.MaxVisitedValidChild();
    break;
  case AlphaZeroSettings::NextActionKind::RANDOM_VISIT_WEIGHTED:
    action_id = node.RandomWeightedValidChild(rnd_);
    break;
  default:
    THROW("unreachable");
  }

  LOG_ALGO(
    *logger_, "\nalphazero: choose action " + std::to_string(action_id) +
    " '" + node.NodeState().ActionToString(Action(Action::ActionKind::ACTIONID, action_id)) + "'");
  LOG_ALGO_IE(*logger_, node.ToString(), 2 /* indent */, true /* endline */);

  // If training, record the nodes for which alpha-zero makes an
  // action decision.
  if (settings_.training_) {
    // Create a sample for the just-chosen action for 'node'. Anything
    // needed from 'node' must be copied out of 'nextactionid_arena_'
    // and into standard memory because 'node' does not persist beyond
    // this call.
    auto node_probs = node.ChildProbability();
    assert((node_probs.size() * sizeof(float)) == node.NodeTritonResult()->PiByteSize());
    game_action_samples_.emplace_back(
      node.NodeState().Copy(std::pmr::new_delete_resource()),
      node.NodeTritonResult(), std::move(node_probs), node.Value());
  }

  nodes_.clear();
  nextactionid_arena_.release();

  return action_id;
}

AlphaZeroAlgo::NodesMap::iterator
AlphaZeroAlgo::FindOrCreateNode(StateUniqueConstPtr state, bool* created)
{
  auto fnode_itr = nodes_.find(state.get());
  if (fnode_itr != nodes_.end()) {
    if (created != nullptr) {
      *created = false;
    }
    return fnode_itr;
  }

  auto result = triton_model_->Infer(*state);
  assert(result->PiEntryCount() == state->TotalActionIdCount());

  const State* pstate = state.get();

  auto [node_itr, ok] = nodes_.emplace(
    std::piecewise_construct,
    std::forward_as_tuple(pstate),
    std::forward_as_tuple(nextactionid_arena_, std::move(state), std::move(result)));
  assert(ok);

  AlphaZeroNode& new_node = node_itr->second;

  // Node creation needs to be recognized as the first visit.
  new_node.IncrementVisitCount();

  statistics_.total_node_count_++;
  if (new_node.IsTerminal()) {
    statistics_.terminal_node_count_++;
  }

  if (created != nullptr) {
    *created = true;
  }

  return node_itr;
}

std::pair<float, AlphaZeroNode*>
AlphaZeroAlgo::SearchTree(
  AlphaZeroNode* node, const size_t search_depth_remaining,
  const size_t game_depth_remaining, const std::vector<double>* dirichlet)
{
  // If 'node' is a terminal node then show that it was visited and
  // return its value.
  if (node->IsTerminal()) {
    LOG_ALGO_IE(
      *logger_, " terminal(p" + std::to_string(node->NodeState().ActivePlayerId()) +
      ") >>" + std::to_string(node->Value()) + ">> " + std::to_string(node->Value()),
      0 /* indent */, false /* endline */);
    statistics_.search_endby_terminal_count_++;
    node->IncrementVisitCount();
    return std::make_pair(node->Value(), node);
  }

#ifndef NDEBUG
  // Non-terminal nodes must maintain correctness between 'node' visit
  // count and children visit counts.
  node->ValidateVisitCounts();
#endif

  // If game depth has been reached then assume game is a draw. If
  // search depth is reached then return 'node's value. Don't
  // increment the node count when stopping for depth, this is because
  // we always want the node count to equal the sum of the child
  // counts so if we are not going to visit a child we don't want to
  // update the node count.
  if (game_depth_remaining == 0) {
    LOG_ALGO_IE(
      *logger_, " game-depth(p" + std::to_string(node->NodeState().ActivePlayerId()) +
      ") >>" + std::to_string(DRAW_VALUE) + ">> " + std::to_string(node->Value()),
      0 /* indent */, false /* endline */);
    statistics_.search_endby_game_depth_count_++;
    return std::make_pair(DRAW_VALUE, node);
  }
  if (search_depth_remaining == 0) {
    LOG_ALGO_IE(
      *logger_, " search-depth(p" + std::to_string(node->NodeState().ActivePlayerId()) +
      ") >>" + std::to_string(node->Value()) + ">> " + std::to_string(node->Value()),
      0 /* indent */, false /* endline */);
    statistics_.search_endby_search_depth_count_++;
    return std::make_pair(node->Value(), node);
  }

  // Move down tree following best actions until perform an action
  // that doesn't yet have a child node. Don't include actions that
  // would cause a loop/cycle for this search.
  const size_t node_generation = node->Generation();
  size_t cycle_avoided_cnt = 0;
  std::optional<uint64_t> best_action_id =
    node->MaxQU(node_generation, settings_.cpuct_, dirichlet, settings_.dirichlet_ratio_, &cycle_avoided_cnt);
  statistics_.cycle_avoided_count_ += cycle_avoided_cnt;
  THROWIF(!best_action_id,
          "alpha-zero unable to find action ID with max QU\n" + node->ToString(settings_.cpuct_));
  LOG_ALGO_IE(
    *logger_,
    ' ' + std::to_string(*best_action_id) + "(p" + std::to_string(node->NodeState().ActivePlayerId()) + ')',
    0 /* indent */, false /* endline */);

  bool next_created = false;
  AlphaZeroNode* next = node->Child(*best_action_id);
  if (next == nullptr) {
    // Find or create the node representing the state that results
    // when 'best_action_id' is applied to 'node'.
    StateUniqueConstPtr next_state(
      node->NodeState().CopyAndUpdate(*best_action_id, &nextactionid_arena_));
    NodesMap::iterator snext_itr = FindOrCreateNode(std::move(next_state), &next_created);
    next = &snext_itr->second;

    // Add the edge to connect 'node' to 'next'.
    node->SetChild(*best_action_id, next, 0 /* visit_count */);
    statistics_.edge_count_++;
  }

  // Set generation for the next node to be visited.
  next->SetGeneration(node_generation);

  // We update the visit counts before we move down the tree, this is
  // because we want the cpuct calculation to take into account this
  // visit even if we visit this node again in a loop.
  node->IncrementVisitCount();
  node->IncrementChildVisitCount(*best_action_id);

  // If next node to be visited was just created, then instead of
  // visiting it we simply set 'v' to its value. Dirichlet is only
  // used on the root node of a search, so set to nullptr for
  // subsequent nodes.
  float v = 0.0;
  AlphaZeroNode* tnode = nullptr;
  if (next_created) {
    v = next->Value();
    LOG_ALGO_IE(
      *logger_, " new >>" + std::to_string(v) + ">> " + std::to_string(next->Value()),
      0 /* indent */, false /* endline */);
  }
  else {
    // If 'next' is a different player than 'node', then a game
    // side-turn has been played.
    const size_t gdr = game_depth_remaining -
      (node->NodeState().ActivePlayerId() == next->NodeState().ActivePlayerId() ? 0 : 1);

    std::tie(v, tnode) = SearchTree(
      next, search_depth_remaining - 1, gdr, nullptr /* dirichlet */);
  }

  // If 'next' has a different active player than 'node', then need to
  // invert 'v'.
  if (node->NodeState().ActivePlayerId() != next->NodeState().ActivePlayerId()) {
    v = -v;
    LOG_ALGO_IE(
      *logger_, " >>" + std::to_string(v) + ">>", 0 /* indent */, false /* endline */);
  }

  // Update 'node's value, we have already incremented the visit count
  // for the node on the way down the tree, so need to take that into
  // account when updating the value.
  node->SetValue(((node->Value() * (node->VisitCount() - 1)) + v) / node->VisitCount());
  LOG_ALGO_IE(
    *logger_, " " + std::to_string(node->Value()), 0 /* indent */, false /* endline */);

  return std::make_pair(v, tnode);
}

void
AlphaZeroAlgo::GenerateDirichletSamples(
  const float alpha, const size_t sample_size, const size_t sample_count)
{
  const std::vector<double> alphas(sample_size, alpha);

  gsl_rng *rng = gsl_rng_alloc(gsl_rng_mt19937);
  gsl_rng_set(rng, (unsigned)time(NULL));

  dirichlet_samples_idx_ = 0;

  for (size_t idx = 0; idx < sample_count; ++idx) {
    std::vector<double> thetas(sample_size);
    gsl_ran_dirichlet(rng, sample_size, alphas.data(), thetas.data());
    dirichlet_samples_.emplace_back(std::move(thetas));
  }

  gsl_rng_free(rng);
}

//
// AlphaZeroNode
//
AlphaZeroNode::AlphaZeroNode(
  std::pmr::memory_resource& arena, StateUniqueConstPtr state,
  std::shared_ptr<const TritonInferResult> infer_result) :
  state_(std::move(state)), result_(infer_result),
  children_(state_->TotalActionIdCount(), nullptr, &arena),
  children_visit_counts_(state_->TotalActionIdCount(), 0, &arena),
  visit_count_(0), generation_(0)
{
  // Is node is a terminal, then its value is based on the terminal
  // status, otherwise it comes from the model.
  const auto& tstatus = state_->TerminalStatus();
  switch (tstatus.Kind()) {
  case StateTerminalStatus::TerminalKind::DRAW:
    value_ = DRAW_VALUE;
    break;
  case StateTerminalStatus::TerminalKind::WINNER:
    value_ = (tstatus.WinnerId() == state_->ActivePlayerId()) ? WINNER_VALUE : LOSER_VALUE;
    break;
  case StateTerminalStatus::TerminalKind::NON_TERMINAL: {
    assert(result_->VEntryCount() == 1);
    value_ = result_->V();
    if (std::isnan(value_)) {
      value_ = 0.0;
    }
  } break;
  }
}

void
AlphaZeroNode::SetChild(uint64_t action_id, AlphaZeroNode* child, size_t visit_count)
{
  assert(child != nullptr);
  assert(children_[action_id] == nullptr);
  children_[action_id] = child;
  children_visit_counts_[action_id] = visit_count;
}

std::vector<float>
AlphaZeroNode::ChildProbability() const
{
  std::vector<float> probs(state_->TotalActionIdCount(), 0.0);

  for (uint64_t aid : state_->ValidActionIds()) {
    probs[aid] = float(children_visit_counts_[aid]) / std::max(uint64_t(1), visit_count_ - 1);
  }

  return probs;
}

std::optional<uint64_t>
AlphaZeroNode::MaxQU(
  const size_t cycle_generation, const float cpuct, const std::vector<double>* dirichlet,
  const float dirichlet_ratio, size_t* cycle_avoided_count,
  std::vector<float>* qus, std::vector<float>* nocycle_qus) const
{
  if (qus != nullptr) {
    qus->clear();
    qus->resize(state_->TotalActionIdCount(), 0.0);
  }
  if (nocycle_qus != nullptr) {
    nocycle_qus->clear();
    nocycle_qus->resize(state_->TotalActionIdCount(), 0.0);
  }

  // We keep track of the best action that does *not* form a
  // loop/cycle, and the best action that may form a loop/cycle. We
  // prefer best action that does *not* form a loop/cycle, but use the
  // other in cases where no such action exists.
  const auto& pi = result_->Pi();
  float max_qu = -std::numeric_limits<float>::max();
  uint64_t best_action_id = std::numeric_limits<uint64_t>::max();
  float max_nocycle_qu = -std::numeric_limits<float>::max();
  uint64_t best_nocycle_action_id = std::numeric_limits<uint64_t>::max();

  for (uint64_t aid : state_->ValidActionIds()) {
    float qu;

    // Handle nan's in pi. Such a model probably isn't any good, but
    // using nan in below logic will produce invalid results.
    float dpi = (std::isnan(pi[aid])) ? 0.0 : pi[aid];

    if (dirichlet != nullptr) {
      dpi = (dpi * (1.0 - dirichlet_ratio)) + (dirichlet->at(aid) * dirichlet_ratio);
    }

    bool cycle = false;
    AlphaZeroNode* child = children_[aid];
    if (child == nullptr) {
      assert(children_visit_counts_[aid] == 0);
      qu = std::sqrt(visit_count_) * dpi * cpuct;
    }
    else {
      // generation == 0 indicates "no generation" so can't determine
      // if there is a cycle.
      if ((cycle_generation != 0) && (child->generation_ == cycle_generation)) {
        cycle = true;
      }

      float v = child->value_;
      if (child->state_->ActivePlayerId() != state_->ActivePlayerId()) {
        v = -v;
      }
      qu = (v + (std::sqrt(visit_count_) * dpi * cpuct / children_visit_counts_[aid]));
    }

    if (qu > max_qu) {
      max_qu = qu;
      best_action_id = aid;
    }
    if (!cycle && (qu > max_nocycle_qu)) {
      max_nocycle_qu = qu;
      best_nocycle_action_id = aid;
    }

    if (qus != nullptr) {
      qus->at(aid) = qu;
    }
    if (!cycle && (nocycle_qus != nullptr)) {
      nocycle_qus->at(aid) = qu;
    }
  }

  if (best_nocycle_action_id != std::numeric_limits<uint64_t>::max()) {
    if ((cycle_avoided_count != nullptr) && (best_nocycle_action_id != best_action_id)) {
      *cycle_avoided_count += 1;
    }
    return std::make_optional<uint64_t>(best_nocycle_action_id);
  }

  return ((best_action_id == std::numeric_limits<uint64_t>::max()) ?
          std::nullopt : std::make_optional<uint64_t>(best_action_id));
}

uint64_t
AlphaZeroNode::MaxVisitedValidChild() const
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

uint64_t
AlphaZeroNode::RandomWeightedValidChild(common::Random& rnd) const
{
  THROWIF(visit_count_ == 0, "alpha-zero node has zero visits");

  uint64_t visit_offset = rnd(uint64_t(1), uint64_t(visit_count_ - 1));

#ifndef NDEBUG
  // When debugging, make sure that there are no children for invalid
  // action IDs.
  const auto& vaids = state_->ValidActionIds();
  for (uint64_t aid = 0; aid < state_->TotalActionIdCount(); ++aid) {
    if (std::find(vaids.begin(), vaids.end(), aid) == vaids.end()) {
      assert(children_[aid] == nullptr);
      assert(children_visit_counts_[aid] == 0);
    }
  }
#endif

  uint64_t count_total = 0;
  for (uint64_t aid : state_->ValidActionIds()) {
    count_total += children_visit_counts_[aid];
    if (count_total >= visit_offset) {
      return aid;
    }
  }

  THROW("alpha-zero unable to find valid child with non-zero visits");
}

void
AlphaZeroNode::ValidateVisitCounts() const
{
  // Make sure non-terminal 'node's visit count equals 1 + sum of the
  // children visit counts. See AlphaZeroNode header documentation.
  uint64_t total_child_visit_count = 1;
  for (uint64_t aid : state_->ValidActionIds()) {
    total_child_visit_count += children_visit_counts_[aid];
  }

  if ((visit_count_ == 0) || (visit_count_ != total_child_visit_count)) {
    THROW("invalid node and children visit counts\n" + ToString());
  }
}

std::string
AlphaZeroNode::ToString(float cpuct) const
{
  std::stringstream sstr;

  sstr << "node <hash " + std::to_string(state_->Hash()) + ">\n";
  sstr
    << "  generation: " << generation_
    << ", active player ID: " << state_->ActivePlayerId()
    << ", visit count: " << visit_count_ << ", value: " << value_ << '\n';

  sstr << "  child value/visits: [";
  bool first = true;
  for (uint64_t aid = 0; aid < state_->TotalActionIdCount(); ++aid) {
    const AlphaZeroNode* child = children_[aid];
    sstr
      << ((first) ? "" : ", ")
      << ((child != nullptr) ? std::to_string(child->value_) : "-")
      << '/' << children_visit_counts_[aid];
    first = false;
  }
  sstr << "]\n";

  const auto& pi = result_->Pi();
  sstr << "  pi: [";
  first = true;
  for (size_t idx = 0; idx < state_->TotalActionIdCount(); ++idx) {
    sstr << ((first) ? "" : ", ") << pi[idx];
    first = false;
  }
  sstr << "]\n";

  if (cpuct > 0.0) {
    std::vector<float> qus, nocycle_qus;
    std::optional<uint64_t> best_action_id =
      MaxQU(0 /* generation */, cpuct, nullptr /* dirichlet */, 0.0 /* dirichlet_ratio */,
            nullptr /* cycle_avoided_count */, &qus, &nocycle_qus);
    sstr << "  cpuct: " << cpuct
         << ", best action: ";
    if (best_action_id) {
      sstr << NodeState().ActionToString(Action(Action::ActionKind::ACTIONID, *best_action_id))
          << " (" << std::to_string(*best_action_id) << ")\n";
    }
    else {
      sstr << "<none>\n";
    }
    sstr << "    no-cycle QUs: [";
    first = true;
    for (float p : nocycle_qus) {
      sstr << ((first) ? "" : ", ") << p;
      first = false;
    }
    sstr << "]\n" << "    QUs: [";
    first = true;
    for (float p : qus) {
      sstr << ((first) ? "" : ", ") << p;
      first = false;
    }
    sstr << "]\n";
  }

  sstr << "  state:\n";
  sstr << common::DuplicateAndIndent(state_->ToString(), 4 /* indent */);

  return sstr.str();
}

//
// AlphaZeroStatistics
//
AlphaZeroStatistics&
AlphaZeroStatistics::operator+=(const AlphaZeroStatistics& rhs)
{
  total_node_count_ += rhs.total_node_count_;
  terminal_node_count_ += rhs.terminal_node_count_;
  edge_count_ += rhs.edge_count_;
  search_count_ += rhs.search_count_;
  search_endby_search_depth_count_ += rhs.search_endby_search_depth_count_;
  search_endby_game_depth_count_ += rhs.search_endby_game_depth_count_;
  search_endby_terminal_count_ += rhs.search_endby_terminal_count_;
  cycle_avoided_count_ += rhs.cycle_avoided_count_;
  return *this;
}

void
AlphaZeroStatistics::Reset()
{
  total_node_count_ = 0;
  terminal_node_count_ = 0;
  edge_count_ = 0;
  search_count_ = 0;
  search_endby_search_depth_count_ = 0;
  search_endby_game_depth_count_ = 0;
  search_endby_terminal_count_ = 0;
  cycle_avoided_count_ = 0;
}

std::string
AlphaZeroStatistics::ToString() const
{
  std::stringstream ss;
  ss << "nodes: " << total_node_count_ << ", internal " << (total_node_count_ - terminal_node_count_)
     << ", terminal " << terminal_node_count_ << '\n';
  ss << "edges: " << edge_count_ << ", avg outgoing per node "
     << std::fixed << std::setprecision(2) << ((float)edge_count_ / total_node_count_) << '\n';
  ss
    << "searches: " << search_count_
    << ", end by search-depth " << search_endby_search_depth_count_
    << ", end by game-depth " << search_endby_game_depth_count_
    << ", end by terminal " << search_endby_terminal_count_
    << ", cycles avoided " << cycle_avoided_count_;

  return ss.str();
}

//
// AlphaZeroTrainingSamples
//
std::string
AlphaZeroTrainingSamples::Sample::ToString() const
{
  std::stringstream sstr;

  sstr << "sample count: " << count_ << ", z: " << z_value_ << ", v: " << target_v_ << '\n';
  if (!target_pi_.empty()) {
    sstr << "  pi: [";
    bool first = true;
    for (float pi : target_pi_) {
      sstr << ((first) ? "" : ", ") << pi;
      first = false;
    }
    sstr << "]\n";
  }

  sstr << "  " << common::DuplicateAndIndent(state_->ToString(), 4 /* indent */) << '\n';

  return sstr.str();
}

std::string
AlphaZeroTrainingSamples::ToString() const
{
  std::stringstream sstr;

  sstr << "count: " << train_samples_.size() << '\n';
  sstr << "aggregate-kind: " << AggregateKindToString(aggregate_kind_) << '\n';
  sstr << "value-kind: " << ValueKindToString(value_kind_);
  return sstr.str();
}

void
AlphaZeroTrainingSamples::AcquireAndAggregate(
  std::deque<AlphaZeroTrainingSamples::Sample>&& samples)
{
  for (auto& sample : samples) {
    // Sample value is either game outcome value, predicted 'v' or
    // some combination.
    float node_value = 0.0;
    switch (value_kind_) {
    case ValueKind::Z:
      node_value = sample.z_value_;
      break;
    case ValueKind::V:
      node_value = sample.az_value_;
      break;
    case ValueKind::AVERAGE:
      node_value = (sample.z_value_ + sample.az_value_) / 2.0;
      break;
    }

    // Set target 'pi' and 'v' for the training sample.
    auto sample_idx_itr = train_samples_lookup_.find(sample.state_.get());
    if ((aggregate_kind_ == AggregateKind::ALL) || (sample_idx_itr == train_samples_lookup_.end())) {
      // For ALL or a first-time-seen node, create a new Sample. We
      // don't update 'train_samples_lookup_' map for ALL so that we
      // always end up here and so always create a new sample.
      if (aggregate_kind_ != AggregateKind::ALL) {
        train_samples_lookup_[sample.state_.get()] = train_samples_.size();
      }

      train_samples_.emplace_back(
        std::move(sample.state_), sample.triton_result_, sample.z_value_,
        sample.az_value_, std::move(sample.target_pi_), node_value);
      LOG_ALGO(*logger_, "new training sample:");
      LOG_ALGO_IE(*logger_, train_samples_.back().ToString(),
                  2 /* indent */, true /* endline */);
    }
    else {
      assert(aggregate_kind_ == AggregateKind::AVERAGE);
      // Otherwise average 'target_pi_' and 'node_value' into the
      // existing sample at the index given by 'sample_idx_itr'.
      const size_t idx = sample_idx_itr->second;
      assert(idx < train_samples_.size());
      Sample& existing_sample = train_samples_[idx];
      existing_sample.target_v_ += node_value;

      assert(existing_sample.target_pi_.size() == sample.target_pi_.size());
      for (size_t i = 0; i < existing_sample.target_pi_.size(); ++i) {
        existing_sample.target_pi_[i] += sample.target_pi_[i];
      }

      existing_sample.count_++;
      LOG_ALGO(*logger_, "aggregate into existing training sample:");
      LOG_ALGO_IE(*logger_,  existing_sample.ToString(), 2 /* indent */, true /* endline */);
    }
  }
}

void
AlphaZeroTrainingSamples::WriteTraining(const std::string& dir) const
{
  // Write in.bin, pi.bin and v.bin as a single large batch of each of
  // those training node values.
  std::filesystem::path pin(dir); pin /= "in.bin";
  std::filesystem::path ppi(dir); ppi /= "pi.bin";
  std::filesystem::path pv(dir); pv /= "v.bin";

  LOG_ALGO(*logger_, "writing training input batch to " + pin.string());
  LOG_ALGO(*logger_, "writing training pi batch to " + ppi.string());
  LOG_ALGO(*logger_, "writing training v batch to " + pv.string());

  std::ofstream fin(pin, std::ios::out | std::ios::binary);
  std::ofstream fpi(ppi, std::ios::out | std::ios::binary);
  std::ofstream fv(pv, std::ios::out | std::ios::binary);

  // Write training samples. Make sure the data sizes from each of the
  // training nodes is consistent.
  if (!train_samples_.empty()) {
    const TritonInferResult* repr_result = train_samples_.back().triton_result_.get();
    for (const auto& s : train_samples_) {
      LOG_ALGO_IE(*logger_, s.ToString(), 2 /* indent */, true /* endline */);

      auto& din = s.state_->TritonRepresentation();
      fin.write(reinterpret_cast<const char*>(din.data()), din.size() * sizeof(uint8_t));
      assert((din.size() * sizeof(uint8_t)) == repr_result->InputByteSize());

      for (const float dpi : s.target_pi_) {
        float dpi_per = dpi / s.count_;
        fpi.write(reinterpret_cast<const char*>(&dpi_per), sizeof(dpi_per));
      }

      const float dv_per = s.target_v_ / s.count_;
      fv.write(reinterpret_cast<const char*>(&dv_per), sizeof(dv_per));
    }

    // Write train.json to contain descriptive metadata for the
    // training data.
    std::filesystem::path pjson(dir); pjson /= "train.json";
    LOG_ALGO(*logger_, "writing training metadata to " + pjson.string());
    std::ofstream fjson(pjson, std::ios::out);
    fjson
      << "{\n"
      << " \"in_shape\" : " << ToJsonArray(repr_result->InputShape(), train_samples_.size()) << ",\n"
      << " \"pi_shape\" : " << ToJsonArray(repr_result->PiShape(), train_samples_.size()) << ",\n"
      << " \"v_shape\" : " << ToJsonArray(repr_result->VShape(), train_samples_.size()) << "\n"
      << "}\n";
    fjson.close();
  }

  fin.close();
  fpi.close();
  fv.close();
}

std::string
AlphaZeroTrainingSamples::ToJsonArray(const std::vector<int64_t> shape, size_t batch_count) const
{
  bool first = true;
  std::string str("[");
  for (size_t d : shape) {
    if (first) {
      first = false;
      if (batch_count > 0) {
        d = batch_count;
      }
    } else {
      str += ',';
    }
    str += std::to_string(d);
  }

  str += "]";
  return str;
}

void
AlphaZeroTrainingSamples::Reset()
{
  train_samples_.clear();
  train_samples_lookup_.clear();
}

} // namespace cifre
