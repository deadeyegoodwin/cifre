// Copyright 2024, David Goodwin. All rights reserved.

#include <cstdint>
#include <filesystem>
#include <getopt.h>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "cifre_action.h"
#include "tictactoe_board.h"
#include "tictactoe_player.h"
#include "tictactoe_version.h"
#include "common_error.h"
#include "common_logger.h"
#include "common_util.h"

namespace {

#define _stringify(A) #A
#define CheckTrue(P)                                                    \
  do {                                                                  \
    const bool p__ = (P);                                               \
    if (!p__) {                                                         \
      std::cerr << std::string("error: expected 'true' for: " _stringify(P)) << std::endl; \
      exit(1);                                                          \
    }                                                                   \
  } while (false)

#define CheckTrueMsg(P, MSG)                                            \
  do {                                                                  \
    const bool p__ = (P);                                               \
    if (!p__) {                                                         \
      std::cerr << std::string("error: ") << (MSG) << std::endl;        \
      exit(1);                                                          \
    }                                                                   \
  } while (false)

#define Fail(MSG)                                                     \
  do {                                                                \
    std::cerr << "error: " << (MSG) << std::endl;                     \
    exit(1);                                                          \
  } while (false)

//
// Command-line options and parsing.
//
static const cifre::BoardSize2D DEFAULT_BOARD_SIZE {3, 3};

struct Option {
  static constexpr const char* ArgNone = "";
  static constexpr const char* ArgBool = "boolean";
  static constexpr const char* ArgFloat = "float";
  static constexpr const char* ArgInt = "integer";
  static constexpr const char* ArgStr = "string";

  enum OptionId {
    OPTION_HELP = 1000,
    OPTION_QUIET,
    OPTION_VERBOSE,
    OPTION_LOG_FILE,
    OPTION_REPORT_FILE,

    OPTION_TRAIN,
    OPTION_TRAIN_DIR,

    OPTION_BOARD_SIZE,
    OPTION_GAME_COUNT,

    OPTION_PLAYER1,
    OPTION_PLAYER2,
    OPTION_PLAYER_NAME,
    OPTION_PLAYER_STARTING,
    OPTION_PLAYER_MODEL,
    OPTION_PLAYER_ALPHAZERO_UPDATE_COUNT,
    OPTION_PLAYER_ALPHAZERO_CPUCT
  };

  Option(OptionId id, std::string flag, std::string arg_desc, std::string desc)
      : id_(id), flag_(flag), arg_desc_(arg_desc), desc_(desc)
  {
  }

  struct option GetLongOption() const
  {
    struct option lo {
      flag_.c_str(), (!arg_desc_.empty()) ? required_argument : no_argument,
          nullptr, id_
    };
    return lo;
  }

  const OptionId id_;
  const std::string flag_;
  const std::string arg_desc_;
  const std::string desc_;
};

const std::vector<Option> options_ {
  {Option::OPTION_HELP, "help", Option::ArgNone, "Print usage"},

  {Option::OPTION_QUIET, "quiet", Option::ArgNone,
   "Disable all output."},
  {Option::OPTION_VERBOSE, "verbose", Option::ArgNone,
   "Enable verbose output."},
  {Option::OPTION_LOG_FILE, "log-file", Option::ArgStr,
   "File for console output. 'stdout' and 'stderr' are allowed."},
  {Option::OPTION_REPORT_FILE, "report-file", Option::ArgStr,
   "File for report output. Default is no report output."},

  {Option::OPTION_TRAIN, "train", Option::ArgNone,
   "Use self-play to generate training data. --train-dir must be specified."},
  {Option::OPTION_TRAIN_DIR, "train-dir", Option::ArgStr,
   "Training data directory. Has no effect if --train is not also specified."},

  {Option::OPTION_BOARD_SIZE, "board-size", Option::ArgInt,
   "Size of the game board. A square board with N rows and N columns is specified as '--board-size=N'. "
   "A rectangular board with N rows and M columns is specified as '--board-size=NxM'. "
   "Default is '--board-size=" + DEFAULT_BOARD_SIZE.ToString() + "'."},
  {Option::OPTION_GAME_COUNT, "game-count", Option::ArgInt,
   "Number of games to play. Default is 1."},

  {Option::OPTION_PLAYER1, "player1", Option::ArgStr,
   "Type of the first player: 'human', 'random', 'alphazero'."},
  {Option::OPTION_PLAYER2, "player2", Option::ArgStr,
   "Type of the first player: 'human', 'random', 'alphazero'."},
  {Option::OPTION_PLAYER_NAME, "name", Option::ArgStr,
   "Name of player. --player1 or --player2 must be specified before this argument. "
   "The default name is automatically generated from the player kind."},
  {Option::OPTION_PLAYER_STARTING, "starting", Option::ArgNone,
   "Indicate that the player should start the game. May only be indicated for a "
   "single player. --player1 or --player2 must be specified before this argument."},
  {Option::OPTION_PLAYER_MODEL, "model", Option::ArgStr,
   "Model associated with player. --player1 or --player2 must be specified "
   "before this argument."},
  {Option::OPTION_PLAYER_ALPHAZERO_UPDATE_COUNT, "alphazero-update-count", Option::ArgInt,
   "Number of updates (searches) made to alphazero tree before "
   "choosing the next action for the player. --player1 or --player2 must "
   "be specified before this argument."},
  {Option::OPTION_PLAYER_ALPHAZERO_CPUCT, "alphazero-cpuct", Option::ArgFloat,
   "AlphaZero cpuct value."}};

size_t
ParseUnsigned(const std::string& str, size_t min_value = 0)
{
  std::string::size_type idx;
  size_t ul = std::stoull(str, &idx);
  CheckTrueMsg(idx == str.size(),
               "unable to parse argument '" + str + "' as unsigned integer");
  CheckTrueMsg(ul >= min_value,
               "expected unsigned integer value >= " + std::to_string(min_value) +
               ", got " + std::to_string(ul));
  return ul;
}

float
ParseFloat(const std::string& str, float min_value = -std::numeric_limits<float>::max())
{
  std::string::size_type idx;
  float f = std::stof(str, &idx);
  CheckTrueMsg(idx == str.size(),
               "unable to parse argument '" + str + "' as float");
  CheckTrueMsg(f >= min_value,
               "expected float value >= " + std::to_string(min_value) +
               ", got " + std::to_string(f));
  return f;
}

std::string
FloatToString(float f, size_t precision = 2)
{
  std::stringstream ss;
  ss << std::fixed << std::setprecision(precision) << f;
  return ss.str();
}

std::string
FormatUsageMessage(std::string str, int offset)
{
  int width = 60;
  int current_pos = offset;
  while (current_pos + width < int(str.length())) {
    int n = str.rfind(' ', current_pos + width);
    if (n != int(std::string::npos)) {
      str.replace(n, 1, "\n\t");
      current_pos += (width + 9);
    }
  }

  return str;
}

std::string
Usage()
{
  std::stringstream ss;

  ss << "usage: tictactoe [options]" << std::endl;
  for (const auto& o : options_) {
    if (!o.arg_desc_.empty()) {
      ss << "  --" << o.flag_ << " <" << o.arg_desc_ << ">" << std::endl
         << "\t" << FormatUsageMessage(o.desc_, 0) << std::endl;
    }
    else {
      ss << "  --" << o.flag_ << std::endl
         << "\t" << FormatUsageMessage(o.desc_, 0) << std::endl;
    }
  }

  return ss.str();
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

//
// Settings for each player.
//
struct PlayerSettings {
  enum class Kind { UNKNOWN, HUMAN, RANDOM, ALPHAZERO };

  PlayerSettings(const std::string& kind_str, size_t id) :
    kind_(StringToKind(kind_str)), id_(id), name_(DefaultName()) { }

  static Kind StringToKind(const std::string& str) {
    if (str == "human") { return Kind::HUMAN; }
    if (str == "random") { return Kind::RANDOM; }
    if (str == "alphazero") { return Kind::ALPHAZERO; }
    Fail("unknown player kind '" + str + "'");
    return Kind::UNKNOWN;
  }

  static std::string KindToString(const Kind kind) {
    switch (kind) {
    case Kind::UNKNOWN: return "<unknown>";
    case Kind::HUMAN: return "human";
    case Kind::RANDOM: return "random";
    case Kind::ALPHAZERO: return "alphazero";
    }
    Fail("unknown player kind");
    return "<none>";
  }

  std::string DefaultName() const {
    return PlayerSettings::KindToString(kind_) + std::to_string(id_);
  }

  Kind kind_ {Kind::UNKNOWN};
  size_t id_;
  std::string name_;
  bool starting_ {false};
  std::string model_;
  cifre::AlphaZeroSettings alphazero_settings_;
};

cifre::tictactoe::Player*
CreatePlayer(const std::shared_ptr<common::Logger>& logger, const PlayerSettings& player_settings,
             const std::shared_ptr<cifre::AlphaZero>& alphazero)
{
  const char bc = (player_settings.id_ == 1) ? 'o' : 'x';

  switch (player_settings.kind_) {
  case PlayerSettings::Kind::UNKNOWN:
    break;
  case PlayerSettings::Kind::HUMAN:
    assert(alphazero == nullptr);
    return new cifre::tictactoe::HumanPlayer(logger, player_settings.name_, bc, 1.0);
  case PlayerSettings::Kind::RANDOM:
    assert(alphazero == nullptr);
    return new cifre::tictactoe::RandomPlayer(logger, player_settings.name_, bc, 1.0);
  case PlayerSettings::Kind::ALPHAZERO:
    assert(alphazero != nullptr);
    return new cifre::tictactoe::AlphaZeroPlayer(logger, player_settings.name_, bc, 1.0, alphazero);
  }

  Fail("cannot create player '" + player_settings.name_ + "'");
}

void
InitializeAlphaZeroForTraining(cifre::AlphaZeroSettings* settings)
{
  settings->training_ = true;
  settings->next_action_kind_ =
    cifre::AlphaZeroSettings::NextActionKind::RANDOM_VISIT_WEIGHTED;
  settings->dirichlet_alpha_ = std::min(1.0, 10.0 / 9.0 /* FIXME need total-action-id-count from game */);
  settings->max_depth_ = 20; // FIXME, should be adjusted after each turn
  settings->cpuct_ = 4.0;
}

}  // namespace

namespace cifre { namespace tictactoe {

enum class PlayResult { WINNER1, WINNER2, DRAW, QUIT };

// Play one game, return PlayResult indicating outcome.
PlayResult
Play(const std::shared_ptr<common::Logger>& logger,
     const std::vector<cifre::tictactoe::Player*>& players, Board* board)
{
  // Let all players know a new game is starting.
  for (Player* p : players) {
    p->GameStart();
  }

  bool game_quit = false;
  cifre::State::TerminalStatusKind terminal_status = board->TerminalStatus();
  while (!game_quit &&
         (terminal_status == cifre::State::TerminalStatusKind::NON_TERMINAL)) {
    Player* player = board->ActivePlayer();
    Action action = player->NextAction(*board);
    switch (action.Kind()) {
    case Action::ActionKind::INVALID:
    case Action::ActionKind::HELP:
    case Action::ActionKind::SHOW_BOARD:
      Fail("unexpected player action");
      break;

    case Action::ActionKind::QUIT:
      game_quit = true;
      break;

    case Action::ActionKind::ACTIONID:
      board->Update(action.ActionId());
      break;
    }

    terminal_status = board->TerminalStatus();
  }

  // Game was quit.
  if (game_quit) {
    LOGGER_LOGMED(*logger, "game quit: no winner");
    LOGGER_LOGMED(*logger, board->ToString());
    return PlayResult::QUIT;
  }

  // Let all players know game is completed.
  for (Player* p : players) {
    p->GameEnd(board, terminal_status);
  }

  // Game completed as a draw.
  if (terminal_status == cifre::State::TerminalStatusKind::DRAW) {
    LOGGER_LOGMED(*logger, "game over: draw");
    LOGGER_LOGMED(*logger, board->ToString());
    return PlayResult::DRAW;
  }

  // Game completed with a winner.
  const size_t winner_id =
    (terminal_status == cifre::State::TerminalStatusKind::ACTIVE_WINNER) ?
    board->ActivePlayerId() : board->OpponentPlayerId();
  LOGGER_LOGMED(*logger, "game over: winner " + players[winner_id]->Name());
  LOGGER_LOGMED(*logger, board->ToString());

  return (winner_id == 0) ? PlayResult::WINNER1 : PlayResult::WINNER2;
}

}} // namespace cifre::tictactoe

int
main(int argc, char **argv)
{
  try {
    bool quiet = false;
    bool verbose = false;
    std::string log_filename = "stdout";
    std::string report_filename;
    bool train = false;
    std::string train_dir;
    cifre::BoardSize2D board_size = DEFAULT_BOARD_SIZE;
    size_t game_count = 1;

    PlayerSettings splayer1("human", 1);
    PlayerSettings splayer2("human", 2);
    PlayerSettings* active_splayer = nullptr;
    bool player1_specified = false;
    bool player2_specified = false;

    std::vector<struct option> long_options;
    for (const auto& o : options_) {
      long_options.push_back(o.GetLongOption());
    }
    long_options.push_back({nullptr, 0, nullptr, 0});

    int flag;
    while ((flag = getopt_long(argc, argv, "", &long_options[0], NULL)) != -1) {
      switch (flag) {
      case Option::OPTION_HELP:
      case '?':
        std::cout << Usage() << std::endl;
        return 1;

      case Option::OPTION_QUIET:
        verbose = false;
        quiet = true;
        break;
      case Option::OPTION_VERBOSE:
        verbose = true;
        quiet = false;
        break;
      case Option::OPTION_LOG_FILE:
        log_filename = optarg;
        break;
      case Option::OPTION_REPORT_FILE:
        report_filename = optarg;
        break;

      case Option::OPTION_TRAIN:
        // For the alphazero setting defaults to be handled correctly,
        // --train must appear before any setting changes that happen
        // in --player1 or --player2.
        CheckTrueMsg(!player1_specified && !player2_specified,
                     "--train must be specified before --player1 and --player2");
        train = true;
        InitializeAlphaZeroForTraining(&splayer1.alphazero_settings_);
        InitializeAlphaZeroForTraining(&splayer2.alphazero_settings_);
        break;
      case Option::OPTION_TRAIN_DIR:
        train_dir = optarg;
        break;

      case Option::OPTION_BOARD_SIZE:
        board_size = cifre::BoardSize2D(optarg);
        break;
      case Option::OPTION_GAME_COUNT:
        game_count = ParseUnsigned(optarg);
        break;

      case Option::OPTION_PLAYER1:
        player1_specified = true;
        splayer1.kind_ = PlayerSettings::StringToKind(optarg);
        splayer1.name_ = splayer1.DefaultName();
        active_splayer = &splayer1;
        break;
      case Option::OPTION_PLAYER2:
        player2_specified = true;
        splayer2.kind_ = PlayerSettings::StringToKind(optarg);
        splayer2.name_ = splayer2.DefaultName();
        active_splayer = &splayer2;
        break;
      case Option::OPTION_PLAYER_NAME:
        CheckTrueMsg(active_splayer != nullptr,
                     "--player1 or --player2 must be specified before --name");
        active_splayer->name_ = optarg;
        break;
      case Option::OPTION_PLAYER_STARTING:
        CheckTrueMsg(active_splayer != nullptr,
                     "--player1 or --player2 must be specified before --starting");
        active_splayer->starting_ = true;
        break;
      case Option::OPTION_PLAYER_MODEL:
        CheckTrueMsg(active_splayer != nullptr,
                     "--player1 or --player2 must be specified before --model");
        active_splayer->model_ = optarg;
        break;
      case Option::OPTION_PLAYER_ALPHAZERO_UPDATE_COUNT:
        CheckTrueMsg(active_splayer != nullptr,
                     "--player1 or --player2 must be specified before --alphazero-update-count");
        active_splayer->alphazero_settings_.update_count_ = ParseUnsigned(optarg, 1);
        break;
      case Option::OPTION_PLAYER_ALPHAZERO_CPUCT:
        CheckTrueMsg(active_splayer != nullptr,
                     "--player1 or --player2 must be specified before --alphazero-cpuct");
        active_splayer->alphazero_settings_.cpuct_ = ParseFloat(optarg, 0.0);
        break;
      }
    }

    // Log level is set based on --quiet, --verbose and if human is
    // playing or not. If human is playing then default output log
    // level is MED as that is what is required to see the turn action
    // prompts. If human is not playing then default is LOW. Verbose
    // sets level to HIGH.
    common::Logger::Level log_level = common::Logger::Level::OFF;
    if (!quiet) {
      log_level = (verbose) ? common::Logger::Level::HIGH : common::Logger::Level::LOW;
    }

    if (!train && ((splayer1.kind_ == PlayerSettings::Kind::HUMAN) ||
                   (splayer2.kind_ == PlayerSettings::Kind::HUMAN))) {
      CheckTrueMsg(log_level != common::Logger::Level::OFF,
                   "--quiet cannot be specified if there is a human player");
      if (!verbose) {
        log_level = common::Logger::Level::MED;
      }
    }

    auto logger = std::shared_ptr<common::Logger>(
      CreateLogger(log_filename, log_level, common::Logger::Timestamp::NONE));

    // Echo command-line...
    if (logger->FilterLevel() != common::Logger::Level::OFF) {
      std::stringstream ss;
      for (auto i = 0; i < argc; ++i) {
        ss << argv[i] << " ";
      }
      logger->Log(ss.str());
      logger->Log(cifre::tictactoe::VersionToString(true /* show_dependent_versions */));
    }

    CheckTrueMsg(game_count >= 1, "--game-count must be >= 1");

    // At most one player may be marked as --starting. If none are
    // specified then start with player1.
    CheckTrueMsg(!splayer1.starting_ || !splayer2.starting_,
                 "--starting may only be specified for a single player");
    const size_t starting_player_idx = (splayer2.starting_) ? 1 : 0;

    // If training, then 'train_dir' must exist. With training
    // --player1 must be specified as alphazero.
    CheckTrueMsg(!train || std::filesystem::is_directory(train_dir),
                 "--train-dir must be an existing directory");
    CheckTrueMsg(!train || (player1_specified &&
                            (splayer1.kind_ == PlayerSettings::Kind::ALPHAZERO)),
                 "--train requires --player1=alphazero");

    // Create shared triton and alphazero object(s) if needed. If
    // training and only player1 was specified then , then share a
    // single alphazero object between player1 and player2 to get
    // self-play.
    std::shared_ptr<cifre::Triton> triton;
    std::unique_ptr<cifre::AlphaZeroAggregator> alphazero_aggregator;
    std::shared_ptr<cifre::AlphaZero> alphazero1, alphazero2;
    if ((splayer1.kind_ == PlayerSettings::Kind::ALPHAZERO) ||
        (splayer2.kind_ == PlayerSettings::Kind::ALPHAZERO)) {
      triton.reset(new cifre::Triton(logger));
      CheckTrueMsg(triton->IsReady(), "triton is not ready");
      // FIXME make sure no models are already loaded into Triton. Due
      // to an earlier failure, if there is an old model still loaded
      // it could cause hard to debug failures.

      alphazero_aggregator =
        std::make_unique<cifre::AlphaZeroAggregator>(
          logger, cifre::AlphaZeroAggregator::AggregateKind::AVERAGE,
          cifre::AlphaZeroAggregator::ValueKind::AVERAGE); // FIXME provide options

      if (splayer1.kind_ == PlayerSettings::Kind::ALPHAZERO) {
        CheckTrueMsg(!splayer1.model_.empty(), "--player1=alphazero requires --model");
        alphazero1 = std::make_shared<cifre::AlphaZero>(
          logger, triton, splayer1.alphazero_settings_, splayer1.model_);
      }

      // If training and --player2 is not specified, then self-play
      // with the same alphazero object as player1.
      if (train && !player2_specified) {
        splayer2 = splayer1;
        splayer2.id_ = 2;
        splayer1.name_ = "self1";
        splayer2.name_ = "self2";
        alphazero2 = alphazero1;
      }
      // ... otherwise, player2 is initialized normally.
      else if (splayer2.kind_ == PlayerSettings::Kind::ALPHAZERO) {
        CheckTrueMsg(!splayer2.model_.empty(), "--player2=alphazero requires --model");
        alphazero2 = std::make_shared<cifre::AlphaZero>(
          logger, triton, splayer2.alphazero_settings_, splayer2.model_);
      }
    }

    // Create players. The players are collected in a vector and each
    // player's position in the vector is the players ID.
    std::unique_ptr<cifre::tictactoe::Player> player1(CreatePlayer(logger, splayer1, alphazero1));
    std::unique_ptr<cifre::tictactoe::Player> player2(CreatePlayer(logger, splayer2, alphazero2));
    std::vector<cifre::tictactoe::Player*> players { player1.get(), player2.get() };

    // Play game(s) starting at the initial board each game.
    size_t games_completed = 0;
    size_t draw_count = 0;
    size_t winner1_count = 0;
    size_t winner2_count = 0;
    for (size_t gidx = 0; gidx < game_count; gidx++) {
      cifre::tictactoe::Board board(logger, board_size, players, starting_player_idx);

      if (game_count > 1) {
        logger->LogMed("====");
        logger->LogMed("==== Game " + std::to_string(gidx + 1) + " of " + std::to_string(game_count) + " ====");
        logger->LogMed("====");
      }

      cifre::tictactoe::PlayResult presult = Play(logger, players, &board);
      switch (presult) {
      case cifre::tictactoe::PlayResult::QUIT:
        break;
      case cifre::tictactoe::PlayResult::DRAW:
        games_completed++;
        draw_count++;
        break;
      case cifre::tictactoe::PlayResult::WINNER1:
        games_completed++;
        winner1_count++;
        break;
      case cifre::tictactoe::PlayResult::WINNER2:
        games_completed++;
        winner2_count++;
        break;
      }

      // If training, aggregate the training samples and
      // statistics. The aggregation takes ownership of some AlphaZero
      // state, rendering the node tree invalid, thus the AlphaZero
      // object is Reset() within Aggregate().
      if (train) {
        if (alphazero1 != nullptr) {
          alphazero_aggregator->Aggregate(alphazero1.get());
        }
        if ((alphazero2 != nullptr) && (alphazero2 != alphazero1)) {
          alphazero_aggregator->Aggregate(alphazero2.get());
        }
      }
    }

    // End of all games output...
    logger->LogLow("==== " + std::to_string(games_completed) + " game" +
                   ((games_completed == 1) ? " " : "s ") + "completed");
    if (games_completed > 0) {
      logger->LogLow(player1->Name() + ": " + std::to_string(winner1_count) +
                     " (" + FloatToString(100.0 * winner1_count / games_completed, 1 /* precision */) + "%)",
                     4 /* indent */);
      logger->LogLow(player2->Name() + ": " + std::to_string(winner2_count) +
                     " (" + FloatToString(100.0 * winner2_count / games_completed, 1 /* precision */) + "%)",
                     4 /* indent */);
      logger->LogLow("draw: " + std::to_string(draw_count) +
                     " (" + FloatToString(100.0 * draw_count / games_completed, 1 /* precision */) + "%)",
                     4 /* indent */);
    }

    // Aggregated alphazero statistics, if any...
    if (alphazero_aggregator != nullptr) {
      logger->LogLow("AlphaZero");
      logger->LogLow(alphazero_aggregator->ToString(), 4 /* indent */);
    }

    // Triton statistics, if any...
    if (triton != nullptr) {
      logger->LogLow("Triton");
      logger->LogLow(triton->ToString(), 4 /* indent */);
    }

    // Write report, if requested.
    if (!report_filename.empty()) {
      std::ofstream fr(report_filename, std::ios::out);
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

    // If training, write alphazero training data.
    if (train && (alphazero_aggregator != nullptr)) {
      alphazero_aggregator->WriteTraining(train_dir);
    }

  } catch (const std::exception& ex) {
    std::cerr << "error: " << ex.what() << std::endl;
    exit(1);
  }

  return 0;
}
