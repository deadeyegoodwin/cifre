// Copyright 2026, David Goodwin. All rights reserved.

#include <cstdint>
#include <filesystem>
#include <getopt.h>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "cifre_game.h"
#include "biplane_board.h"
#include "biplane_player.h"
#include "common_error.h"
#include "common_util.h"

namespace cifre { namespace biplane {

static const size_t DEFAULT_MAX_TURNS_PER_GAME = 64;

//
// Biplane
//
class Biplane : public cifre::Game {
public:
  // Construct an biplane game.
  Biplane();

  // See cifre::Game
  const std::string& Name() const override {
    static std::string n("Biplane"); return n; }

  // See cifre::Game
  const std::string& ExecutableName() const override {
    static std::string n("biplane"); return n; }

  // See cifre::Game
  std::vector<cifre::GameOption> CustomGameOptions() const override;

  // See cifre::Game
  bool ParseCustomOption(const GameOption& opt, const std::string& arg_str) override;

  // See cifre::Game
  cifre::AlphaZeroSettings DefaultAlphaZeroSettings(
    bool training, bool az_vs_az) const override;

  // See cifre::Game
  Player* CreatePlayer(
    const Player::Kind kind, const size_t id, const std::string& name,
    const std::shared_ptr<cifre::Algo>& algo) const override;

  // See cifre::Game
  StateUniqueConstPtr CreateInitialState(
    const std::vector<Player*>& players, size_t starting_player_idx) const override;

private:
  GameOption::OptionId OPTION_GAME_SHAPE;
  GameOption::OptionId OPTION_MAX_TURNS_PER_GAME;

  cifre::MatrixSize game_shape_;
  size_t game_max_distance_;
};

Biplane::Biplane()
  : cifre::Game(), game_max_distance_(DEFAULT_MAX_DISTANCE)
{
  cifre::GameOption::OptionId custom_id = cifre::Game::FirstCustomOptionId;
  OPTION_GAME_SHAPE = custom_id++;
  OPTION_MAX_TURNS_PER_GAME = custom_id++;

  // Set game-dependent options that are declared in cifre::Game
  max_turns_per_game_ = DEFAULT_MAX_TURNS_PER_GAME;
}

std::vector<cifre::GameOption>
Biplane::CustomGameOptions() const
{
  std::vector<cifre::GameOption> custom;
  custom = {
    {OPTION_GAME_SHAPE, "game-shape", GameOption::ArgInt,
     "Maximum distance that planes can be separated and remain on the " + Name() + " board. "
     "Must be an odd number."},
    {OPTION_MAX_TURNS_PER_GAME, "max-turns-per-game", GameOption::ArgInt,
     "Maximum number of turns allowed per game. One turn consists of each "
     "player having a chance to move."}};

  return custom;
}

bool
Biplane::ParseCustomOption(const GameOption& opt, const std::string& arg_str)
{
  if (opt.Id() == OPTION_GAME_SHAPE) {
    // game_shape must parse as a single value (which becomes a square
    // MatrixSize).
    game_shape_ = cifre::MatrixSize(arg_str);
    GameCheckTrueMsg(game_shape_.RowCount() == game_shape_.ColumnCount(),
                     Name() + " board max-distance must be specified as --game-shape=<dist>");
    GameCheckTrueMsg((game_shape_.RowCount() > 0) && ((game_shape_.RowCount() % 2) == 1),
                     Name() + " board max-distance specified with --game-shape must be positive odd integer");
    GameCheckTrueMsg(game_shape_.RowCount() <= MAX_DISTANCE,
                     Name() + " board max-distance specified with --game-shape must be <= " +
                     std::to_string(MAX_DISTANCE));
    game_max_distance_ = game_shape_.RowCount();
  }
  else if (opt.Id() == OPTION_MAX_TURNS_PER_GAME) {
    max_turns_per_game_ = cifre::GameOption::ParseUnsigned(opt, arg_str, 1 /* min */);
  }
  else {
    return false;
  }

  return true;
}

cifre::AlphaZeroSettings
Biplane::DefaultAlphaZeroSettings(bool training, bool az_vs_az) const
{
  cifre::AlphaZeroSettings settings;
  settings.training_ = training;

  settings.search_count_ = 500;
  settings.max_search_depth_ = 64;
  settings.cpuct_ = 10.0;

  // If 'training' then using MCTS to self-play and so want to use
  // both dirichlet and weighted-random action. If two alphazero
  // players are playing each other ('az_vs_az') then no dirichlet but
  // still use weighted-random action or else every game will be
  // identical. Otherwise no dirichlet and choose the best move.
  if (training) {
    settings.next_action_kind_ =
      cifre::AlphaZeroSettings::NextActionKind::RANDOM_VISIT_WEIGHTED;
    settings.dirichlet_alpha_ = 10.0 / 64.0;
    settings.dirichlet_sample_count_ = 2117;
    settings.dirichlet_ratio_ = 0.25;
  }
  else if (az_vs_az) {
    settings.next_action_kind_ =
      cifre::AlphaZeroSettings::NextActionKind::RANDOM_VISIT_WEIGHTED_90;
    settings.dirichlet_alpha_ = 0.0; // disabled
  }
  else { // AZ vs non-AZ opponent
    settings.next_action_kind_ =
      cifre::AlphaZeroSettings::NextActionKind::MAX_VISITED;
    settings.dirichlet_alpha_ = 0.0; // disabled
  }

  return settings;
}

cifre::Player*
Biplane::CreatePlayer(
  const Player::Kind kind, const size_t id, const std::string& name,
  const std::shared_ptr<cifre::Algo>& algo) const
{
  switch (kind) {
  case Player::Kind::UNKNOWN:
    break;
  case Player::Kind::HUMAN:
    return new HumanPlayer(logger_, id, name, algo);
  case Player::Kind::ALGO:
    assert(algo != nullptr);
    return new cifre::AlgoPlayer(logger_, id, name, algo);
  }

  return nullptr;
}

StateUniqueConstPtr
Biplane::CreateInitialState(
  const std::vector<Player*>& players, size_t starting_player_idx) const
{
  return StateUniquePtr(
    new Board(
      game_max_distance_, players, starting_player_idx, std::pmr::new_delete_resource()),
    StateDeleterNewDelete<Board>());
}

}} // namespace cifre::biplane

int
main(int argc, char **argv)
{
  cifre::biplane::Biplane game;
  game.Initialize(argc, argv);
  game.Play();

  return 0;
}
