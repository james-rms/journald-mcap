#include <string>

struct Options {
  std::string output_filename = "out.mcap";
  uint64_t start_sec = 0;
  uint64_t end_sec = UINT64_MAX;
  bool start_now = false;
  bool watch = false;
  bool help = false;
  bool version = false;
};

int parse_options(int argc, const char **argv, Options *options);
