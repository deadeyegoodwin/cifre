// Copyright 2024, David Goodwin. All rights reserved.

#include <filesystem>
#include <iomanip>
#include <limits>
#include <sstream>

#include "gsl/gsl_rng.h"
#include "gsl/gsl_randist.h"
#include "cifre_alphazero.h"
#include "common_error.h"

namespace cifre {

constexpr float DRAW_VALUE = 0.0;
constexpr float WINNER_VALUE = 1.0;
constexpr float LOSER_VALUE = -1.0;

//
// AlphaZero
//
AlphaZero::AlphaZero(
  const std::shared_ptr<common::Logger>& logger, std::unique_ptr<TritonModel> triton_model,
  const AlphaZeroSettings& settings) :
  logger_(logger), triton_model_(std::move(triton_model)), settings_(settings)
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
AlphaZero::Reset()
{
  nodes_.clear();
  game_action_nodes_.clear();
  statistics_.Reset();
}

void
AlphaZero::GameStart()
{
  Reset();
}

void
AlphaZero::GameEnd(const State& terminal_state, const StateTerminalStatus& status,
                   std::vector<AlphaZeroTrainingSamples::Sample>* samples)
{
  THROWIF(status.Kind() == cifre::StateTerminalStatus::TerminalKind::NON_TERMINAL,
          "game ended with NON-TERMINAL status");

  // If 'samples' is non-nullptr, then append all the collected
  // training samples to 'samples'. At this point each training sample
  // consists of just the node and the z-value (game outcome value).
  if (samples != nullptr) {
    for (auto& n : game_action_nodes_) {
      // Game outcome value (Z).
      float z_value = DRAW_VALUE;
      switch (status.Kind()) {
      case StateTerminalStatus::TerminalKind::NON_TERMINAL:
      case StateTerminalStatus::TerminalKind::DRAW:
        break;
      case StateTerminalStatus::TerminalKind::WINNER:
        const size_t winner_id = status.WinnerId();
        z_value = (n->NodeState().ActivePlayerId() == winner_id) ? WINNER_VALUE : LOSER_VALUE;
        break;
      }

      samples->emplace_back(n, z_value);
    }
  }

  Reset();
}

uint64_t
AlphaZero::NextActionId(const State& state, size_t max_game_turns_remaining)
{
  THROWIF(state.TerminalStatus().Kind() != StateTerminalStatus::TerminalKind::NON_TERMINAL,
          "alpha-zero cannot find next action for terminal state");

  // FIXME here we assume the game has two players...
  // Use the std::max to avoid issues with overflow as
  // std::numeric_limits<size_t>::max() may be the value of
  // 'max_game_turns_remaining'.
  const size_t max_game_depth =
    std::max(max_game_turns_remaining, max_game_turns_remaining * 2);

  // If Dirichlet distribution is being used but has not yet been
  // generated, then do so now using GSL (GNU Scientific
  // Library). This is done lazily because a State object is needed to
  // get the total number of action IDs.
  const std::vector<double>* dirichlet = nullptr;
  if (settings_.dirichlet_alpha_ > 0.0) {
    if (dirichlet_samples_.empty()) {
      GenerateDirichletSamples(settings_.dirichlet_alpha_, state.TotalActionIdCount(),
                               settings_.dirichlet_sample_count_);
    }

    assert(dirichlet_samples_.size() == settings_.dirichlet_sample_count_);
    dirichlet = &dirichlet_samples_[dirichlet_samples_idx_];
    dirichlet_samples_idx_ = (dirichlet_samples_idx_ + 1) % dirichlet_samples_.size();
  }

  // Find, creating if necessary, tree node that represents 'state'.
  auto node = FindOrCreateNode(std::unique_ptr<const State>(state.Clone()));
  assert(node != nullptr);

  LOGGER_LOGHIGH(*logger_, "\nalphazero: starting node");
  LOGGER_LOGHIGH_IE(*logger_, node->ToString(settings_.cpuct_),
                    2 /* indent */, true /* endline */);

  // Update the tree by performing searches starting from 'node'.
  for (size_t idx = 0; idx < settings_.search_count_; ++idx) {
    LOGGER_LOGHIGH_IE(
      *logger_,
      "search " + std::to_string(idx+1) + '/' + std::to_string(settings_.search_count_) + ':',
      0 /* indent */, false /* endline */);
    SearchTree(node.get(), settings_.max_search_depth_, max_game_depth, dirichlet);
    LOGGER_LOGHIGH_IE(*logger_, "\n", 0 /* indent */, false /* endline */);
  }

  LOGGER_LOGHIGH(*logger_, "\nalphazero: choosing action for node");
  LOGGER_LOGHIGH_IE(*logger_, node->ToString(settings_.cpuct_),
                    2 /* indent */, true /* endline */);

  // Based on 'kind', choose the next action. Because 'state' is not a
  // terminal state, there must be at least one action with a non-zero
  // visit count.
  uint64_t action_id;
  switch (settings_.next_action_kind_) {
  case AlphaZeroSettings::NextActionKind::MAX_VISITED:
    action_id = node->MaxVisitedValidChild();
    break;
  case AlphaZeroSettings::NextActionKind::RANDOM_VISIT_WEIGHTED:
    action_id = node->RandomWeightedValidChild(rnd_);
    break;
  default:
    THROW("unreachable");
  }

  // If training, record the nodes for which alpha-zero makes an
  // action decision.
  if (settings_.training_) {
    game_action_nodes_.push_back(node);
  }

  return action_id;
}

std::shared_ptr<AlphaZeroNode>
AlphaZero::FindOrCreateNode(std::unique_ptr<const State> state, bool* created)
{
  std::shared_ptr<AlphaZeroNode> new_node;
  bool new_created;

  auto node_itr = nodes_.find(state.get());
  if (node_itr != nodes_.end()) {
    new_node = node_itr->second;
    new_created = false;
  }
  else {
    auto result = triton_model_->Infer(*state);
    assert(result->PiEntryCount() == state->TotalActionIdCount());

    const State* pstate = state.get();

    new_created = true;
    new_node = std::make_shared<AlphaZeroNode>(std::move(state), std::move(result));
    nodes_[pstate] = new_node;

    // Node creation needs to be recognized as the first visit.
    new_node->IncrementVisitCount();

    statistics_.total_node_count_++;
    if (new_node->IsTerminal()) {
      statistics_.terminal_node_count_++;
    }
  }

  if (created != nullptr) {
    *created = new_created;
  }

  assert(new_node != nullptr);
  return new_node;
}

float
AlphaZero::SearchTree(
  AlphaZeroNode* node, const size_t search_depth_remaining,
  const size_t game_depth_remaining, const std::vector<double>* dirichlet)
{
  // If 'node' is a terminal node then show that it was visited and
  // return its value.
  if (node->IsTerminal()) {
    LOGGER_LOGHIGH(*logger_, " terminal(p" + std::to_string(node->NodeState().ActivePlayerId()) + ')');
    LOGGER_LOGHIGH_IE(*logger_, node->ToString(settings_.cpuct_),
                      2 /* indent */, true /* endline */);
    statistics_.search_endby_terminal_count_++;
    node->IncrementVisitCount();
    return node->Value();
  }

#ifndef NDEBUG
  // Non-terminal nodes must maintain correctness betweeo 'node' visit
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
    LOGGER_LOGHIGH(*logger_, " game-depth(p" + std::to_string(node->NodeState().ActivePlayerId()) + ')');
    LOGGER_LOGHIGH_IE(*logger_, node->ToString(settings_.cpuct_),
                      2 /* indent */, true /* endline */);
    statistics_.search_endby_game_depth_count_++;
    return DRAW_VALUE;
  }
  if (search_depth_remaining == 0) {
    LOGGER_LOGHIGH(*logger_, " search-depth(p" + std::to_string(node->NodeState().ActivePlayerId()) + ')');
    LOGGER_LOGHIGH_IE(*logger_, node->ToString(settings_.cpuct_),
                      2 /* indent */, true /* endline */);
    statistics_.search_endby_search_depth_count_++;
    return node->Value();
  }

  // Move down tree following best actions until perform an action
  // that doesn't yet have a child node.
  std::optional<uint64_t> best_action_id =
    node->MaxQU(settings_.cpuct_, dirichlet, settings_.dirichlet_ratio_);
  THROWIF(!best_action_id, "alpha-zero unable to find action ID with max QU");
  LOGGER_LOGHIGH_IE(
    *logger_,
    ' ' + std::to_string(*best_action_id) + "(p" + std::to_string(node->NodeState().ActivePlayerId()) + ')',
    0 /* indent */, false /* endline */);

  AlphaZeroNode* next = node->Child(*best_action_id);
  if (next == nullptr) {
    // Find or create the node representing the state that results
    // when 'best_action_id' is applied to 'node'.
    std::unique_ptr<State> next_state(node->NodeState().Clone());
    next_state->Update(*best_action_id);

    std::shared_ptr<AlphaZeroNode> snext = FindOrCreateNode(std::move(next_state));
    assert(snext != nullptr);
    next = snext.get();

    // Add the edge to connect 'node' to 'next', then continue search.
    node->SetChild(*best_action_id, next, 0 /* visit_count */);
    statistics_.edge_count_++;
  }

  // We update the visit counts before we move down the tree, this is
  // because we want the cpuct calculation to take into account this
  // visit even if we visit this node again in a loop.
  node->IncrementVisitCount();
  node->IncrementChildVisitCount(*best_action_id);

  float v = SearchTree(
    next, search_depth_remaining - 1, game_depth_remaining - 1, nullptr /* dirichlet */);

  // If 'next' has a different active player than 'node', then need to
  // invert 'v'.
  if (node->NodeState().ActivePlayerId() != next->NodeState().ActivePlayerId()) {
    v = -v;
  }

  // Update 'node's value, we have already incremented the visit count
  // for the node one the way down the tree, so need to take that into
  // account when updating the value.
  node->SetValue(((node->Value() * (node->VisitCount() - 1)) + v) / node->VisitCount());

  return v;
}

void
AlphaZero::GenerateDirichletSamples(
  const float alpha, const size_t sample_size, const size_t sample_count)
{
  const std::vector<double> alphas(sample_size, alpha);

  gsl_rng *rng = gsl_rng_alloc (gsl_rng_mt19937);
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
    std::unique_ptr<const State> state, std::shared_ptr<const TritonInferResult> infer_result) :
  state_(std::move(state)), result_(infer_result),
  children_(state_->TotalActionIdCount(), nullptr),
  children_visit_counts_(state_->TotalActionIdCount(), 0),
  visit_count_(0)
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
  const float cpuct, const std::vector<double>* dirichlet,
  const float dirichlet_ratio, std::vector<float>* qus) const
{
  if (qus != nullptr) {
    qus->clear();
    qus->resize(state_->TotalActionIdCount(), 0.0);
  }

  const auto& pi = result_->Pi();
  float max_qu = -std::numeric_limits<float>::max();
  uint64_t best_action_id = std::numeric_limits<uint64_t>::max();

  for (uint64_t aid : state_->ValidActionIds()) {
    float qu;

    float dpi = pi[aid];
    if (dirichlet != nullptr) {
      dpi = (dpi * (1.0 - dirichlet_ratio)) + (dirichlet->at(aid) * dirichlet_ratio);
    }

    AlphaZeroNode* child = children_[aid];
    if (child == nullptr) {
      assert(children_visit_counts_[aid] == 0);
      qu = std::sqrt(visit_count_) * dpi * cpuct;
    }
    else {
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

    if (qus != nullptr) {
      qus->at(aid) = qu;
    }
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
    if (vaids.find(aid) == vaids.end()) {
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
    THROW("invalid node and children visit counts\n" + ToString(0.0));
  }
}

std::string
AlphaZeroNode::ToString(float cpuct, common::Logger::Level filter_level) const
{
  std::stringstream sstr;

  sstr << "node <hash " + std::to_string(state_->Hash()) + ">\n";
  sstr
    << "  active player ID: " << state_->ActivePlayerId()
    << ",  visit count: " << visit_count_ << ", value: " << value_ << '\n';

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

  std::vector<float> qus;
  std::optional<uint64_t> best_action_id =
    MaxQU(cpuct, nullptr /* dirichlet */, 0.0 /* dirichlet_ratio */, &qus);
  sstr << "  cpuct: " << cpuct
       << ", best action: " << ((best_action_id) ? std::to_string(*best_action_id) : "<none>")
       << ", QUs: [";
  first = true;
  for (float p : qus) {
    sstr << ((first) ? "" : ", ") << p;
    first = false;
  }
  sstr << "]\n";

  sstr << "  state:\n";
  sstr << common::IndentMultiline(state_->ToString(), 4 /* indent */);

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
  search_endby_search_depth_count_ += rhs.search_endby_search_depth_count_;
  search_endby_game_depth_count_ += rhs.search_endby_game_depth_count_;
  search_endby_terminal_count_ += rhs.search_endby_terminal_count_;
  return *this;
}

void
AlphaZeroStatistics::Reset()
{
  total_node_count_ = 0;
  terminal_node_count_ = 0;
  edge_count_ = 0;
  search_endby_search_depth_count_ = 0;
  search_endby_game_depth_count_ = 0;
  search_endby_terminal_count_ = 0;
}

std::string
AlphaZeroStatistics::ToString() const
{
  std::stringstream ss;
  ss << "nodes: " << total_node_count_ << ", internal " << (total_node_count_ - terminal_node_count_)
     << ", terminal " << terminal_node_count_ << '\n';
  ss << "edges: " << edge_count_ << ", avg outgoing per node "
     << std::fixed << std::setprecision(2) << ((float)edge_count_ / total_node_count_) << '\n';

  const size_t search_count =
    search_endby_search_depth_count_ + search_endby_game_depth_count_ + search_endby_terminal_count_;
  ss
    << "searches: " << search_count
    << ", end by search-depth " << search_endby_search_depth_count_
    << ", end by game-depth " << search_endby_game_depth_count_
    << ", end by terminal " << search_endby_terminal_count_;

  return ss.str();
}

//
// AlphaZeroTrainingSamples
//
void
AlphaZeroTrainingSamples::AcquireAndAggregate(
  std::vector<AlphaZeroTrainingSamples::Sample>&& samples)
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
      node_value = sample.node_->Value();
      break;
    case ValueKind::AVERAGE:
      node_value = (sample.z_value_ + sample.node_->Value()) / 2.0;
      break;
    }

    // Set target 'pi' and 'v' for the training sample.
    const State* sample_state = &sample.node_->NodeState();
    auto sample_itr = train_samples_.find(sample_state);
    if ((aggregate_kind_ == AggregateKind::ALL) || (sample_itr == train_samples_.end())) {
      // For ALL or a first-time-seen node, create a new Sample.
      auto node_probs = sample.node_->ChildProbability();
      assert((node_probs.size() * sizeof(float)) == sample.node_->NodeTritonResult().PiByteSize());

#ifndef NDEBUG
      auto created_sample_itr =
#endif
        train_samples_.emplace(
          std::piecewise_construct,
          std::forward_as_tuple(sample_state),
          std::forward_as_tuple(sample.node_, sample.z_value_, std::move(node_probs), node_value));
      assert(created_sample_itr.second);
    }
    else {
      // Otherwise average 'node_probs' and 'node_value' into the
      // existing sample.
      assert(aggregate_kind_ == AggregateKind::AVERAGE);
      Sample& sample = sample_itr->second;
      sample.target_v_ += node_value;

      auto node_probs = sample.node_->ChildProbability();
      assert((node_probs.size() * sizeof(float)) == sample.node_->NodeTritonResult().PiByteSize());
      for (size_t i = 0; i < node_probs.size(); ++i) {
        sample.target_pi_[i] += node_probs[i];
      }

      sample.count_++;
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

  LOGGER_LOGHIGH(*logger_, "writing training input batch to " + pin.string());
  LOGGER_LOGHIGH(*logger_, "writing training pi batch to " + ppi.string());
  LOGGER_LOGHIGH(*logger_, "writing training v batch to " + pv.string());

  std::ofstream fin(pin, std::ios::out | std::ios::binary);
  std::ofstream fpi(ppi, std::ios::out | std::ios::binary);
  std::ofstream fv(pv, std::ios::out | std::ios::binary);

  // Write training samples. Make sure the data sizes from each of the
  // training nodes is consistent.
  const TritonInferResult* repr_result = nullptr;
  for (const auto& pr : train_samples_) {
    if (repr_result == nullptr) {
      repr_result = &(pr.second.node_->NodeTritonResult());
    }

    LOGGER_LOGHIGH(*logger_, "writing training sample, value " +
                   std::to_string(pr.second.target_v_) + ':');
    LOGGER_LOGHIGH_IE(*logger_, pr.first->ToString(),
                      2 /* indent */, true /* endline */);

    auto& din = pr.first->TritonRepresentation();
    fin.write(reinterpret_cast<const char*>(din.data()), din.size() * sizeof(uint8_t));
    assert((din.size() * sizeof(uint8_t)) == repr_result->InputByteSize());

    for (const float dpi : pr.second.target_pi_) {
      float dpi_per = dpi / pr.second.count_;
      fpi.write(reinterpret_cast<const char*>(&dpi_per), sizeof(dpi_per));
    }

    const float dv = pr.second.target_v_ / pr.second.count_;
    fv.write(reinterpret_cast<const char*>(&dv), sizeof(dv));
  }

  fin.close();
  fpi.close();
  fv.close();

  // Write train.json to contain descriptive metadata for the training
  // data.
  if (!train_samples_.empty()) {
    std::filesystem::path pjson(dir); pjson /= "train.json";
    LOGGER_LOGHIGH(*logger_, "writing training metadata to " + pjson.string());
    std::ofstream fjson(pjson, std::ios::out);
    fjson
      << "{\n"
      << " \"in_shape\" : " << ToJsonArray(repr_result->InputShape(), train_samples_.size()) << ",\n"
      << " \"pi_shape\" : " << ToJsonArray(repr_result->PiShape(), train_samples_.size()) << ",\n"
      << " \"v_shape\" : " << ToJsonArray(repr_result->VShape(), train_samples_.size()) << "\n"
      << "}\n";
    fjson.close();
  }
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

} // namespace cifre
