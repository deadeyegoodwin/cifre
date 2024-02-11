// Copyright 2024, David Goodwin. All rights reserved.
#pragma once

#include <string>

#include "common_logger.h"

namespace cifre {

//
// Game
//
// Interface to a game.
//
class Game {
public:
  Game(common::Logger* logger, const std::string& workdir);

private:
  common::Logger* logger_;
  const std::string workdir_;
};


} // namespace cifre
