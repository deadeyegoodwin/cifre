// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include "cifre_board2d.h"
#include "common_settings.h"

namespace cifre { namespace celcath {

//
// Settings
//
// Celcath settings.
//
class Settings : public common::MutableSettings {
public:
  Settings();
  Settings(const json& j);

  // Sync the settings in this object to match the values contained in
  // the underlying MutableSettings.
  void Sync();

  // Size of the game board.
  size_t BoardRowCount() const { return board_row_count_; }
  size_t BoardColumnCount() const { return board_column_count_; }
  cifre::BoardSize2D BoardSize() const {
    return BoardSize2D(board_row_count_, board_column_count_); }

  // Minimum and maximum number of actions player must perform before
  // ending a turn.
  size_t MinActionsPerTurn() const { return min_actions_per_turn_; }
  size_t MaxActionsPerTurn() const { return max_actions_per_turn_; }

  // ID of the player that plays first. Player ID is zero based.
  size_t StartingPlayerId() const { return starting_player_id_; };

  size_t TotalActionIdCount() const { return total_action_id_count_; }
  uint64_t StepActionId() const { return step_action_id_; }
  uint64_t EndTurnActionId() const { return end_turn_action_id_; }

private:
  size_t board_row_count_;
  size_t board_column_count_;

  size_t min_actions_per_turn_;
  size_t max_actions_per_turn_;

  size_t starting_player_id_;

  size_t total_action_id_count_;
  uint64_t step_action_id_;
  uint64_t end_turn_action_id_;
};

}} // namespace cifre::celcath
