// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <cstddef>
#include <cstdint>
#include <getopt.h>
#include <memory>
#include <string>

#include "cifre_action.h"
#include "cifre_alphazero.h"
#include "cifre_player.h"
#include "cifre_state.h"
#include "common_logger.h"

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

  // Return the name of the game.
  virtual const std::string& Name() const = 0;

  // Return the name of the game's executable.
  virtual const std::string& ExecutableName() const = 0;

  // Create a human-controlled Player for the game.
  virtual cifre::Player* CreateHumanPlayer(
    const std::shared_ptr<common::Logger>& logger,
    const std::string& name, const char board_char) const = 0;

  // Set the default AlphaZero settings for the game, depending on if
  // 'train'ing is being done or not.
  virtual cifre::AlphaZeroSettings DefaultAlphaZeroSettings(const bool train) const = 0;

  // Play number of games specified by --game-count.
  void Play();

  // Return the cifre::State representing the initial game state.
  virtual State* InitialState(
    const std::vector<Player*>& players, size_t starting_player_idx) const = 0;

  // Built-in command-line options. Specific games should start their
  // options with Game::FirstCustomOptionId.
  enum BuiltInOptionId {
    OPTION_HELP = GameOption::OptionId(0),
    OPTION_QUIET,
    OPTION_VERBOSE,
    OPTION_LOG_FILE,
    OPTION_REPORT_FILE,

    OPTION_TRAIN,
    OPTION_TRAIN_DIR,

    OPTION_MAX_TURNS_PER_GAME,
    OPTION_GAME_COUNT,

    OPTION_PLAYER1,
    OPTION_PLAYER2,
    OPTION_PLAYER_NAME,
    OPTION_PLAYER_STARTING,
    OPTION_PLAYER_MODEL,
    OPTION_PLAYER_ALPHAZERO_SEARCH_COUNT,
    OPTION_PLAYER_ALPHAZERO_CPUCT,

    OPTION_FIRST_CUSTOM_ID
  };

  // The lowest value OptionId that should be used for custom options.
  static constexpr const GameOption::OptionId FirstCustomOptionId = OPTION_FIRST_CUSTOM_ID;

  // Return the GameOption for a given OptionId. Error if OptionId is
  // not a known option.
  const GameOption& OptionIdToOption(GameOption::OptionId id) const;

  // Return the custom GameOptions for the game.
  virtual std::vector<GameOption> CustomGameOptions() const = 0;

  // Parse a custom 'opt'ion. Return false if unable to parse the
  // option.
  virtual bool ParseCustomOption(const GameOption& opt, const std::string& arg_str) = 0;

  // Return usage message for the game's options.
  std::string Usage() const;

private:
  class PlayerSettings;

  // Parse command-line arguments.
  void Parse(int argc, char **argv);

  // Return player created from settings.
  Player* CreatePlayer(const PlayerSettings& player_settings,
                       const std::shared_ptr<AlphaZero>& alphazero) const;

  // Logger for the game.
  std::shared_ptr<common::Logger> logger_;

  // Has this game be initialized by a call to Initialize()?
  bool initialized_;

  // The command-line options for the game. These include the BuiltIn
  // options plus any custom options added by the deriving game.
  std::vector<GameOption> game_options_;

  // Game settings
  size_t game_count_;
  size_t max_turns_per_game_;
  size_t starting_player_idx_;
  std::string report_filename_;
  std::string train_dir_;

  // Player settings
  std::shared_ptr<PlayerSettings> splayer1_;
  std::shared_ptr<PlayerSettings> splayer2_;

  // Shared triton and alpha-zero objects. These objects are shared
  // across all games.
  std::shared_ptr<Triton> triton_;
  std::unique_ptr<AlphaZeroAggregator> alphazero_aggregator_;
};

// Macros for error checking and error reporting in game
// executables. These macros do not use a common::Logger and so can be
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
