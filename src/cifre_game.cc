// Copyright 2024, David Goodwin. All rights reserved.

#include <filesystem>
#include <iostream>
#include <sstream>
#include <vector>

#include "cifre_game.h"
#include "cifre_version.h"
#include "common_error.h"
#include "common_logger.h"
#include "common_task_pool.h"
#include "common_util.h"

namespace cifre {

namespace {

std::string
FloatToString(float f, size_t precision = 2)
{
  std::stringstream ss;
  ss << std::fixed << std::setprecision(precision) << f;
  return ss.str();
}

std::string
FormatUsageMessage(std::string str, size_t offset = 0, size_t width = 60)
{
  size_t current_pos = offset;
  while ((current_pos + width) < str.length()) {
    size_t n = str.rfind(' ', current_pos + width);
    if (n != std::string::npos) {
      str.replace(n, 1, "\n\t");
      current_pos += (width + 9);
    }
  }

  return str;
}

common::Logger*
CreateLogger(const std::string& filename, common::Logger::Level level,
             common::Logger::Timestamp timestamp_kind)
{
  if (filename.empty()) {
    return nullptr;
  }

  if (filename == "stdout") {
    return new common::StreamLogger(level, timestamp_kind, std::cout);
  }
  if (filename == "stderr") {
    return new common::StreamLogger(level, timestamp_kind, std::cerr);
  }

  return new common::StreamLogger(level, timestamp_kind, filename, false /* buffered */);
}

} // namespace

//
// PlayOneTask
//
// A common::TaskPool::Task object that manages its own TritonModel,
// Player and AlphaZero objects; and (re)uses those objects to play
// one or more games. Each Invoke() plays one game.
//
class PlayOneTask : public common::TaskPool::Task {
public:
  enum class GameResult { WINNER1, WINNER2, DRAW, QUIT };

  PlayOneTask(
    const std::shared_ptr<common::Logger>& logger,
    std::unique_ptr<State> initial_state,
    std::vector<std::unique_ptr<Player>> players,
    std::vector<std::shared_ptr<AlphaZero>> alphazeros,
    size_t max_turns_per_game) :
    logger_(logger), initial_state_(std::move(initial_state)),
    players_(std::move(players)), alphazeros_(std::move(alphazeros)),
    max_turns_per_game_(max_turns_per_game) { }

  // Return the result of playing one game. Undefined until Invoke()
  // is called and returns.
  GameResult Result() const { return result_; }

  // Return AlphaZero statistics collected for all alphazero activity.
  const AlphaZeroStatistics& AlphaZeroStats() const { return alphazero_statistics_; }

  // Return the training samples collected from playing one
  // game. Undefined until Invoke() is called and returns.
  std::vector<AlphaZeroTrainingSamples::Sample>&& AcquireTrainSamples() {
    return std::move(train_samples_); }

  // See common::TaskPool::Task
  bool Invoke() override;

private:
  std::shared_ptr<common::Logger> logger_;
  std::unique_ptr<State> initial_state_;
  std::vector<std::unique_ptr<Player>> players_;
  std::vector<std::shared_ptr<AlphaZero>> alphazeros_;
  const size_t max_turns_per_game_;

  AlphaZeroStatistics alphazero_statistics_;
  std::vector<AlphaZeroTrainingSamples::Sample> train_samples_;
  GameResult result_;
};

bool
PlayOneTask::Invoke()
{
  // Create initial state for the game that gets modified as the game
  // progresses.
  std::unique_ptr<State> state(initial_state_->Clone());

  // Let all players and alphazeros know a new game is starting.
  for (auto& p : players_) {
    p->GameStart();
  }
  for (auto& a : alphazeros_) {
    if (a != nullptr) {
      a->GameStart();
    }
  }

  alphazero_statistics_.Reset();
  train_samples_.clear();
  result_ = GameResult::QUIT;

  const auto starting_player_id = state->ActivePlayerId();
  size_t game_turns_remaining = max_turns_per_game_;

  bool game_quit = false;
  StateTerminalStatus terminal_status = state->TerminalStatus();
  while (!game_quit &&
         (game_turns_remaining > 0) &&
         (terminal_status.Kind() == StateTerminalStatus::TerminalKind::NON_TERMINAL)) {
    const auto player_id = state->ActivePlayerId();
    const Player* player = players_[player_id].get();
    const Action action = player->NextAction(*state, game_turns_remaining);
    switch (action.Kind()) {
    case Action::ActionKind::INVALID:
    case Action::ActionKind::HELP:
    case Action::ActionKind::SHOW_STATE:
      GameFail("unexpected player action");
      break;

    case Action::ActionKind::QUIT:
      game_quit = true;
      break;

    case Action::ActionKind::ACTIONID:
      state->Update(action.ActionId());
      terminal_status = state->TerminalStatus();
      break;
    }

    // If 'state' active player differs from 'player' (indicating that
    // it changed as a result 'action'), and active player is now
    // 'starting_player_id', then a turn has completed.
    if ((player_id != state->ActivePlayerId()) &&
        (starting_player_id == state->ActivePlayerId())) {
      --game_turns_remaining;
    }
  }

  // Game was quit.
  if (game_quit) {
    LOGGER_LOGMED(*logger_, "game quit: no winner");
    LOGGER_LOGMED(*logger_, state->ToString());
    result_ = GameResult::QUIT;
    return false;
  }

  // If ran out of game turns, then game is a draw.
  if (game_turns_remaining == 0) {
    terminal_status = StateTerminalStatus(StateTerminalStatus::TerminalKind::DRAW);
  }

  // Let all players and alphazeros know game is completed. Collect
  // statistics.
  for (auto& p : players_) {
    p->GameEnd(*state, terminal_status);
  }
  for (auto& a : alphazeros_) {
    if (a != nullptr) {
      alphazero_statistics_ += a->Statistics();
      a->GameEnd(*state, terminal_status, &train_samples_);
    }
  }

  // Game completed as a draw.
  if (terminal_status.Kind() == StateTerminalStatus::TerminalKind::DRAW) {
    LOGGER_LOGMED(*logger_, "game over: draw");
    LOGGER_LOGMED(*logger_, state->ToString());
    result_ = GameResult::DRAW;
    return false;
  }

  // Game completed with a winner.
  const size_t winner_id = terminal_status.WinnerId();

  // FIXME only supports 2 player games
  result_ = (winner_id == 0) ? GameResult::WINNER1 : GameResult::WINNER2;

  // Return false to indicate that this task should be returned by
  // the TaskPool on the completed queue.
  return false;
}

//
// GameOption
//
uint64_t
GameOption::ParseUnsigned(
  const GameOption& opt, const std::string& arg_str, uint64_t min_value, uint64_t max_value)
{
  std::string::size_type idx;
  uint64_t ul = std::stoull(arg_str, &idx);
  GameCheckTrueMsg(idx == arg_str.size(),
                   "unable to parse " + opt.FlagWithDash() + "=" + arg_str + " as unsigned integer");
  GameCheckTrueMsg(ul >= min_value,
                   "expected " + opt.FlagWithDash() + " to have integer value >= " +
                   std::to_string(min_value) + ", got " + std::to_string(ul));
  GameCheckTrueMsg(ul <= max_value,
                   "expected " + opt.FlagWithDash() + " to have integer value <= " +
                   std::to_string(max_value) + ", got " + std::to_string(ul));
  return ul;
}

int64_t
GameOption::ParseSigned(
  const GameOption& opt, const std::string& arg_str, int64_t min_value, int64_t max_value)
{
  std::string::size_type idx;
  int64_t sl = std::stoll(arg_str, &idx);
  GameCheckTrueMsg(idx == arg_str.size(),
                   "unable to parse " + opt.FlagWithDash() + "=" + arg_str + " as signed integer");
  GameCheckTrueMsg(sl >= min_value,
                   "expected " + opt.FlagWithDash() + " to have integer value >= " +
                   std::to_string(min_value) + ", got " + std::to_string(sl));
  GameCheckTrueMsg(sl <= max_value,
                   "expected " + opt.FlagWithDash() + " to have integer value <= " +
                   std::to_string(max_value) + ", got " + std::to_string(sl));
  return sl;
}

float
GameOption::ParseFloat(
  const GameOption& opt, const std::string& arg_str, float min_value, float max_value)
{
  std::string::size_type idx;
  float f = std::stof(arg_str, &idx);
  GameCheckTrueMsg(idx == arg_str.size(),
                   "unable to parse " + opt.FlagWithDash() + "=" + arg_str + " as float");
  GameCheckTrueMsg(f >= min_value,
                   "expected " + opt.FlagWithDash() + " to have float value >= " +
                   std::to_string(min_value) + ", got " + std::to_string(f));
  GameCheckTrueMsg(f <= max_value,
                   "expected " + opt.FlagWithDash() + " to have float value <= " +
                   std::to_string(max_value) + ", got " + std::to_string(f));
  return f;
}

std::string
Game::Usage() const
{
  std::stringstream ss;

  ss << "usage: " << ExecutableName() << " [options]" << std::endl;
  for (const auto& o : game_options_) {
    if (!o.ArgumentTypeDescription().empty()) {
      ss << "  " << o.FlagWithDash() << " <" << o.ArgumentTypeDescription() << ">" << std::endl
         << "\t" << FormatUsageMessage(o.Description()) << std::endl;
    }
    else {
      ss << "  " << o.FlagWithDash() << std::endl
         << "\t" << FormatUsageMessage(o.Description()) << std::endl;
    }
  }

  return ss.str();
}

//
// Game
//
Game::Game() :
  initialized_(false), parallel_(8), game_count_(1), starting_player_idx_(0)
{
  game_options_ = {
    {BuiltInOptionId::OPTION_HELP, "help", GameOption::ArgNone, "Print usage"},

    {BuiltInOptionId::OPTION_QUIET, "quiet", GameOption::ArgNone,
     "Disable all output."},
    {BuiltInOptionId::OPTION_VERBOSE, "verbose", GameOption::ArgNone,
     "Enable verbose output."},
    {BuiltInOptionId::OPTION_LOG_FILE, "log-file", GameOption::ArgStr,
     "File for console output. 'stdout' and 'stderr' are allowed."},
    {BuiltInOptionId::OPTION_REPORT_FILE, "report-file", GameOption::ArgStr,
     "File for report output. Default is no report output."},

    {BuiltInOptionId::OPTION_TRAIN, "train", GameOption::ArgNone,
     "Use self-play to generate training data. --train-dir must be specified."},
    {BuiltInOptionId::OPTION_TRAIN_DIR, "train-dir", GameOption::ArgStr,
     "Training data directory. Has no effect if --train is not also specified."},

    {BuiltInOptionId::OPTION_PARALLEL, "parallel", GameOption::ArgInt,
     "Number of games to play in parallel. Default is 8."},
    {BuiltInOptionId::OPTION_GAME_COUNT, "game-count", GameOption::ArgInt,
     "Number of games to play. Default is 1."},

    {BuiltInOptionId::OPTION_PLAYER1, "player1", GameOption::ArgStr,
     "Type of the first player: 'human', 'random', 'alphazero'."},
    {BuiltInOptionId::OPTION_PLAYER2, "player2", GameOption::ArgStr,
     "Type of the first player: 'human', 'random', 'alphazero'."},
    {BuiltInOptionId::OPTION_PLAYER_NAME, "name", GameOption::ArgStr,
     "Name of player. --player1 or --player2 must be specified before this argument. "
     "The default name is automatically generated from the player kind."},
    {BuiltInOptionId::OPTION_PLAYER_STARTING, "starting", GameOption::ArgNone,
     "Indicate that the player should start the game. May only be indicated for a "
     "single player. --player1 or --player2 must be specified before this argument."},
    {BuiltInOptionId::OPTION_PLAYER_ALPHAZERO_MODEL, "alphazero-model", GameOption::ArgStr,
     "Alphazero model associated with player. --player1 or --player2 must be specified "
     "before this argument."},
    {BuiltInOptionId::OPTION_PLAYER_ALPHAZERO_SEARCH_COUNT, "alphazero-search-count", GameOption::ArgInt,
     "Number of searches made to alphazero tree before "
     "choosing the next action for the player. --player1 or --player2 must "
     "be specified before this argument."},
    {BuiltInOptionId::OPTION_PLAYER_ALPHAZERO_SEARCH_DEPTH, "alphazero-search-depth", GameOption::ArgInt,
     "Maximum depth of searches made to alphazero tree before "
     "choosing the next action for the player. --player1 or --player2 must "
     "be specified before this argument."},
    {BuiltInOptionId::OPTION_PLAYER_ALPHAZERO_CPUCT, "alphazero-cpuct", GameOption::ArgFloat,
     "AlphaZero cpuct value."}};
}

void
Game::Initialize(int argc, char **argv)
{
  try {
    assert(!initialized_);
    if (!initialized_) {
      const auto& coptions = CustomGameOptions();
      game_options_.insert(game_options_.end(), coptions.begin(), coptions.end());

      Parse(argc, argv);
    }
  } catch (const std::exception& ex) {
    GameFail(ex.what());
  }

  initialized_ = true;
}

const GameOption&
Game::OptionIdToOption(GameOption::OptionId id) const
{
  for (const auto& opt : game_options_) {
    if (opt.Id() == id) {
      return opt;
    }
  }

  GameFail("unknown option ID " + std::to_string(id));
}

void
Game::Parse(int argc, char **argv)
{
  bool quiet = false;
  bool verbose = false;
  std::string log_filename = "stdout";
  bool train = false;

  PlayerSettings splayer1("human", 1);
  PlayerSettings splayer2("human", 2);
  PlayerSettings* active_splayer = nullptr;

  std::vector<struct option> long_options;
  for (const auto& o : game_options_) {
    long_options.push_back(o.LongOption());
  }
  long_options.push_back({nullptr, 0, nullptr, 0});

  // First pass through the arguments to see if --train is
  // specified. The flags must be parsed first to set game-specific
  // and training-specific defaults. Also parse help and logging
  // command here so that they are initialized as soon as possible.
  int flag;
  while ((flag = getopt_long(argc, argv, "", &long_options[0], NULL)) != -1) {
    switch (flag) {
    case BuiltInOptionId::OPTION_HELP:
    case '?':
      std::cout << Usage() << std::endl;
      exit(1);

    case BuiltInOptionId::OPTION_QUIET:
      verbose = false;
      quiet = true;
      break;
    case BuiltInOptionId::OPTION_VERBOSE:
      verbose = true;
      quiet = false;
      break;
    case BuiltInOptionId::OPTION_LOG_FILE:
      log_filename = optarg;
      break;

    case BuiltInOptionId::OPTION_TRAIN:
      train = true;
      break;
    case BuiltInOptionId::OPTION_TRAIN_DIR:
      train_dir_ = optarg;
      break;
    default:
      break;
    }
  }

  // Log level is set based on --quiet, --verbose and if human is
  // playing or not. If human is playing then default output log level
  // is MED as that is what is required to see the turn action
  // prompts. If human is not playing then default is LOW. Verbose
  // sets level to HIGH.
  common::Logger::Level log_level = common::Logger::Level::OFF;
  if (!quiet) {
    log_level = (verbose) ? common::Logger::Level::HIGH : common::Logger::Level::LOW;
  }

  logger_.reset(CreateLogger(log_filename, log_level, common::Logger::Timestamp::NONE));

  // Echo command-line...
  if (logger_->FilterLevel() != common::Logger::Level::OFF) {
    std::stringstream ss;
    for (auto i = 0; i < argc; ++i) {
      ss << argv[i] << " ";
    }
    logger_->Log(ss.str());
    logger_->Log(cifre::VersionToString(true /* show_dependent_versions */));
  }

  // Set default alphazero values based on --train.
  splayer1.alphazero_settings_ = DefaultAlphaZeroSettings(train);
  splayer2.alphazero_settings_ = DefaultAlphaZeroSettings(train);

  // Second pass through the arguments for everying not handled in the
  // first pass.
  optind = 1;
  while ((flag = getopt_long(argc, argv, "", &long_options[0], NULL)) != -1) {
    const GameOption& game_option = OptionIdToOption(flag);
    switch (flag) {
    case BuiltInOptionId::OPTION_HELP:
    case BuiltInOptionId::OPTION_QUIET:
    case BuiltInOptionId::OPTION_VERBOSE:
    case BuiltInOptionId::OPTION_LOG_FILE:
    case BuiltInOptionId::OPTION_TRAIN:
    case BuiltInOptionId::OPTION_TRAIN_DIR:
      // Handled in first pass...
      break;

    case BuiltInOptionId::OPTION_REPORT_FILE:
      report_filename_ = optarg;
      break;

    case BuiltInOptionId::OPTION_PARALLEL:
      parallel_ = GameOption::ParseUnsigned(game_option, optarg, 1 /* min */);
      break;
    case BuiltInOptionId::OPTION_GAME_COUNT:
      game_count_ = GameOption::ParseUnsigned(game_option, optarg, 1 /* min */);
      break;

    case BuiltInOptionId::OPTION_PLAYER1:
      splayer1.cmdline_specified_ = true;
      splayer1.kind_ = PlayerSettings::StringToKind(optarg);
      splayer1.name_ = splayer1.DefaultName();
      active_splayer = &splayer1;
      break;
    case BuiltInOptionId::OPTION_PLAYER2:
      splayer2.cmdline_specified_ = true;
      splayer2.kind_ = PlayerSettings::StringToKind(optarg);
      splayer2.name_ = splayer2.DefaultName();
      active_splayer = &splayer2;
      break;
    case BuiltInOptionId::OPTION_PLAYER_NAME:
      GameCheckTrueMsg(active_splayer != nullptr,
                       "--player1 or --player2 must be specified before --name");
      active_splayer->name_ = optarg;
      break;
    case BuiltInOptionId::OPTION_PLAYER_STARTING:
      GameCheckTrueMsg(active_splayer != nullptr,
                       "--player1 or --player2 must be specified before --starting");
      active_splayer->starting_ = true;
      break;
    case BuiltInOptionId::OPTION_PLAYER_ALPHAZERO_MODEL:
      GameCheckTrueMsg(active_splayer != nullptr,
                       "--player1 or --player2 must be specified before --alphazero-model");
      active_splayer->alphazero_settings_.model_name_ = optarg;
      break;
    case BuiltInOptionId::OPTION_PLAYER_ALPHAZERO_SEARCH_COUNT:
      GameCheckTrueMsg(active_splayer != nullptr,
                       "--player1 or --player2 must be specified before --alphazero-search-count");
      active_splayer->alphazero_settings_.search_count_ =
        GameOption::ParseUnsigned(game_option, optarg, 1 /* min */);
      break;
    case BuiltInOptionId::OPTION_PLAYER_ALPHAZERO_SEARCH_DEPTH:
      GameCheckTrueMsg(active_splayer != nullptr,
                       "--player1 or --player2 must be specified before --alphazero-search-depth");
      active_splayer->alphazero_settings_.max_search_depth_ =
        GameOption::ParseUnsigned(game_option, optarg, 1 /* min */);
      break;
    case BuiltInOptionId::OPTION_PLAYER_ALPHAZERO_CPUCT:
      GameCheckTrueMsg(active_splayer != nullptr,
                       "--player1 or --player2 must be specified before --alphazero-cpuct");
      active_splayer->alphazero_settings_.cpuct_ =
        GameOption::ParseFloat(game_option, optarg, 0.0 /* min */);
      break;
    default:
      // Must be a custom option... allow derived game to process...
      GameCheckTrueMsg(ParseCustomOption(game_option, optarg),
                       "unrecognized option " + game_option.FlagWithDash());
      break;
    }
  }

  if (!train && ((splayer1.kind_ == PlayerSettings::Kind::HUMAN) ||
                 (splayer2.kind_ == PlayerSettings::Kind::HUMAN))) {
    GameCheckTrueMsg(logger_->FilterLevel() != common::Logger::Level::OFF,
                     "--quiet cannot be specified if there is a human player");
    if (!verbose) {
      logger_->SetFilterLevel(common::Logger::Level::MED);
    }
  }

  // At most one player may be marked as --starting. If none are
  // specified then start with player1.
  GameCheckTrueMsg(!splayer1.starting_ || !splayer2.starting_,
                   "--starting may only be specified for a single player");
  starting_player_idx_ = (splayer2.starting_) ? 1 : 0;

  // If training, then 'train_dir_' must exist and --player1 must be
  // specified as alphazero.
  GameCheckTrueMsg(!train || std::filesystem::is_directory(train_dir_),
                   "--train-dir must be an existing directory");
  GameCheckTrueMsg(!train || (splayer1.cmdline_specified_ &&
                              (splayer1.kind_ == PlayerSettings::Kind::ALPHAZERO)),
                   "--train requires --player1=alphazero");

  // Alphazero initialization if required.
  if ((splayer1.kind_ == PlayerSettings::Kind::ALPHAZERO) ||
      (splayer2.kind_ == PlayerSettings::Kind::ALPHAZERO)) {
    // Create shared triton...
    triton_server_.reset(new TritonServer("http://localhost:8000", verbose));
    GameCheckTrueMsg(triton_server_->IsReady(), "triton server is not ready");

    // Load any models needed by player1 or player2. Before loading,
    // make sure that a model of the same name as the model being used
    // by player1 or player2 is not already loaded into Triton. If
    // there is an old model of the same name still loaded then unload
    // it so that it is not used by mistake in place of the specified
    // model.
    const auto& triton_repo = triton_server_->ModelRepositoryIndex();
    if (splayer1.kind_ == PlayerSettings::Kind::ALPHAZERO) {
      GameCheckTrueMsg(!splayer1.alphazero_settings_.model_name_.empty(),
                       "--player1=alphazero requires --alphazero-model");
      if (triton_repo.find(splayer1.alphazero_settings_.model_name_) != triton_repo.end()) {
        GameCheckTrueMsg(triton_server_->UnloadModel(splayer1.alphazero_settings_.model_name_),
                         "failed to unload existing player1 model '" +
                         splayer1.alphazero_settings_.model_name_ + "'");
      }

      GameCheckTrueMsg(triton_server_->LoadModel(
                         splayer1.alphazero_settings_.model_name_, true /* cache */),
                       "failed to load player1 model '" +
                       splayer1.alphazero_settings_.model_name_ + "'");
    }
    if (splayer2.kind_ == PlayerSettings::Kind::ALPHAZERO) {
      GameCheckTrueMsg(!splayer2.alphazero_settings_.model_name_.empty(),
                       "--player2=alphazero requires --alphazero-model");
      if (triton_repo.find(splayer2.alphazero_settings_.model_name_) != triton_repo.end()) {
        GameCheckTrueMsg(triton_server_->UnloadModel(splayer2.alphazero_settings_.model_name_),
                         "failed to unload existing player2 model '" +
                         splayer2.alphazero_settings_.model_name_ + "'");
      }

      GameCheckTrueMsg(triton_server_->LoadModel(
                         splayer2.alphazero_settings_.model_name_, true /* cache */),
                       "failed to load player2 model '" +
                       splayer2.alphazero_settings_.model_name_ + "'");
    }

    // If training and --player2 is not specified, then initialize
    // player1 and player2 settings for self-play.
    if (train && !splayer2.cmdline_specified_) {
      splayer2 = splayer1;
      splayer2.cmdline_specified_ = false;
      splayer2.id_ = 2;
      splayer1.name_ = "self1";
      splayer2.name_ = "self2";
    }

    // Create shared object for collecting alphazero training samples.
    if (train) {
      alphazero_train_samples_ =
        std::make_unique<AlphaZeroTrainingSamples>(
          logger_, AlphaZeroTrainingSamples::AggregateKind::AVERAGE,
          AlphaZeroTrainingSamples::ValueKind::AVERAGE);
    }
  }

  // Put players settings into a const vector as they are not allowed
  // to be changed after this point.
  const auto ps = new std::vector<PlayerSettings> {std::move(splayer1), std::move(splayer2)};
  player_settings_.reset(ps);
}

void
Game::Play()
{
  try {
    const std::vector<cifre::PlayerSettings>& psettings = *player_settings_;
    std::unique_ptr<AlphaZeroStatistics> alphazero_statistics;

    // PlayOneTask objects are enqueued into a TaskPool and then this
    // thread pulls each completed game from the TaskPool's completed
    // queue, aggregates the results and then re-enqueues the task for
    // another game (if necessary).
    common::TaskPool pool(parallel_);
    std::vector<std::unique_ptr<common::TaskPool::Task>> completed_tasks;

    // Create enough PlayOneTask objects to keep all pool threads
    // busy, but not more than 'game_count'. Each task gets its own
    // Players and AlphaZeros so that it can execute independently of
    // other tasks.
    size_t games_started = std::min(game_count_, std::max(size_t(2), size_t(parallel_ * 1.5)));
    for (size_t gidx = 0; gidx < games_started; ++gidx) {
      // Create AlphaZero objects. If only player1 was specified on
      // cmdline and is alphazero, then share a single alphazero
      // object between player1 and player2 to get self-play.
      std::vector<std::shared_ptr<AlphaZero>> alphazeros(2);
      if ((psettings[0].kind_ == PlayerSettings::Kind::ALPHAZERO) ||
          (psettings[1].kind_ == PlayerSettings::Kind::ALPHAZERO)) {
        // Collect statistics from alphazero...
        alphazero_statistics = std::make_unique<AlphaZeroStatistics>();

        if (psettings[0].kind_ == PlayerSettings::Kind::ALPHAZERO) {
          std::unique_ptr<TritonModel> triton_model(
            triton_server_->CreateTritonModel(psettings[0].alphazero_settings_.model_name_));
          alphazeros[0] = std::make_shared<AlphaZero>(
            logger_, std::move(triton_model),
            psettings[0].alphazero_settings_);
        }
        if (psettings[1].kind_ == PlayerSettings::Kind::ALPHAZERO) {
          if (!psettings[1].cmdline_specified_) {
            alphazeros[1] = alphazeros[0];
          }
          else {
            std::unique_ptr<TritonModel> triton_model(
              triton_server_->CreateTritonModel(psettings[1].alphazero_settings_.model_name_));
            alphazeros[1] = std::make_shared<AlphaZero>(
              logger_, std::move(triton_model),
              psettings[1].alphazero_settings_);
          }
        }
      }

      // Create players. The players are collected in a vector and each
      // player's position in the vector is the players ID.
      std::vector<std::unique_ptr<Player>> players;
      players.emplace_back(CreatePlayer(psettings[0], alphazeros[0]));
      players.emplace_back(CreatePlayer(psettings[1], alphazeros[1]));

      // Initial state for all games.
      std::unique_ptr<State> istate(
        InitialState({ players[0].get(), players[1].get() }, starting_player_idx_));

      pool.Enqueue(std::make_unique<PlayOneTask>(
                     logger_, std::move(istate),
                     std::move(players), std::move(alphazeros),
                     max_turns_per_game_));
    }

    // Dequeue completed tasks, aggregate results and re-enqueue the
    // task until all the games have been completed.
    size_t games_completed = 0;
    size_t games_quit = 0;
    size_t draw_count = 0;
    size_t winner1_count = 0;
    size_t winner2_count = 0;
    while ((games_completed + games_quit) < game_count_) {
      std::unique_ptr<common::TaskPool::Task> task;
      THROWIF(!pool.Dequeue(task), "failed to dequeue game result");

      std::unique_ptr<PlayOneTask> ptask(dynamic_cast<PlayOneTask*>(task.release()));
      PlayOneTask::GameResult presult = ptask->Result();
      switch (presult) {
      case PlayOneTask::GameResult::QUIT:
        games_quit++;
        break;
      case PlayOneTask::GameResult::DRAW:
        games_completed++;
        draw_count++;
        break;
      case PlayOneTask::GameResult::WINNER1:
        games_completed++;
        winner1_count++;
        break;
      case PlayOneTask::GameResult::WINNER2:
        games_completed++;
        winner2_count++;
        break;
      }

      // Aggregate the training samples.
      if (alphazero_train_samples_ != nullptr) {
        alphazero_train_samples_->AcquireAndAggregate(
          std::move(ptask->AcquireTrainSamples()));
      }

      // Collect alphazero statistics
      if (alphazero_statistics != nullptr) {
        *alphazero_statistics += ptask->AlphaZeroStats();
      }

      // If 'game_count_' games have not yet been started, then
      // re-enqueue 'ptask' to play another game. If we don't need
      // 'ptask' anymore then put it in 'completed_tasks', because we
      // need the lifetime of the tasks to extend to the end of the
      // function since we access their contents below (sometimes
      // indirectly, like accessing Player objects in State::ToString
      // function).
      if (games_started < game_count_) {
        pool.Enqueue(std::move(ptask));
        games_started++;
      }
      else {
        completed_tasks.push_back(std::move(ptask));
      }
    }

    // End of all games output...
    logger_->LogLow("==== " + std::to_string(games_completed) + " game" +
                    ((games_completed == 1) ? " " : "s ") + "completed");
    if (games_completed > 0) {
      logger_->LogLow(psettings[0].name_ + ": " + std::to_string(winner1_count) +
                      " (" + FloatToString(100.0 * winner1_count / games_completed, 1 /* precision */) + "%)",
                      4 /* indent */);
      logger_->LogLow(psettings[1].name_ + ": " + std::to_string(winner2_count) +
                      " (" + FloatToString(100.0 * winner2_count / games_completed, 1 /* precision */) + "%)",
                      4 /* indent */);
      logger_->LogLow("draw: " + std::to_string(draw_count) +
                      " (" + FloatToString(100.0 * draw_count / games_completed, 1 /* precision */) + "%)",
                      4 /* indent */);
    }

    // Alphazero statistics, if any...
    if (alphazero_statistics != nullptr) {
      logger_->LogLow("AlphaZero");
      logger_->LogLow(alphazero_statistics->ToString(), 4 /* indent */);
    }

    // Triton statistics, if any...
    if (triton_server_ != nullptr) {
      logger_->LogLow("Triton");
      logger_->LogLow(triton_server_->ToString(), 4 /* indent */);
    }

    // Write report, if requested.
    if (!report_filename_.empty()) {
      std::ofstream fr(report_filename_, std::ios::out);
      fr
        << "{\n"
        << "  \"game-results\" : {\n"
        << "    \"count\" : " << games_completed << ",\n"
        << "    \"player1\" : " << winner1_count << ",\n"
        << "    \"player2\" : " << winner2_count << ",\n"
        << "    \"draw\" : " << draw_count << "\n"
        << "  }\n"
        << "}\n";
      fr.close();
    }

    // Write alphazero training data.
    if (!train_dir_.empty() && (alphazero_train_samples_ != nullptr)) {
      alphazero_train_samples_->WriteTraining(train_dir_);
    }
  } catch (const std::exception& ex) {
    GameFail(ex.what());
  }
}

Player*
Game::CreatePlayer(
  const PlayerSettings& player_settings, const std::shared_ptr<AlphaZero>& alphazero) const
{
  const char bc = (player_settings.id_ == 1) ? 'o' : 'x';

  switch (player_settings.kind_) {
  case PlayerSettings::Kind::UNKNOWN:
    break;
  case PlayerSettings::Kind::HUMAN:
    assert(alphazero == nullptr);
    return CreateHumanPlayer(logger_, player_settings.name_, bc);
  case PlayerSettings::Kind::RANDOM:
    assert(alphazero == nullptr);
    return new RandomPlayer(logger_, player_settings.name_, bc);
  case PlayerSettings::Kind::ALPHAZERO:
    assert(alphazero != nullptr);
    return new AlphaZeroPlayer(logger_, player_settings.name_, bc, alphazero);
  }

  GameFail("cannot create player '" + player_settings.name_ + "'");
}

} // namespace cifre
