// Copyright 2025, David Goodwin. All rights reserved.
#pragma once

#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include "common_logger.h"

namespace cifre {

//
// Logger
//
// Derived from common::Logger but hides all of those methods to
// present a different interface.
//
class Logger : protected common::StreamLogger {
public:
  // Types of logging available.
  enum class Kind {
    GAME,
    ALGO,
    TRITON,
    TRITON_CLIENT,    // verbose logging from triton client

    // must be last kind
    LAST
  };
  static constexpr size_t KindCount = size_t(Kind::LAST);

  static std::string KindToString(const Kind kind) {
    switch (kind) {
    case Kind::GAME: return "GAME";
    case Kind::ALGO: return "ALGO";
    case Kind::TRITON: return "TRITON";
    case Kind::TRITON_CLIENT: return "TRITON_CLIENT";
    case Kind::LAST: return "LAST";
    }
    return "<none>"; // unreachable
  }

public:
  // Construct logger that writes to a stream.  Logger holds a
  // reference to 'of' so the caller is required to ensure that the
  // lifetime of 'of' extends over the lifetime of 'this' logger.
  Logger(std::ostream& of) :
    common::StreamLogger(common::Logger::Level::LOW, common::Logger::Timestamp::NONE, of),
    kinds_(KindCount, false) { }

  // Open named file and stream logging to that file.
  Logger(const std::string& filename, bool buffered = true) :
    common::StreamLogger(common::Logger::Level::LOW, common::Logger::Timestamp::NONE, filename, buffered),
    kinds_(KindCount, false) { }

  virtual ~Logger() = default;

  // Return true if 'k'ind of logging is enabled.
  bool IsEnabled(Kind k) const { return kinds_[size_t(k)]; }

  // Enable or disable a type of logging.
  void Enable(Kind k) { kinds_[size_t(k)] = true; }
  void Disable(Kind k) { kinds_[size_t(k)] = false; }

  // Enable or disable all logging.
  void EnableAll() { std::fill(kinds_.begin(), kinds_.end(), true); }
  void DisableAll() { std::fill(kinds_.begin(), kinds_.end(), false); }

  // Log a message.
  void Write(const std::string& msg, size_t indent=0, bool endline=true) {
    Log(msg, indent, endline); }

  // Return string representation of the logger.
  std::string ToString() const {
    std::string s; for (auto k = Kind::GAME; k < Kind::LAST; k = Kind(size_t(k) + 1)) {
      s += KindToString(k) + ": " + (IsEnabled(k) ? "true" : "false");
      if (k != Kind::LAST) { s += ", "; }}; return s; }

protected:
  std::vector<bool> kinds_;
};

//
// Logging defines.
//
// These avoid the evaluation of the message expression if the message
// is not going to be logged. The _IE versions of the macros allow the
// indent (I) and endline (E) to be specified. The non _IE versions
// use indent=0 and enline=true.
//
#define LOG_GAME(L, M) LOG_GAME_IE(L, M, 0, true)
#define LOG_GAME_IE(L, M, I, E)                                         \
  do {                                                                  \
    cifre::Logger& logger__ = (L);                                      \
    if (logger__.IsEnabled(cifre::Logger::Kind::GAME)) {                \
      logger__.Write(M, I, E);                                          \
    }                                                                   \
  } while (false)

#define LOG_ALGO(L, M) LOG_ALGO_IE(L, M, 0, true)
#define LOG_ALGO_IE(L, M, I, E)                                         \
  do {                                                                  \
    cifre::Logger& logger__ = (L);                                      \
    if (logger__.IsEnabled(cifre::Logger::Kind::ALGO)) {                \
      logger__.Write(M, I, E);                                          \
    }                                                                   \
  } while (false)

#define LOG_TRITON(L, M) LOG_TRITON_IE(L, M, 0, true)
#define LOG_TRITON_IE(L, M, I, E)                                       \
  do {                                                                  \
    cifre::Logger& logger__ = (L);                                      \
    if (logger__.IsEnabled(cifre::Logger::Kind::TRITON)) {              \
      logger__.Write(M, I, E);                                          \
    }                                                                   \
  } while (false)

} // namespace cifre
