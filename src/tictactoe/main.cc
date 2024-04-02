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

static const cifre::BoardSize2D DEFAULT_BOARD_SIZE {3};

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
  GameOption::OptionId OPTION_BOARD_SIZE;

  cifre::BoardSize2D board_size_;
};

TicTacToe::TicTacToe()
  : cifre::Game(), board_size_(DEFAULT_BOARD_SIZE)
{
  cifre::GameOption::OptionId custom_id = cifre::Game::FirstCustomOptionId;
  OPTION_BOARD_SIZE = custom_id++;
}

std::vector<cifre::GameOption>
TicTacToe::CustomGameOptions() const
{
  std::vector<cifre::GameOption> custom;
  custom = {
    {OPTION_BOARD_SIZE, "board-size", GameOption::ArgInt,
     "Size of the tic-tac-toe board. A board with N rows and N columns is specified "
     "as '--board-size=N'. "
     "Default is '--board-size=" + DEFAULT_BOARD_SIZE.ToString() + "'."}};

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
  if (opt.Id() == OPTION_BOARD_SIZE) {
    board_size_ = cifre::BoardSize2D(GameOption::ParseUnsigned(opt, arg_str, 2 /* min */));
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
  return new Board(board_size_, players, starting_player_idx);
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
