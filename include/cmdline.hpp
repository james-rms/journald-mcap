#ifndef CMDLINE_HPP
#define CMDLINE_HPP

#include <string>

enum TimePoint {
  TIME_UNIX,
  TIME_NOW,
  TIME_BOOT,
  TIME_SHUTDOWN,
  TIME_WAIT,
};

struct Options {
  std::string output_filename = "out.mcap";
  TimePoint start = TIME_BOOT;
  uint64_t start_sec = 0;
  TimePoint end = TIME_NOW;
  uint64_t end_sec = 0;
  bool help = false;
  bool version = false;
};

int parse_options(int argc, const char **argv, Options *options);

#endif
