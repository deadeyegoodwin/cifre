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
#include "celcath_board.h"
#include "celcath_player.h"
#include "common_error.h"
#include "common_logger.h"
#include "common_util.h"

namespace cifre { namespace celcath {

static const cifre::BoardSize2D DEFAULT_GAME_SHAPE {8, 8};
static const size_t DEFAULT_MAX_TURNS_PER_GAME = 50;

//
// Celcath
//
class Celcath : public cifre::Game {
public:
  // Construct a celcath game.
  Celcath();

  // See cifre::Game
  const std::string& Name() const override {
    static std::string n("Celcath"); return n; }

  // See cifre::Game
  const std::string& ExecutableName() const override {
    static std::string n("celcath"); return n; }

  // See cifre::Game
  std::vector<cifre::GameOption> CustomGameOptions() const override;

  // See cifre::Game
  bool ParseCustomOption(const GameOption& opt, const std::string& arg_str) override;

  // See cifre::Game
  cifre::AlphaZeroSettings DefaultAlphaZeroSettings(const bool train) const override;

  // See cifre::Game
  Player* CreatePlayer(
    const Player::Kind kind, const std::string& name, const char ch,
    const std::shared_ptr<cifre::Algo>& algo) const override;

  // See cifre::Game
  State* CreateInitialState(
    const std::vector<Player*>& players, size_t starting_player_idx) const override;

private:
  GameOption::OptionId OPTION_GAME_SHAPE;
  GameOption::OptionId OPTION_MAX_TURNS_PER_GAME;

  cifre::BoardSize2D game_shape_;
};

Celcath::Celcath() : cifre::Game(), game_shape_(DEFAULT_GAME_SHAPE)
{
  cifre::GameOption::OptionId custom_id = cifre::Game::FirstCustomOptionId;
  OPTION_GAME_SHAPE = custom_id++;
  OPTION_MAX_TURNS_PER_GAME = custom_id++;

  // Set game-dependent options that are declared in cifre::Game
  max_turns_per_game_ = DEFAULT_MAX_TURNS_PER_GAME;
}

std::vector<cifre::GameOption>
Celcath::CustomGameOptions() const
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
Celcath::ParseCustomOption(const GameOption& opt, const std::string& arg_str)
{
  if (opt.Id() == OPTION_GAME_SHAPE) {
    game_shape_ = cifre::BoardSize2D(arg_str);
    GameCheckTrueMsg((game_shape_.RowCount() >= 4) && (game_shape_.ColumnCount() >= 4),
                     Name() + " board must be " + cifre::BoardSize2D(4, 4).ToString() +
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
Celcath::DefaultAlphaZeroSettings(const bool train) const
{
  cifre::AlphaZeroSettings settings;
  settings.training_ = train;

  if (train) { // train = true
    settings.next_action_kind_ =
      cifre::AlphaZeroSettings::NextActionKind::RANDOM_VISIT_WEIGHTED;
    settings.dirichlet_alpha_ = 10.0 / 64.0;
    settings.dirichlet_sample_count_ = 2117;
    settings.dirichlet_ratio_ = 0.25;
    settings.search_count_ = 500;
    settings.max_search_depth_ = 64;
    settings.cpuct_ = 10.0;
  }
  else { // train = false
    settings.next_action_kind_ =
      cifre::AlphaZeroSettings::NextActionKind::MAX_VISITED;
    settings.dirichlet_alpha_ = 0.0; // disable dirichlet
    settings.search_count_ = 500;
    settings.max_search_depth_ = 64;
    settings.cpuct_ = 1.0;
  }

  return settings;
}

cifre::Player*
Celcath::CreatePlayer(
  const Player::Kind kind, const std::string& name, const char ch,
  const std::shared_ptr<cifre::Algo>& algo) const
{
  switch (kind) {
  case Player::Kind::UNKNOWN:
    break;
  case Player::Kind::HUMAN:
    assert(algo == nullptr);
    return new HumanPlayer(logger_, name, ch);
  case Player::Kind::ALGO:
    assert(algo != nullptr);
    return new cifre::AlgoPlayer(logger_, name, ch, algo);
  }

  return nullptr;
}

cifre::State*
Celcath::CreateInitialState(
  const std::vector<Player*>& players, size_t starting_player_idx) const
{
  static std::shared_ptr<Stepper> stepper = std::make_shared<B3S23Stepper>(logger_);
  return new Board(game_shape_, players, starting_player_idx, stepper);
}

}} // namespace cifre::celcath

int
main(int argc, char **argv)
{
  cifre::celcath::Celcath game;
  game.Initialize(argc, argv);
  game.Play();

  return 0;
}
