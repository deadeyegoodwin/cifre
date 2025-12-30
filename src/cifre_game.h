// Copyright 2025, David Goodwin. All rights reserved.
#pragma once

#include <cstddef>
#include <cstdint>
#include <getopt.h>
#include <memory>
#include <string>

#include "cifre_action.h"
#include "cifre_algo_alphazero.h"
#include "cifre_logger.h"
#include "cifre_player.h"
#include "cifre_state.h"

namespace cifre {

//
// GameOption
//
// Class for handling command-line game options.
//
class GameOption {
public:
  // Type for the ID of a BuiltIn or custom option.
  using OptionId = int;

  // Option argument types.
  static constexpr const char* ArgNone = "";
  static constexpr const char* ArgBool = "boolean";
  static constexpr const char* ArgFloat = "float";
  static constexpr const char* ArgInt = "integer";
  static constexpr const char* ArgStr = "string";

  // Create a command-line option.
  GameOption(OptionId id, const std::string& flag,
             const std::string& arg_desc, const std::string& desc)
    : id_(id), flag_(flag), arg_desc_(arg_desc), desc_(desc) { }

  // Return the OptionId of this option.
  OptionId Id() const { return id_; }

  // Return the flag for this option, with or without dashes.
  const std::string& Flag() const { return flag_; }
  const std::string FlagWithDash() const { return "--" + flag_; }

  // Return the argument type description and option description, both
  // are used for usage message.
  const std::string& ArgumentTypeDescription() const { return arg_desc_; }
  const std::string& Description() const { return desc_; }

  // Return the getopt() option structure for 'this' game option.
  struct option LongOption() const
  {
    struct option lo {
      flag_.c_str(), (!arg_desc_.empty()) ? required_argument : no_argument,
          nullptr, id_
    };
    return lo;
  }

  // Parse ArgInt option as an unsigned integer. Raise error if
  // 'arg_str' is not a valid unsigned ArgInt or is not between
  // 'min_value' and 'max_value', inclusive.
  static uint64_t ParseUnsigned(
    const GameOption& opt, const std::string& arg_str, uint64_t min_value = 0,
    uint64_t max_value = std::numeric_limits<uint64_t>::max());

  // Parse ArgInt option as a signed integer. Raise error if 'arg_str'
  // is not a valid signed ArgInt or is not between 'min_value' and
  // 'max_value', inclusive.
  static int64_t ParseSigned(
    const GameOption& opt, const std::string& arg_str,
    int64_t min_value = std::numeric_limits<int64_t>::min(),
    int64_t max_value = std::numeric_limits<int64_t>::max());

  // Parse ArgFloat option as a float. Raise error if 'arg_str' is not
  // a valid float or is not between 'min_value' and 'max_value',
  // inclusive.
  static float ParseFloat(
    const GameOption& opt, const std::string& arg_str,
    float min_value = -std::numeric_limits<float>::max(),
    float max_value = std::numeric_limits<float>::max());

private:
  OptionId id_;
  std::string flag_;
  std::string arg_desc_;
  std::string desc_;
};

//
// PlayerSettings
//
// Class for collecting command-line player options and settings.
//
struct PlayerSettings {
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
    return "<none>"; // unreachable
  }

  static Player::Kind KindToPlayerKind(const Kind kind) {
    switch (kind) {
    case Kind::UNKNOWN: return Player::Kind::UNKNOWN;
    case Kind::HUMAN: return Player::Kind::HUMAN;
    case Kind::RANDOM:
    case Kind::ALPHAZERO:
      return Player::Kind::ALGO;
    }
    return Player::Kind::UNKNOWN; // unreachable
  }

  std::string DefaultName() const {
    return PlayerSettings::KindToString(kind_) + std::to_string(id_ + 1);
  }

  Kind kind_ {Kind::UNKNOWN};
  bool cmdline_specified_ {false};
  size_t id_;
  std::string name_;
  bool starting_ {false};
  AlphaZeroSettings alphazero_settings_;
};

//
// Game
//
// Base class on which cifre-based games can be derived.
//
class Game {
public:
  Game();
  virtual ~Game() = default;

  // Initialize the game options and other state. Must be called
  // before game(s) can be played. Can be called only once.
  void Initialize(int argc, char **argv);

  // Play number of games specified by --game-count.
  void Play();

  // Return the name of the game.
  virtual const std::string& Name() const = 0;

  // Return the name of the game's executable.
  virtual const std::string& ExecutableName() const = 0;

  // Return the start and/or end banner for the game.
  virtual const std::string StartBanner() const { return std::string(); }
  virtual const std::string EndBanner() const { return std::string(); }

  // Set the default AlphaZero settings for the game.  If 'training'
  // then using MCTS to self-play. If 'az_vs_az' then two alphazero
  // players are playing each other outside of MCTS. If both are false
  // then an AlphaZero player is playing a non-AlphaZero player.
  virtual cifre::AlphaZeroSettings DefaultAlphaZeroSettings(
    bool training, bool az_vs_az) const = 0;

  // Return the cifre::State representing the initial game state. This
  // may be called multiple times if it is necessary to create
  // multiple copies of the initial state.
  virtual StateUniqueConstPtr CreateInitialState(
    const std::vector<Player*>& players, size_t starting_player_idx) const = 0;

  // Create Player for the game. 'algo' must be nullptr for
  // non-algorithmic players. Returns nullptr if the game does not
  // support players of 'kind'.
  virtual Player* CreatePlayer(
    const Player::Kind kind, const size_t id, const std::string& name,
    const std::shared_ptr<Algo>& algo) const = 0;

  // Built-in command-line options. Specific games should start their
  // options with Game::FirstCustomOptionId.
  enum BuiltInOptionId {
    OPTION_HELP = GameOption::OptionId(0),
    OPTION_QUIET,
    OPTION_VERBOSE,
    OPTION_LOG_FILE,
    OPTION_LOG_INVOKE,
    OPTION_LOG_PROGRESS,
    OPTION_LOG_SUMMARY,
    OPTION_LOG_GAME,
    OPTION_LOG_ALGO,
    OPTION_LOG_TRITON,
    OPTION_LOG_TRITON_CLIENT,
    OPTION_REPORT_FILE,

    OPTION_TRAIN,
    OPTION_TRAIN_DIR,

    OPTION_PARALLEL,
    OPTION_GAME_COUNT,

    OPTION_PLAYER1,
    OPTION_PLAYER2,
    OPTION_PLAYER_NAME,
    OPTION_PLAYER_STARTING,
    OPTION_PLAYER_ALPHAZERO_MODEL,
    OPTION_PLAYER_ALPHAZERO_SEARCH_COUNT,
    OPTION_PLAYER_ALPHAZERO_SEARCH_DEPTH,
    OPTION_PLAYER_ALPHAZERO_CPUCT,

    OPTION_FIRST_CUSTOM_ID
  };

  // The lowest value OptionId that should be used for custom options.
  static constexpr const GameOption::OptionId FirstCustomOptionId = OPTION_FIRST_CUSTOM_ID;

  // Return the GameOption for a given OptionId. Error if OptionId is
  // not a known option.
  const GameOption& OptionIdToOption(GameOption::OptionId id) const;

  // Return the custom GameOptions for the game. At the least, a game
  // must provide a --game-shape custom option that accepts the game
  // shape as a comma separated tuple (for example, --game-shape=4,5),
  // and must provide --max-turns-per-game.
  virtual std::vector<GameOption> CustomGameOptions() const = 0;

  // Parse a custom 'opt'ion. Return false if unable to parse the
  // option.
  virtual bool ParseCustomOption(const GameOption& opt, const std::string& arg_str) = 0;

  // Return usage message for the game's options.
  std::string Usage() const;

protected:
  // Logger for the game.
  std::shared_ptr<Logger> logger_;

  // Game-dependent options. These must be set by the derived game
  // class to values appropriate for the game.
  size_t max_turns_per_game_;

private:
  // Parse command-line arguments.
  void Parse(int argc, char **argv);

  // Return player created from settings.
  Player* CreatePlayerInternal(
    const PlayerSettings& player_settings, const std::shared_ptr<Algo>& algo) const;

  // Has this game be initialized by a call to Initialize()?
  bool initialized_;

  // The command-line options for the game. These include the BuiltIn
  // options plus any custom options added by the deriving game.
  std::vector<GameOption> game_options_;

  // Game independent settings
  size_t parallel_;
  size_t game_count_;
  size_t starting_player_idx_;
  std::string report_filename_;
  std::string train_dir_;

  // Ordered player settings so that player_settings_[0] is the
  // settings for "player 1" which is player ID 0, etc. These settings
  // are assigned during Initialize() and are constant as they cannot
  // be changed after that point.
  std::unique_ptr<const std::vector<PlayerSettings>> player_settings_;

  // Shared triton and alpha-zero training samples objects. These
  // objects are shared across all games.
  std::shared_ptr<TritonServer> triton_server_;
  std::unique_ptr<AlphaZeroTrainingSamples> alphazero_train_samples_;
};

// Macros for error checking and error reporting in game
// executables. These macros do not use a cifre::Logger and so can be
// used before a logger is created.
#define _stringify(A) #A
#define GameCheckTrue(P)                                                \
  do {                                                                  \
    const bool p__ = (P);                                               \
    if (!p__) {                                                         \
      std::cerr << std::string("error: expected 'true' for: " _stringify(P)) << std::endl; \
      exit(1);                                                          \
    }                                                                   \
  } while (false)

#define GameCheckTrueMsg(P, MSG)                                        \
  do {                                                                  \
    const bool p__ = (P);                                               \
    if (!p__) {                                                         \
      std::cerr << std::string("error: ") << (MSG) << std::endl;        \
      exit(1);                                                          \
    }                                                                   \
  } while (false)

#define GameFail(MSG)                                                 \
  do {                                                                \
    std::cerr << "error: " << (MSG) << std::endl;                     \
    exit(1);                                                          \
  } while (false)

} // namespace cifre
