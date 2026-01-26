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
#include "gettysburg_action.h"
#include "gettysburg_board.h"
#include "gettysburg_board_cell_factory.h"
#include "gettysburg_board_move_factory.h"
#include "gettysburg_player.h"
#include "common_error.h"
#include "common_util.h"

namespace cifre { namespace gettysburg {

static const cifre::MatrixSize MIN_GAME_SHAPE {5, 7};
static const cifre::MatrixSize MAX_GAME_SHAPE {
  Board::MAX_BOARD_ROWS, Board::MAX_BOARD_COLUMNS};
static const cifre::MatrixSize DEFAULT_GAME_SHAPE = MAX_GAME_SHAPE;
static const size_t DEFAULT_MAX_TURNS_PER_GAME = 20;

//
// Gettysburg
//
class Gettysburg : public cifre::Game {
public:
  // Construct a gettysburg game.
  Gettysburg();

  // See cifre::Game
  const std::string& Name() const override {
    static std::string n("Gettysburg"); return n; }

  // See cifre::Game
  const std::string& ExecutableName() const override {
    static std::string n("gettysburg"); return n; }

  // See cifre::Game
  const std::string StartBanner() const;
  const std::string EndBanner() const;

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

  // Custom one-time initialization for gettysburg.
  void CustomInitialize();

private:
  GameOption::OptionId OPTION_GAME_SHAPE;
  GameOption::OptionId OPTION_MAX_TURNS_PER_GAME;
  GameOption::OptionId OPTION_DISPLAY_LARGE_BOARD;
  GameOption::OptionId OPTION_DISPLAY_VERSION;

  OOB::Scenario scenario_;
  cifre::MatrixSize game_shape_;
  cifre::MatrixSize plane_shape_;
  BoardHex::BoardDisplayKind starting_display_kind_;
  uint32_t starting_display_version_;
  std::unique_ptr<BoardCellFactory> cell_factory_;
  std::unique_ptr<BoardMoveFactory> move_factory_;
};

Gettysburg::Gettysburg() :
  cifre::Game(), game_shape_(DEFAULT_GAME_SHAPE), plane_shape_(DEFAULT_GAME_SHAPE),
  starting_display_kind_(BoardHex::BoardDisplayKind::SMALL), starting_display_version_(0)
{
  cifre::GameOption::OptionId custom_id = cifre::Game::FirstCustomOptionId;
  OPTION_GAME_SHAPE = custom_id++;
  OPTION_MAX_TURNS_PER_GAME = custom_id++;
  OPTION_DISPLAY_LARGE_BOARD = custom_id++;
  OPTION_DISPLAY_VERSION = custom_id++;

  // Set game-dependent options that are declared in cifre::Game
  max_turns_per_game_ = DEFAULT_MAX_TURNS_PER_GAME;
}

const std::string
Gettysburg::StartBanner() const
{
  std::stringstream sstr;

  sstr
    << "**********************************\n"
    << "**          Gettysburg          **\n"
    << "**********************************";
  return sstr.str();
}

const std::string
Gettysburg::EndBanner() const
{
  std::stringstream sstr;
  sstr
    << "Gettysburg\n"
    << common::DuplicateAndIndent(cell_factory_->ToString(), 2 /* indent */);
  return sstr.str();
}

std::vector<cifre::GameOption>
Gettysburg::CustomGameOptions() const
{
  std::vector<cifre::GameOption> custom;
  custom = {
    {OPTION_GAME_SHAPE, "game-shape", GameOption::ArgInt,
     "Size of the " + Name() + " board. A board with N rows and M columns is specified "
     "as '--game-shape=N,M'. "
     "Default is '--game-shape=" + DEFAULT_GAME_SHAPE.ToString() + "'."},
    {OPTION_MAX_TURNS_PER_GAME, "max-turns-per-game", GameOption::ArgInt,
     "Maximum number of turns allowed per game."},
    {OPTION_DISPLAY_LARGE_BOARD, "display-board-large", GameOption::ArgNone,
     "Display large board by default."},
    {OPTION_DISPLAY_VERSION, "display-board-version", GameOption::ArgInt,
     "Display board using specified display version by default."}};

  return custom;
}

bool
Gettysburg::ParseCustomOption(const GameOption& opt, const std::string& arg_str)
{
  if (opt.Id() == OPTION_GAME_SHAPE) {
    game_shape_ = cifre::MatrixSize(arg_str);
    GameCheckTrueMsg(
      (game_shape_.RowCount() >= MIN_GAME_SHAPE.RowCount()) &&
      (game_shape_.ColumnCount() >= MIN_GAME_SHAPE.ColumnCount()) &&
      (game_shape_.RowCount() <= MAX_GAME_SHAPE.RowCount()) &&
      (game_shape_.ColumnCount() <= MAX_GAME_SHAPE.ColumnCount()),
      Name() + " board must be between " + MIN_GAME_SHAPE.ToString() +
      " and " + MAX_GAME_SHAPE.ToString());
  }
  else if (opt.Id() == OPTION_MAX_TURNS_PER_GAME) {
    max_turns_per_game_ = cifre::GameOption::ParseUnsigned(opt, arg_str, 1 /* min */);
  }
  else if (opt.Id() == OPTION_DISPLAY_LARGE_BOARD) {
    starting_display_kind_ = BoardHex::BoardDisplayKind::LARGE;
  }
  else if (opt.Id() == OPTION_DISPLAY_VERSION) {
    starting_display_version_ = cifre::GameOption::ParseUnsigned(opt, arg_str, 0 /* min */);
  }
  else {
    return false;
  }

  return true;
}

cifre::AlphaZeroSettings
Gettysburg::DefaultAlphaZeroSettings(bool training, bool az_vs_az) const
{
  cifre::AlphaZeroSettings settings;
  settings.training_ = training;

  settings.search_count_ = 800;
  settings.max_search_depth_ = 128;
  settings.cpuct_ = 10.0;

  // If 'training' then using MCTS to self-play and so want to use
  // both dirichlet and weighted-random action. If two alphazero
  // players are playing each other ('az_vs_az') then no dirichlet but
  // still use weighted-random action or else every game will be
  // identical. Otherwise no dirichlet and choose the best move.
  if (training) {
    settings.next_action_kind_ =
      cifre::AlphaZeroSettings::NextActionKind::RANDOM_VISIT_WEIGHTED;
    settings.dirichlet_alpha_ = 10.0 / float(TOTAL_ACTIONID_COUNT);
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
Gettysburg::CreatePlayer(
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
Gettysburg::CreateInitialState(
  const std::vector<Player*>& players, size_t starting_player_idx) const
{
  // Player 0 is 'c' (confederate) and player 1 is 'u' (union).
  players[0]->SetChar(Board::ConfederateChar().Char());
  players[1]->SetChar(Board::UnionChar().Char());

  // 'starting_player_id' is ignored because gettysburg must start
  // with confederates.
  auto board = new Board(
    cell_factory_.get(), move_factory_.get(),
    scenario_, game_shape_, plane_shape_, players,
    std::pmr::new_delete_resource());
  board->SetDisplay(starting_display_kind_, starting_display_version_);
  return StateUniquePtr(board, StateDeleterNewDelete<Board>());
}

void
Gettysburg::CustomInitialize()
{
  // A full-size game is DEFAULT_GAME_SHAPE. Anything smaller than
  // that is considered a "small game" and forces the use of the SMALL
  // scenario.
  scenario_ = OOB::Scenario::JULY_1;
  if (game_shape_ != DEFAULT_GAME_SHAPE) {
    scenario_ = OOB::Scenario::SMALL;
  }

  // The input planes are based on the size of the board. Because the
  // model is trained/used by always having the unit currently being
  // controlled at the center of the plane, there must be a border
  // around the map to allow any actual map hex to be at the center of
  // the plane.
  plane_shape_ = cifre::MatrixSize(
    ((game_shape_.RowCount() - 1) * 2) + 1,
    ((game_shape_.ColumnCount() - 1) * 2) + 1);

  cell_factory_.reset(
    new BoardCellFactory(
      scenario_,
      game_shape_.RowCount(), game_shape_.ColumnCount(),
      plane_shape_.RowCount(), plane_shape_.ColumnCount()));
  move_factory_.reset(
    new BoardMoveFactory(
      cell_factory_.get(), scenario_,
      game_shape_.RowCount(), game_shape_.ColumnCount()));
}

}} // namespace cifre::gettysburg

int
main(int argc, char **argv)
{
  cifre::gettysburg::Gettysburg game;
  game.Initialize(argc, argv);
  game.CustomInitialize();
  game.Play();

  return 0;
}
