// Copyright 2024, David Goodwin. All rights reserved.

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
#include "common_logger.h"
#include "common_util.h"

namespace cifre { namespace tictactoe {

static const cifre::BoardSize2D DEFAULT_GAME_SHAPE {3, 3};

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
  cifre::Player* CreateHumanPlayer(
    const std::shared_ptr<common::Logger>& logger,
    const std::string& name, const char board_char) const override;

  // See cifre::Game
  cifre::AlphaZeroSettings DefaultAlphaZeroSettings(const bool train) const override;

  // See cifre::Game
  State* InitialState(
    const std::vector<Player*>& players, size_t starting_player_idx) const override;

private:
  GameOption::OptionId OPTION_GAME_SHAPE;

  cifre::BoardSize2D game_shape_;
};

TicTacToe::TicTacToe()
  : cifre::Game(), game_shape_(DEFAULT_GAME_SHAPE)
{
  cifre::GameOption::OptionId custom_id = cifre::Game::FirstCustomOptionId;
  OPTION_GAME_SHAPE = custom_id++;
}

std::vector<cifre::GameOption>
TicTacToe::CustomGameOptions() const
{
  std::vector<cifre::GameOption> custom;
  custom = {
    {OPTION_GAME_SHAPE, "game-shape", GameOption::ArgInt,
     "Size of the " + Name() + " board. A board with N rows and N columns is specified "
     "as '--game-shape=N,N'. The board shape must be square. "
     "Default is '--game-shape=" + DEFAULT_GAME_SHAPE.ToString() + "'."}};

  return custom;
}

cifre::Player*
TicTacToe::CreateHumanPlayer(
  const std::shared_ptr<common::Logger>& logger,
  const std::string& name, const char board_char) const
{
  return new HumanPlayer(logger, name, board_char, 1.0);
}

cifre::AlphaZeroSettings
TicTacToe::DefaultAlphaZeroSettings(const bool train) const
{
  cifre::AlphaZeroSettings settings;
  settings.training_ = train;
  settings.enable_cache_ = true;
  settings.search_count_ = 50;

  // FIXME
  if (train) { // train = true
    settings.next_action_kind_ =
      cifre::AlphaZeroSettings::NextActionKind::RANDOM_VISIT_WEIGHTED;
    settings.dirichlet_alpha_ = std::min(1.0, 10.0 / 9.0 /* FIXME need total-action-id-count from game */);
    settings.max_search_depth_ = 20;
    settings.cpuct_ = 4.0; // FIXME this and others should be game specific defaults
  }
  else { // train = false
    settings.next_action_kind_ =
      cifre::AlphaZeroSettings::NextActionKind::MAX_VISITED;
    settings.dirichlet_alpha_ = 0.0; // disable dirichlet
    settings.max_search_depth_ = 4;
    settings.cpuct_ = 1.0;
  }

  return settings;
}

bool
TicTacToe::ParseCustomOption(const GameOption& opt, const std::string& arg_str)
{
  if (opt.Id() == OPTION_GAME_SHAPE) {
    game_shape_ = cifre::BoardSize2D(arg_str);
    GameCheckTrueMsg(game_shape_.RowCount() >= 2,
                     Name() + " board must be " + cifre::BoardSize2D(2, 2).ToString() +
                     " or larger");
    GameCheckTrueMsg(game_shape_.RowCount() == game_shape_.ColumnCount(),
                     Name() + " board must be square");
  }
  else {
    return false;
  }

  return true;
}

cifre::State*
TicTacToe::InitialState(
  const std::vector<Player*>& players, size_t starting_player_idx) const
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
