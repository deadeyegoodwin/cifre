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
Game::Game(common::Logger* logger, Stepper* stepper, Board* board,
           const std::vector<Player*>& players) :
  cifre::Game(logger), stepper_(stepper), board_(board), players_(players)
{
}

void
Game::Play()
{
  // Show the initial board.
  std::cout << board_->ToString() << '\n';

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
        std::string line;

        std::cout
          << player->Name() << " [" << (completed_moves + 1)
          << " of " << max_moves_per_turn << "]: ";
        std::getline(std::cin, line);
        common::Trim(line);

        // Quit game
        if (line == "q") {
          std::cout << "quitting\n";
          goto game_over;
        }
        // Finish turn
        else if (line == "f") {
          if (completed_moves < min_moves_per_turn) {
            std::cout
              << "turn not finished, must complete "
              << (min_moves_per_turn - completed_moves) << " moves\n";
          } else {
            turn_completed = true;
          }
        }
        // Show board
        else if (line == "b") {
          std::cout << board_->ToString() << '\n';
        }
        // Occupy or abandon cell
        else {
          Cell* cell = board_->StringToCell(line);
          if (cell == nullptr) {
            std::cout << "invalid cell position '" << line << "'\n";
          } else {
            Player* cell_owner = cell->Owner();
            if (cell_owner == nullptr) {
              cell->SetOwner(player);
              completed_moves++;
            } else if (cell_owner == player) {
              cell->SetOwner(nullptr);
              completed_moves++;
            } else {
              std::cout << "invalid move, cell '" << line << "' occupied by another player\n";
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
    // Final board...
    std::cout << "final:\n";
    std::cout << board_->ToString() << '\n';
}

}} // namespace cifre::celcath
