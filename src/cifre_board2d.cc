// Copyright 2024, David Goodwin. All rights reserved.

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <regex>
#include <string>
#include <sstream>

#include "cifre_board2d.h"
#include "common_error.h"

namespace cifre {

//
// BoardSize2D
//
BoardSize2D::BoardSize2D(const std::string& str)
{
  static std::regex regex_square("^([1-9][0-9]*)$");
  static std::regex regex_rectangle("^([1-9][0-9]*)[x,]([1-9][0-9]*)$");

  std::smatch m;
  if (!std::regex_match(str, m, regex_rectangle)) {
    THROWIF(!std::regex_match(str, m, regex_square),
            "board size must be specified as 'NxM' or 'N,M'");
    row_cnt_ = column_cnt_ = std::stoi(m[1]);
  }
  else {
    row_cnt_ = std::stoi(m[1]);
    column_cnt_ = std::stoi(m[2]);
  }
}

std::string
BoardSize2D::ToString() const
{
  return std::to_string(row_cnt_) + "x" + std::to_string(column_cnt_);
}

//
// Board2D
//
Board2D::Board2D(const cifre::BoardSize2D& size, const std::vector<Player*>& players,
                 size_t starting_player_id) :
  size_(size), players_(players), active_player_id_(starting_player_id),
  terminal_status_(cifre::StateTerminalStatus::TerminalKind::NON_TERMINAL)
{
  THROWIF(active_player_id_ >= EmptyPlayerId(),
          "invalid starting player index " + std::to_string(active_player_id_));

  // Derived class must size 'cells_flattened_' and set 'cells' as in
  // the below example where there is one float in the triton
  // representation representing each cell on the board.
  //
  // cells_flattened_.resize(size_.CellCount() * sizeof(float));
  // cells_ = reinterpret_cast<float*>(cells_flattened_.data());
}

Board2D::Board2D(const Board2D& other) :
  size_(other.size_), players_(other.players_),
  active_player_id_(other.active_player_id_), terminal_status_(other.terminal_status_),
  valid_action_ids_(other.valid_action_ids_), cells_flattened_(other.cells_flattened_)
{
  cells_ = reinterpret_cast<float*>(cells_flattened_.data());
}

size_t
Board2D::Hash() const
{
  const std::vector<uint8_t>& bytes = TritonRepresentation();

  const uint64_t mulp = 2654435789;
  uint64_t hash = 104395301;
  for (uint8_t b : bytes) {
    hash += (b * mulp) ^ (hash >> 23);
  }

  return hash ^ (hash << 37);
}

bool
Board2D::Equal(const State* other) const
{
  return TritonRepresentation() == other->TritonRepresentation();
}

bool
Board2D::IsValidAction(const Action& action) const
{
  switch (action.Kind()) {
  case Action::ActionKind::INVALID:
    return false;

  case Action::ActionKind::HELP:
  case Action::ActionKind::QUIT:
  case Action::ActionKind::SHOW_STATE:
    return true;

  case Action::ActionKind::ACTIONID:
    return valid_action_ids_.find(action.ActionId()) != valid_action_ids_.end();
  }

  return false;
}

} // namespace cifre
