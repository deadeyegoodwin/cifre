// Copyright 2025, David Goodwin. All rights reserved.

#include <cstdint>
#include <filesystem>
#include <getopt.h>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "cifre_game.h"
#include "othello_board.h"
#include "othello_player.h"
#include "common_error.h"
#include "common_util.h"

namespace cifre { namespace othello {

static const cifre::Size2D DEFAULT_GAME_SHAPE {8, 8};
static const size_t DEFAULT_MAX_TURNS_PER_GAME = 64;

//
// Othello
//
class Othello : public cifre::Game {
public:
  // Construct an othello game.
  Othello();

  // See cifre::Game
  const std::string& Name() const override {
    static std::string n("Othello"); return n; }

  // See cifre::Game
  const std::string& ExecutableName() const override {
    static std::string n("othello"); return n; }

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

  cifre::Size2D game_shape_;
};

Othello::Othello()
  : cifre::Game(), game_shape_(DEFAULT_GAME_SHAPE)
{
  cifre::GameOption::OptionId custom_id = cifre::Game::FirstCustomOptionId;
  OPTION_GAME_SHAPE = custom_id++;
  OPTION_MAX_TURNS_PER_GAME = custom_id++;

  // Set game-dependent options that are declared in cifre::Game
  max_turns_per_game_ = DEFAULT_MAX_TURNS_PER_GAME;
}

std::vector<cifre::GameOption>
Othello::CustomGameOptions() const
{
  std::vector<cifre::GameOption> custom;
  custom = {
    {OPTION_GAME_SHAPE, "game-shape", GameOption::ArgInt,
     "Size of the " + Name() + " board. A board with N rows and N columns is specified "
     "as '--game-shape=N,N'. The board shape must be square. "
     "Default is '--game-shape=" + DEFAULT_GAME_SHAPE.ToString() + "'."},
    {OPTION_MAX_TURNS_PER_GAME, "max-turns-per-game", GameOption::ArgInt,
     "Maximum number of turns allowed per game. One turn consists of each "
     "player having a chance to occupy a space on the " + Name() + " board."}};

  return custom;
}

bool
Othello::ParseCustomOption(const GameOption& opt, const std::string& arg_str)
{
  if (opt.Id() == OPTION_GAME_SHAPE) {
    game_shape_ = cifre::Size2D(arg_str);
    GameCheckTrueMsg(game_shape_.RowCount() >= 4,
                     Name() + " board must be " + cifre::Size2D(4, 4).ToString() +
                     " or larger");
    GameCheckTrueMsg(game_shape_.RowCount() == game_shape_.ColumnCount(),
                     Name() + " board must be square");
    GameCheckTrueMsg((game_shape_.RowCount() % 2) == 0,
                     Name() + " board must have even number of rows ");
    GameCheckTrueMsg((game_shape_.ColumnCount() % 2) == 0,
                     Name() + " board must have even number of columns");
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
Othello::DefaultAlphaZeroSettings(bool training, bool az_vs_az) const
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
Othello::CreatePlayer(
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
Othello::CreateInitialState(
  const std::vector<Player*>& players, size_t starting_player_idx) const
{
  return StateUniquePtr(
    new Board(
      game_shape_, players, starting_player_idx, std::pmr::new_delete_resource()),
    StateDeleterNewDelete<Board>());
}

}} // namespace cifre::othello

int
main(int argc, char **argv)
{
  cifre::othello::Othello game;
  game.Initialize(argc, argv);
  game.Play();

  return 0;
}
