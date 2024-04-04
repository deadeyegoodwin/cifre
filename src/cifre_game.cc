// Copyright 2024, David Goodwin. All rights reserved.

#include <filesystem>
#include <iostream>
#include <sstream>
#include <vector>

#include "cifre_game.h"
#include "cifre_version.h"
#include "common_error.h"
#include "common_logger.h"
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
// Settings for a player.
//
struct Game::PlayerSettings {
  enum class Kind { UNKNOWN, HUMAN, RANDOM, ALPHAZERO };

  PlayerSettings(const std::string& kind_str, size_t id) :
    kind_(StringToKind(kind_str)), id_(id), name_(DefaultName()) { }

  static Kind StringToKind(const std::string& str) {
    if (str == "human") { return Kind::HUMAN; }
    if (str == "random") { return Kind::RANDOM; }
    if (str == "alphazero") { return Kind::ALPHAZERO; }
    return Kind::UNKNOWN;
  }

  static std::string KindToString(const Kind kind) {
    switch (kind) {
    case Kind::UNKNOWN: return "<unknown>";
    case Kind::HUMAN: return "human";
    case Kind::RANDOM: return "random";
    case Kind::ALPHAZERO: return "alphazero";
    }
    return "<none>";
  }

  std::string DefaultName() const {
    return PlayerSettings::KindToString(kind_) + std::to_string(id_);
  }

  Kind kind_ {Kind::UNKNOWN};
  bool cmdline_specified_ {false};
  size_t id_;
  std::string name_;
  bool starting_ {false};
  AlphaZeroSettings alphazero_settings_;
};


// Play one game, return PlayResult indicating outcome.
enum class PlayResult { WINNER1, WINNER2, DRAW, QUIT };
PlayResult
PlayOne(const std::shared_ptr<common::Logger>& logger,
        std::unique_ptr<State> state,
        const std::vector<Player*>& players,
        size_t game_turns_remaining)
{
  // Let all players know a new game is starting.
  for (Player* p : players) {
    p->GameStart();
  }

  const auto starting_player_id = state->ActivePlayerId();

  bool game_quit = false;
  StateTerminalStatus terminal_status = state->TerminalStatus();
  while (!game_quit &&
         (game_turns_remaining > 0) &&
         (terminal_status.Kind() == StateTerminalStatus::TerminalKind::NON_TERMINAL)) {
    const auto player_id = state->ActivePlayerId();
    const Player* player = players[player_id];
    const Action action = player->NextAction(*state, game_turns_remaining);
    switch (action.Kind()) {
    case Action::ActionKind::INVALID:
    case Action::ActionKind::HELP:
    case Action::ActionKind::SHOW_BOARD:
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
    LOGGER_LOGMED(*logger, "game quit: no winner");
    LOGGER_LOGMED(*logger, state->ToString());
    return PlayResult::QUIT;
  }

  // If ran out of game turns, then game is a draw.
  if (game_turns_remaining == 0) {
    terminal_status = StateTerminalStatus(StateTerminalStatus::TerminalKind::DRAW);
  }

  // Let all players know game is completed.
  for (Player* p : players) {
    p->GameEnd(*state, terminal_status);
  }

  // Game completed as a draw.
  if (terminal_status.Kind() == StateTerminalStatus::TerminalKind::DRAW) {
    LOGGER_LOGMED(*logger, "game over: draw");
    LOGGER_LOGMED(*logger, state->ToString());
    return PlayResult::DRAW;
  }

  // Game completed with a winner.
  const size_t winner_id = terminal_status.WinnerId();

  // FIXME only supports 2 player games
  return (winner_id == 0) ? PlayResult::WINNER1 : PlayResult::WINNER2;
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
  initialized_(false), game_count_(1), starting_player_idx_(0)
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

  splayer1_ = std::make_shared<PlayerSettings>("human", 1);
  splayer2_ = std::make_shared<PlayerSettings>("human", 2);
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
  splayer1_->alphazero_settings_ = DefaultAlphaZeroSettings(train);
  splayer2_->alphazero_settings_ = DefaultAlphaZeroSettings(train);

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

    case BuiltInOptionId::OPTION_GAME_COUNT:
      game_count_ = GameOption::ParseUnsigned(game_option, optarg, 1 /* min */);
      break;

    case BuiltInOptionId::OPTION_PLAYER1:
      splayer1_->cmdline_specified_ = true;
      splayer1_->kind_ = PlayerSettings::StringToKind(optarg);
      splayer1_->name_ = splayer1_->DefaultName();
      active_splayer = splayer1_.get();
      break;
    case BuiltInOptionId::OPTION_PLAYER2:
      splayer2_->cmdline_specified_ = true;
      splayer2_->kind_ = PlayerSettings::StringToKind(optarg);
      splayer2_->name_ = splayer2_->DefaultName();
      active_splayer = splayer2_.get();
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

  if (!train && ((splayer1_->kind_ == PlayerSettings::Kind::HUMAN) ||
                 (splayer2_->kind_ == PlayerSettings::Kind::HUMAN))) {
    GameCheckTrueMsg(logger_->FilterLevel() != common::Logger::Level::OFF,
                     "--quiet cannot be specified if there is a human player");
    if (!verbose) {
      logger_->SetFilterLevel(common::Logger::Level::MED);
    }
  }

  // At most one player may be marked as --starting. If none are
  // specified then start with player1.
  GameCheckTrueMsg(!splayer1_->starting_ || !splayer2_->starting_,
                   "--starting may only be specified for a single player");
  starting_player_idx_ = (splayer2_->starting_) ? 1 : 0;

  // If training, then 'train_dir_' must exist and --player1 must be
  // specified as alphazero.
  GameCheckTrueMsg(!train || std::filesystem::is_directory(train_dir_),
                   "--train-dir must be an existing directory");
  GameCheckTrueMsg(!train || (splayer1_->cmdline_specified_ &&
                              (splayer1_->kind_ == PlayerSettings::Kind::ALPHAZERO)),
                   "--train requires --player1=alphazero");

  // Create shared triton and alphazero aggregator object(s) if
  // needed.
  if ((splayer1_->kind_ == PlayerSettings::Kind::ALPHAZERO) ||
      (splayer2_->kind_ == PlayerSettings::Kind::ALPHAZERO)) {
    triton_.reset(new Triton(logger_));
    GameCheckTrueMsg(triton_->IsReady(), "triton is not ready");

    // Make sure that a model of the same name as the model being used
    // by player1 or player2 is not already loaded into Triton. If
    // there is an old model of the same name still loaded then unload
    // it so that it is not used by mistake in place of the specified
    // model.
    const auto& triton_repo = triton_->ModelRepositoryIndex();
    if (splayer1_->kind_ == PlayerSettings::Kind::ALPHAZERO) {
      GameCheckTrueMsg(!splayer1_->alphazero_settings_.model_name_.empty(),
                       "--player1=alphazero requires --alphazero-model");
      if (triton_repo.find(splayer1_->alphazero_settings_.model_name_) != triton_repo.end()) {
        triton_->UnloadModel(splayer1_->alphazero_settings_.model_name_);
      }
    }
    if (splayer2_->kind_ == PlayerSettings::Kind::ALPHAZERO) {
      GameCheckTrueMsg(!splayer2_->alphazero_settings_.model_name_.empty(),
                       "--player2=alphazero requires --alphazero-model");
      if (triton_repo.find(splayer2_->alphazero_settings_.model_name_) != triton_repo.end()) {
        triton_->UnloadModel(splayer2_->alphazero_settings_.model_name_);
      }
    }

    // If training and --player2 is not specified, then initialize
    // player2 settings for self-play.
    if (train && !splayer2_->cmdline_specified_) {
      *splayer2_ = *splayer1_;
      splayer2_->cmdline_specified_ = false;
      splayer2_->id_ = 2;
      splayer1_->name_ = "self1";
      splayer2_->name_ = "self2";
    }

    if (train) {
      alphazero_aggregator_ =
        std::make_unique<AlphaZeroAggregator>(
          logger_, AlphaZeroAggregator::AggregateKind::AVERAGE,
          AlphaZeroAggregator::ValueKind::AVERAGE);
    }
  }
}

void
Game::Play()
{
  // Play game --game-count times starting at the initial state each
  // game. Each game episode gets its own state, players, and
  // alpha-zero objects so that they can be executed independently.
  size_t games_completed = 0;
  size_t draw_count = 0;
  size_t winner1_count = 0;
  size_t winner2_count = 0;
  for (size_t gidx = 0; gidx < game_count_; gidx++) {
    if (game_count_ > 1) {
      logger_->LogMed("====");
      logger_->LogMed("==== Game " + std::to_string(gidx + 1) + " of " + std::to_string(game_count_) + " ====");
      logger_->LogMed("====");
    }

    // Create players for this game. If only player1 was specified,
    // then share a single alphazero object between player1 and
    // player2 to get self-play.
    std::shared_ptr<AlphaZero> alphazero1, alphazero2;
    if ((splayer1_->kind_ == PlayerSettings::Kind::ALPHAZERO) ||
        (splayer2_->kind_ == PlayerSettings::Kind::ALPHAZERO)) {
      if (splayer1_->kind_ == PlayerSettings::Kind::ALPHAZERO) {
        alphazero1 = std::make_shared<AlphaZero>(
          logger_, triton_, splayer1_->alphazero_settings_);
      }
      if (splayer2_->kind_ == PlayerSettings::Kind::ALPHAZERO) {
        if (!splayer2_->cmdline_specified_) {
          alphazero2 = alphazero1;
        }
        else {
          alphazero2 = std::make_shared<AlphaZero>(
            logger_, triton_, splayer2_->alphazero_settings_);
        }
      }
    }

    // Create players. The players are collected in a vector and each
    // player's position in the vector is the players ID.
    std::unique_ptr<Player> player1(CreatePlayer(*splayer1_, alphazero1));
    std::unique_ptr<Player> player2(CreatePlayer(*splayer2_, alphazero2));
    std::vector<Player*> players { player1.get(), player2.get() };

    // Create initial state for the game.
    std::unique_ptr<State> initial_state(InitialState(players, starting_player_idx_));

    // Play the game...
    PlayResult presult = PlayOne(
      logger_, std::move(initial_state), players, max_turns_per_game_);
    switch (presult) {
    case PlayResult::QUIT:
      break;
    case PlayResult::DRAW:
      games_completed++;
      draw_count++;
      break;
    case PlayResult::WINNER1:
      games_completed++;
      winner1_count++;
      break;
    case PlayResult::WINNER2:
      games_completed++;
      winner2_count++;
      break;
    }

    // Aggregate the training samples and statistics. The aggregation
    // takes ownership of some AlphaZero state, rendering the node
    // tree invalid, thus the AlphaZero object is Reset() within
    // Aggregate().
    if (alphazero_aggregator_ != nullptr) {
      if (alphazero1 != nullptr) {
        alphazero_aggregator_->Aggregate(alphazero1.get());
      }
      if ((alphazero2 != nullptr) && (alphazero2 != alphazero1)) {
        alphazero_aggregator_->Aggregate(alphazero2.get());
      }
    }
  }

  // End of all games output...
  logger_->LogLow("==== " + std::to_string(games_completed) + " game" +
                  ((games_completed == 1) ? " " : "s ") + "completed");
  if (games_completed > 0) {
    logger_->LogLow(splayer1_->name_ + ": " + std::to_string(winner1_count) +
                    " (" + FloatToString(100.0 * winner1_count / games_completed, 1 /* precision */) + "%)",
                    4 /* indent */);
    logger_->LogLow(splayer2_->name_ + ": " + std::to_string(winner2_count) +
                    " (" + FloatToString(100.0 * winner2_count / games_completed, 1 /* precision */) + "%)",
                    4 /* indent */);
    logger_->LogLow("draw: " + std::to_string(draw_count) +
                    " (" + FloatToString(100.0 * draw_count / games_completed, 1 /* precision */) + "%)",
                    4 /* indent */);
  }

  // Aggregated alphazero statistics, if any...
  if (alphazero_aggregator_ != nullptr) {
    logger_->LogLow("AlphaZero");
    logger_->LogLow(alphazero_aggregator_->ToString(), 4 /* indent */);
  }

  // Triton statistics, if any...
  if (triton_ != nullptr) {
    logger_->LogLow("Triton");
    logger_->LogLow(triton_->ToString(), 4 /* indent */);
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
  if (!train_dir_.empty() && (alphazero_aggregator_ != nullptr)) {
    alphazero_aggregator_->WriteTraining(train_dir_);
  }
}

Player*
Game::CreatePlayer(
  const Game::PlayerSettings& player_settings, const std::shared_ptr<AlphaZero>& alphazero) const
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
    return new RandomPlayer(logger_, player_settings.name_, bc, 1.0);
  case PlayerSettings::Kind::ALPHAZERO:
    assert(alphazero != nullptr);
    return new AlphaZeroPlayer(logger_, player_settings.name_, bc, 1.0, alphazero);
  }

  GameFail("cannot create player '" + player_settings.name_ + "'");
}

} // namespace cifre
