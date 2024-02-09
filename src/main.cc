// Copyright 2024, David Goodwin. All rights reserved.

#include <cstdint>
#include <getopt.h>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "cifre_nnet.h"
#include "cifre_triton.h"
#include "cifre_version.h"
#include "common_logger.h"

namespace {

#define _stringify(A) #A
#define CheckTrue(P)                                                    \
  do {                                                                  \
    const bool p__ = (P);                                               \
    if (!p__) {                                                         \
      std::cerr << std::string("error: expected 'true' for: " _stringify(P)) << std::endl; \
      exit(1);                                                          \
    }                                                                   \
  } while (false)

#define CheckTrueMsg(P, MSG)                                            \
  do {                                                                  \
    const bool p__ = (P);                                               \
    if (!p__) {                                                         \
      std::cerr << std::string("error: ") << (MSG) << std::endl;        \
      exit(1);                                                          \
    }                                                                   \
  } while (false)

#define Fail(MSG)                                                     \
  do {                                                                \
    std::cerr << "error: " << (MSG) << std::endl;                     \
    exit(1);                                                          \
  } while (false)

//
// Command-line options and parsing.
//
struct Option {
  static constexpr const char* ArgNone = "";
  static constexpr const char* ArgBool = "boolean";
  static constexpr const char* ArgFloat = "float";
  static constexpr const char* ArgInt = "integer";
  static constexpr const char* ArgStr = "string";

  enum OptionId {
    OPTION_HELP = 1000,
    OPTION_LOG_OFF,
    OPTION_LOG_LOW,
    OPTION_LOG_MED,
    OPTION_LOG_HIGH,
    OPTION_LOG_FILE,
    OPTION_WORKDIR
  };

  Option(OptionId id, std::string flag, std::string arg_desc, std::string desc)
      : id_(id), flag_(flag), arg_desc_(arg_desc), desc_(desc)
  {
  }

  struct option GetLongOption() const
  {
    struct option lo {
      flag_.c_str(), (!arg_desc_.empty()) ? required_argument : no_argument,
          nullptr, id_
    };
    return lo;
  }

  const OptionId id_;
  const std::string flag_;
  const std::string arg_desc_;
  const std::string desc_;
};

static const std::vector<Option> options_ {
  {Option::OPTION_HELP, "help", Option::ArgNone, "Print usage"},

  {Option::OPTION_LOG_OFF, "log-off", Option::ArgNone,
   "Disable all logging."},
  {Option::OPTION_LOG_LOW, "log-low", Option::ArgNone,
   "Enable low-volume logging. Default."},
  {Option::OPTION_LOG_MED, "log-med", Option::ArgNone,
   "Enable medium-volume logging."},
  {Option::OPTION_LOG_HIGH, "log-high", Option::ArgNone,
   "Enable high-volume logging."},
  {Option::OPTION_LOG_FILE, "log-file", Option::ArgStr,
   "File for log output. 'stdout' and 'stderr' are allowed."},

  {Option::OPTION_WORKDIR, "workdir", Option::ArgStr,
   "Working directory, defaults to /tmp/cifre."}
};

static std::string
FormatUsageMessage(std::string str, int offset)
{
  int width = 60;
  int current_pos = offset;
  while (current_pos + width < int(str.length())) {
    int n = str.rfind(' ', current_pos + width);
    if (n != int(std::string::npos)) {
      str.replace(n, 1, "\n\t");
      current_pos += (width + 9);
    }
  }

  return str;
}

static std::string
Usage()
{
  std::stringstream ss;

  ss << "Usage: cifre [options]" << std::endl;
  for (const auto& o : options_) {
    if (!o.arg_desc_.empty()) {
      ss << "  --" << o.flag_ << " <" << o.arg_desc_ << ">" << std::endl
         << "\t" << FormatUsageMessage(o.desc_, 0) << std::endl;
    } else {
      ss << "  --" << o.flag_ << std::endl
         << "\t" << FormatUsageMessage(o.desc_, 0) << std::endl;
    }
  }

  return ss.str();
}

common::StreamLogger*
CreateLogger(const std::string& filename, common::Logger::Level level,
             common::Logger::Timestamp timestamp_kind)
{
  if (filename.empty()) {
    return nullptr;
  }

  if (filename == "stdout") {
    return new common::StreamLogger(level, timestamp_kind, std::cout);
  }
  if (filename == "stderr") {
    return new common::StreamLogger(level, timestamp_kind, std::cerr);
  }

  return new common::StreamLogger(level, timestamp_kind, filename, false /* buffered */);
}

}  // namespace

int
main(int argc, char **argv)
{
  try {
    common::Logger::Level log_level = common::Logger::Level::LOW;
    std::string log_filename = "stdout";
    std::string workdir = "/tmp/cifre";

    std::vector<struct option> long_options;
    for (const auto& o : options_) {
      long_options.push_back(o.GetLongOption());
    }
    long_options.push_back({nullptr, 0, nullptr, 0});

    int flag;
    while ((flag = getopt_long(argc, argv, "", &long_options[0], NULL)) != -1) {
      switch (flag) {
      case Option::OPTION_HELP:
      case '?':
        std::cout << Usage() << std::endl;
        return 1;

      case Option::OPTION_LOG_OFF:
        log_level = common::Logger::Level::OFF;
        break;
      case Option::OPTION_LOG_LOW:
        log_level = common::Logger::Level::LOW;
        break;
      case Option::OPTION_LOG_MED:
        log_level = common::Logger::Level::MED;
        break;
      case Option::OPTION_LOG_HIGH:
        log_level = common::Logger::Level::HIGH;
        break;
      case Option::OPTION_LOG_FILE:
        log_filename = optarg;
        break;

      case Option::OPTION_WORKDIR:
        workdir = optarg;
        break;
      }
    }

    auto logger = std::unique_ptr<common::StreamLogger>(
      CreateLogger(log_filename, log_level, common::Logger::Timestamp::COMPACT));

    // Echo command-line...
    if (logger->FilterLevel() != common::Logger::Level::OFF) {
      std::stringstream ss;
      for (auto i = 0; i < argc; ++i) {
        ss << argv[i] << " ";
      }
      logger->Log(ss.str());
      logger->Log(cifre::VersionToString(true /* show_dependent_versions */));
    }

    // Initialize triton...
    CheckTrueMsg(!workdir.empty(), "--workdir must be specified");
    cifre::Triton triton(logger.get(), workdir);
    CheckTrueMsg(triton.IsReady(), "triton is not ready");

  } catch (const std::exception& ex) {
    std::cerr << "error: " << ex.what() << std::endl;
    exit(1);
  }

  return 0;
}
