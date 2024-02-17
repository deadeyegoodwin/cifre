// Copyright 2024, David Goodwin. All rights reserved.

#include <cstddef>
#include <cstdint>
#include <string>
#include <sstream>

#include "celcath_player.h"

namespace cifre { namespace celcath {

//
// Player
//
Player::Player(common::Logger* logger, const std::string& name,
               char board_char, int32_t neighbor_value) :
  logger_(logger), name_(name), board_char_(board_char), neighbor_value_(neighbor_value)
{
}

std::string
Player::ToString(common::Logger::Level filter_level) const
{
  std::stringstream sstr;
  return sstr.str();
}

}} // namespace cifre::celcath
