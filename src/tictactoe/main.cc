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
    OPTION_TRAIN_DIR,
    OPTION_BOARD_SIZE,
    OPTION_GAME_COUNT,

    OPTION_PLAYER1,
    OPTION_PLAYER2,
    OPTION_PLAYER_NAME,
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
   "File for output. 'stdout' and 'stderr' are allowed."},

  {Option::OPTION_TRAIN_DIR, "train-dir", Option::ArgStr,
   "Training data directory. If specified then the directory must exist "
   "and training data will be output to this directory."},

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

  PlayerSettings() : kind_(Kind::UNKNOWN) { }
  PlayerSettings(const std::string& kind_str, size_t id) :
    kind_(StringToKind(kind_str)), id_(id),
    name_(kind_str + std::to_string(id)) { }

  Kind StringToKind(const std::string& str) const {
    if (str == "human") { return Kind::HUMAN; }
    if (str == "random") { return Kind::RANDOM; }
    if (str == "alphazero") { return Kind::ALPHAZERO; }
    Fail("unknown player kind '" + str + "'");
    return Kind::UNKNOWN;
  }

  Kind kind_;
  size_t id_;
  std::string name_;
  std::string model_;
  cifre::AlphaZero::NextActionSettings alphazero_settings_;
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

  // Let all players know game is completed.
  for (Player* p : players) {
    p->GameEnd(board, terminal_status);
  }

  // Game was quit.
  if (game_quit) {
    LOGGER_LOGMED(*logger, "game quit: no winner");
    LOGGER_LOGMED(*logger, board->ToString());
    return PlayResult::QUIT;
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
    std::string train_dir;
    cifre::BoardSize2D board_size = DEFAULT_BOARD_SIZE;
    size_t game_count = 1;

    PlayerSettings splayer1("human", 1);
    PlayerSettings splayer2("human", 2);
    PlayerSettings* active_splayer = nullptr;

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
        splayer1 = PlayerSettings(optarg, 1);
        active_splayer = &splayer1;
        break;
      case Option::OPTION_PLAYER2:
        splayer2 = PlayerSettings(optarg, 2);
        active_splayer = &splayer2;
        break;
      case Option::OPTION_PLAYER_NAME:
        CheckTrueMsg(active_splayer != nullptr,
                     "--player1 or --player2 must be specified before --name");
        active_splayer->name_ = optarg;
        break;
      case Option::OPTION_PLAYER_MODEL:
        CheckTrueMsg(active_splayer != nullptr,
                     "--player1 or --player2 must be specified before --model");
        active_splayer->model_ = optarg;
        break;
      case Option::OPTION_PLAYER_ALPHAZERO_UPDATE_COUNT:
        CheckTrueMsg(active_splayer != nullptr,
                     "--player1 or --player2 must be specified before --alphazero-update-count");
        active_splayer->alphazero_settings_.alphazero_update_count_ = ParseUnsigned(optarg, 1);
        break;
      case Option::OPTION_PLAYER_ALPHAZERO_CPUCT:
        CheckTrueMsg(active_splayer != nullptr,
                     "--player1 or --player2 must be specified before --alphazero-cpuct");
        active_splayer->alphazero_settings_.alphazero_cpuct_ = ParseFloat(optarg, 0.0);
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

    if ((splayer1.kind_ == PlayerSettings::Kind::HUMAN) ||
        (splayer2.kind_ == PlayerSettings::Kind::HUMAN)) {
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

    CheckTrueMsg(splayer1.kind_ != PlayerSettings::Kind::UNKNOWN,
                 "--player1 must be specified");
    CheckTrueMsg(splayer2.kind_ != PlayerSettings::Kind::UNKNOWN,
                 "--player2 must be specified");

    CheckTrueMsg(game_count >= 1, "--game-count must be >= 1");

    // If training, then 'train_dir' must exist.
    const bool train = !train_dir.empty();
    CheckTrueMsg(!train || std::filesystem::is_directory(train_dir),
                 "--train-dir must be an existing directory");

    // Create shared triton and alphazero object(s) if needed.
    std::shared_ptr<cifre::Triton> triton;
    std::shared_ptr<cifre::AlphaZero> alphazero1, alphazero2;
    if ((splayer1.kind_ == PlayerSettings::Kind::ALPHAZERO) ||
        (splayer2.kind_ == PlayerSettings::Kind::ALPHAZERO)) {
      triton.reset(new cifre::Triton(logger));
      CheckTrueMsg(triton->IsReady(), "triton is not ready");

      if (splayer1.kind_ == PlayerSettings::Kind::ALPHAZERO) {
        CheckTrueMsg(!splayer1.model_.empty(), "--player1=alphazero requires --model");
        splayer1.alphazero_settings_.training_ = train;
        splayer1.alphazero_settings_.kind_ = (train) ?
          cifre::AlphaZero::NextActionSettings::NextActionKind::RANDOM_VISIT_WEIGHTED :
          cifre::AlphaZero::NextActionSettings::NextActionKind::MAX_VISITED;
        splayer1.alphazero_settings_.alphazero_max_depth_ = 20; // FIXME, should be adjusted after each turn
        alphazero1 = std::make_shared<cifre::AlphaZero>(
          logger, triton, splayer1.model_, splayer1.alphazero_settings_);
      }
      if (splayer2.kind_ == PlayerSettings::Kind::ALPHAZERO) {
        CheckTrueMsg(!splayer2.model_.empty(), "--player2=alphazero requires --model");
        splayer2.alphazero_settings_.training_ = train;
        splayer2.alphazero_settings_.kind_ = (train) ?
          cifre::AlphaZero::NextActionSettings::NextActionKind::RANDOM_VISIT_WEIGHTED :
          cifre::AlphaZero::NextActionSettings::NextActionKind::MAX_VISITED;
        splayer2.alphazero_settings_.alphazero_max_depth_ = 20; // FIXME, should be adjusted after each turn
        alphazero2 = std::make_shared<cifre::AlphaZero>(
          logger, triton, splayer2.model_, splayer2.alphazero_settings_);
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
      cifre::tictactoe::Board board(logger, board_size, players);

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

    // Individual player statistics, if any...
    for (const cifre::tictactoe::Player* p : players) {
      std::string str = p->ToStatisticsString();
      if (!str.empty()) {
        logger->LogLow(p->Name());
        logger->LogLow(str, 4 /* indent */);
      }
    }

    // If training, write alphazero training data.
    if (train) {
      if ((alphazero1 != nullptr) && (alphazero2 != nullptr)) {
        alphazero1->MergeTraining(alphazero2.get());
      }
      if (alphazero1 != nullptr) {
        alphazero1->WriteTraining(train_dir);
      }
      else if (alphazero2 != nullptr) {
        alphazero2->WriteTraining(train_dir);
      }
    }

  } catch (const std::exception& ex) {
    std::cerr << "error: " << ex.what() << std::endl;
    exit(1);
  }

  return 0;
}
