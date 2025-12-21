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
#include "tictactoe_board.h"
#include "tictactoe_player.h"
#include "common_error.h"
#include "common_util.h"

namespace cifre { namespace tictactoe {

static const cifre::Size2D DEFAULT_GAME_SHAPE {3, 3};
static const size_t DEFAULT_MAX_TURNS_PER_GAME = 5;

//
// TicTacToe
//
class TicTacToe : public cifre::Game {
public:
  // Construct a tie-tac-toe game.
  TicTacToe();

  // See cifre::Game
  const std::string& Name() const override {
    static std::string n("tic-tac-toe"); return n; }

  // See cifre::Game
  const std::string& ExecutableName() const override {
    static std::string n("tictactoe"); return n; }

  // See cifre::Game
  std::vector<cifre::GameOption> CustomGameOptions() const override;

  // See cifre::Game
  bool ParseCustomOption(const GameOption& opt, const std::string& arg_str) override;

  // See cifre::Game
  cifre::AlphaZeroSettings DefaultAlphaZeroSettings(const bool train) const override;

  // See cifre::Game
  Player* CreatePlayer(
    const Player::Kind kind, const size_t id, const std::string& name,
    const std::shared_ptr<cifre::Algo>& algo) const override;

  // See cifre::Game
  State* CreateInitialState(
    const std::vector<Player*>& players, size_t starting_player_idx) override;

private:
  GameOption::OptionId OPTION_GAME_SHAPE;
  GameOption::OptionId OPTION_MAX_TURNS_PER_GAME;

  cifre::Size2D game_shape_;
};

TicTacToe::TicTacToe()
  : cifre::Game(), game_shape_(DEFAULT_GAME_SHAPE)
{
  cifre::GameOption::OptionId custom_id = cifre::Game::FirstCustomOptionId;
  OPTION_GAME_SHAPE = custom_id++;
  OPTION_MAX_TURNS_PER_GAME = custom_id++;

  // Set game-dependent options that are declared in cifre::Game
  max_turns_per_game_ = DEFAULT_MAX_TURNS_PER_GAME;
}

std::vector<cifre::GameOption>
TicTacToe::CustomGameOptions() const
{
  std::vector<cifre::GameOption> custom;
  custom = {
    {OPTION_GAME_SHAPE, "game-shape", GameOption::ArgInt,
     "Size of the " + Name() + " board. A board with N rows and N columns is specified "
     "as '--game-shape=N,N'. The board shape must be square. "
     "Default is '--game-shape=" + DEFAULT_GAME_SHAPE.ToString() + "'."},
    {OPTION_MAX_TURNS_PER_GAME, "max-turns-per-game", GameOption::ArgInt,
     "Maximum number of turns allowed per game. One turn consists of each "
     "player occupying a space on the " + Name() + " board."}};

  return custom;
}

bool
TicTacToe::ParseCustomOption(const GameOption& opt, const std::string& arg_str)
{
  if (opt.Id() == OPTION_GAME_SHAPE) {
    game_shape_ = cifre::Size2D(arg_str);
    GameCheckTrueMsg(game_shape_.RowCount() >= 2,
                     Name() + " board must be " + cifre::Size2D(2, 2).ToString() +
                     " or larger");
    GameCheckTrueMsg(game_shape_.RowCount() == game_shape_.ColumnCount(),
                     Name() + " board must be square");
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
TicTacToe::DefaultAlphaZeroSettings(const bool train) const
{
  cifre::AlphaZeroSettings settings;
  settings.training_ = train;

  if (train) { // train = true
    settings.per_game_node_tree_ = false;
    settings.next_action_kind_ =
      cifre::AlphaZeroSettings::NextActionKind::RANDOM_VISIT_WEIGHTED;
    settings.dirichlet_alpha_ = 1.0;
    settings.dirichlet_sample_count_ = 1111;
    settings.dirichlet_ratio_ = 0.25;
    settings.search_count_ = 256;
    settings.max_search_depth_ = 12;
    settings.cpuct_ = 10.0;
  }
  else { // train = false
    settings.per_game_node_tree_ = false;
    settings.next_action_kind_ =
      cifre::AlphaZeroSettings::NextActionKind::MAX_VISITED;
    settings.dirichlet_alpha_ = 0.0; // disable dirichlet
    settings.search_count_ = 256;
    settings.max_search_depth_ = 12;
    settings.cpuct_ = 1.0;
  }

  return settings;
}

cifre::Player*
TicTacToe::CreatePlayer(
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

cifre::State*
TicTacToe::CreateInitialState(
  const std::vector<Player*>& players, size_t starting_player_idx)
{
  return new Board(game_shape_, players, starting_player_idx);
}

}} // namespace cifre::tictactoe

int
main(int argc, char **argv)
{
  cifre::tictactoe::TicTacToe game;
  game.Initialize(argc, argv);
  game.Play();

  return 0;
}
