// Copyright 2024, David Goodwin. All rights reserved.

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>

#include "celcath_game.h"
#include "common_util.h"

namespace cifre { namespace celcath {

//
// Game
//
Game::Game(common::Logger* logger, std::unique_ptr<Stepper> stepper,
           Board&& board, std::vector<const Player*>&& players) :
  logger_(logger), stepper_(std::move(stepper)),
  board_(std::move(board)), players_(std::move(players))
{
  // One action for each cell on the board (toggle liveness), plus one
  // action to finish turn.
  const BoardSize& board_size = board_.Size();
  total_action_count_ = (board_size.RowCount() * board_size.ColumnCount()) + 1;
}

uint64_t
Game::TotalActionCount() const
{
  return total_action_count_;
}

const std::vector<uint64_t>&
Game::StateActions(const State& state) const
{
  // FIXME
  static std::vector<uint64_t> sa;
  return sa;
}

void
Game::ApplyAction(State* state) const
{
}

void
Game::Play()
{
  // Show the initial board.
  std::cout << board_.ToString() << '\n';
#if 0
  while (true) {
    // Players alternate turns. During the turn the player can make up
    // to 'max_moves_per_turn', and is required to make at least
    // 'min_moves_per_turn'. FIXME ruleset.
    const size_t max_moves_per_turn = 4;
    const size_t min_moves_per_turn = 1;

    for (Player* player : players_) {
      size_t completed_moves = 0;
      bool turn_completed = false;
      while (!turn_completed) {
        std::cout
          << player->Name() << " [" << (completed_moves + 1)
          << " of " << max_moves_per_turn << "]: ";
        Action action = player->NextAction(*board_);
        switch (action.Kind()) {
        case Action::ActionKind::QUIT:
          std::cout << "quitting\n";
          goto game_over;

        case Action::ActionKind::SHOW_BOARD:
          std::cout << board_->ToString() << '\n';

        case Action::ActionKind::END_TURN:
          if (completed_moves < min_moves_per_turn) {
            std::cout
              << "turn not finished, must complete "
              << (min_moves_per_turn - completed_moves) << " moves\n";
          } else {
            turn_completed = true;
          }

        case Action::ActionKind::MOVE: {
          const std::string& cmd = action.CommandString();
          Cell* cell = board_->StringToCell(cmd);
          if (cell == nullptr) {
            std::cout << "invalid cell position '" << cmd << "'\n";
          } else {
            Player* cell_owner = cell->Owner();
            if (cell_owner == nullptr) {
              cell->SetOwner(player);
              completed_moves++;
            } else if (cell_owner == player) {
              cell->SetOwner(nullptr);
              completed_moves++;
            } else {
              std::cout << "invalid move, cell '" << cmd << "' occupied by another player\n";
            }
          }
        }
        }

        if (completed_moves == max_moves_per_turn) {
          std::cout << "turn finished, completed maximum moves\n";
          turn_completed = true;
        }
      }

      // Updated board...
      std::cout << board_->ToString() << '\n';

      // Use the generation stepper to evolve the board to the next
      // generation.
      std::cout << "stepping: " << stepper_->Name() << '\n';
      stepper_->Step(board_, players_);
      std::cout << board_->ToString() << '\n';
    }
  }
game_over:
#endif
    // Final board...
    std::cout << "final:\n";
    std::cout << board_.ToString() << '\n';
}

}} // namespace cifre::celcath
