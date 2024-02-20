// Copyright 2024, David Goodwin. All rights reserved.

#include "celcath_settings.h"

namespace cifre { namespace celcath {

//
// Settings
//
Settings::Settings()
{
  AllowedSetting("board-row-count", (int32_t)6);
  AllowedSetting("board-column-count", (int32_t)6);
  AllowedSetting("min-actions-per-turn", (int32_t)1);
  AllowedSetting("max-actions-per-turn", (int32_t)1);
  AllowedSetting("starting-player-id", (int32_t)0);

  AllowedSetting("total-action-id-count", (int32_t)0);
  AllowedSetting("end-turn-action-id", (int32_t)0);
}

Settings::Settings(const json& j) :
  Settings()
{
  FromJson(j);
  Sync();
}

void
Settings::Sync()
{
  Setting("board-row-count", board_row_count_);
  Setting("board-column-count", board_column_count_);
  Setting("min-actions-per-turn", min_actions_per_turn_);
  Setting("max-actions-per-turn", max_actions_per_turn_);
  Setting("starting-player-id", starting_player_id_);

  Setting("total-action-id-count", total_action_id_count_);
  Setting("end-turn-action-id", end_turn_action_id_);
}

}} // namespace cifre::celcath
