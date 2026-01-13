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
#include "connect4_board.h"
#include "connect4_player.h"
#include "common_error.h"
#include "common_util.h"

namespace cifre { namespace connect4 {

static const cifre::MatrixSize DEFAULT_GAME_SHAPE {6, 7};
static const size_t DEFAULT_MAX_TURNS_PER_GAME = 21;

//
// Connect4
//
class Connect4 : public cifre::Game {
public:
  // Construct a connect4 game.
  Connect4();

  // See cifre::Game
  const std::string& Name() const override {
    static std::string n("Connect4"); return n; }

  // See cifre::Game
  const std::string& ExecutableName() const override {
    static std::string n("connect4"); return n; }

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
};

Connect4::Connect4()
  : cifre::Game(), game_shape_(DEFAULT_GAME_SHAPE)
{
  cifre::GameOption::OptionId custom_id = cifre::Game::FirstCustomOptionId;
  OPTION_GAME_SHAPE = custom_id++;
  OPTION_MAX_TURNS_PER_GAME = custom_id++;

  // Set game-dependent options that are declared in cifre::Game
  max_turns_per_game_ = DEFAULT_MAX_TURNS_PER_GAME;
}

std::vector<cifre::GameOption>
Connect4::CustomGameOptions() const
{
  std::vector<cifre::GameOption> custom;
  custom = {
    {OPTION_GAME_SHAPE, "game-shape", GameOption::ArgInt,
     "Size of the " + Name() + " board. A board with N rows and M columns is specified "
     "as '--game-shape=N,M'. "
     "Default is '--game-shape=" + DEFAULT_GAME_SHAPE.ToString() + "'."},
    {OPTION_MAX_TURNS_PER_GAME, "max-turns-per-game", GameOption::ArgInt,
     "Maximum number of turns allowed per game. One turn consists of each "
     "player having a chance to occupy a space on the " + Name() + " board."}};

  return custom;
}

bool
Connect4::ParseCustomOption(const GameOption& opt, const std::string& arg_str)
{
  if (opt.Id() == OPTION_GAME_SHAPE) {
    game_shape_ = cifre::MatrixSize(arg_str);
    GameCheckTrueMsg((game_shape_.RowCount() >= 4) && (game_shape_.ColumnCount() >= 4),
                     Name() + " board must be " + cifre::MatrixSize(4, 4).ToString() +
                     " or larger");
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
Connect4::DefaultAlphaZeroSettings(bool training, bool az_vs_az) const
{
  cifre::AlphaZeroSettings settings;
  settings.training_ = training;

  settings.search_count_ = 600;
  settings.max_search_depth_ = 42;
  settings.cpuct_ = 10.0;

  // If 'training' then using MCTS to self-play and so want to use
  // both dirichlet and weighted-random action. If two alphazero
  // players are playing each other ('az_vs_az') then no dirichlet but
  // still use weighted-random action or else every game will be
  // identical. Otherwise no dirichlet and choose the best move.
  if (training) {
    settings.next_action_kind_ =
      cifre::AlphaZeroSettings::NextActionKind::RANDOM_VISIT_WEIGHTED;
    settings.dirichlet_alpha_ = 10.0 / float(game_shape_.ColumnCount());
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
Connect4::CreatePlayer(
  const Player::Kind kind, const size_t id, const std::string& name,
  const std::shared_ptr<cifre::Algo>& algo) const
{
  switch (kind) {
  case Player::Kind::UNKNOWN:
    break;
  case Player::Kind::HUMAN:
    assert(algo == nullptr);
    return new HumanPlayer(logger_, id, name);
  case Player::Kind::ALGO:
    assert(algo != nullptr);
    return new cifre::AlgoPlayer(logger_, id, name, algo);
  }

  return nullptr;
}

StateUniqueConstPtr
Connect4::CreateInitialState(
  const std::vector<Player*>& players, size_t starting_player_idx) const
{
  return StateUniquePtr(
    new Board(
      game_shape_, players, starting_player_idx, std::pmr::new_delete_resource()),
    StateDeleterNewDelete<Board>());
}

}} // namespace cifre::connect4

int
main(int argc, char **argv)
{
  cifre::connect4::Connect4 game;
  game.Initialize(argc, argv);
  game.Play();

  return 0;
}
