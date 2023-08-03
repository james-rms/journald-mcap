#include "cmdline.hpp"
#include <cassert>
#include <string_view>

enum CmdlineToken {
  TOKEN_EMPTY,
  TOKEN_HELP,
  TOKEN_VERSION,
  TOKEN_START,
  TOKEN_END,
  TOKEN_OUTPUT,
  TOKEN_INT,
  TOKEN_NOW,
  TOKEN_BOOT,
  TOKEN_SHUTDOWN,
  TOKEN_WAIT,
  TOKEN_STRING,
};

CmdlineToken token_of(const char *arg) {
  std::string_view this_arg(arg);
  if (this_arg == "-h" || this_arg == "--help") {
    return TOKEN_HELP;
  } else if (this_arg == "--version") {
    return TOKEN_VERSION;
  } else if (this_arg == "-o" || this_arg == "--output") {
    return TOKEN_OUTPUT;
  } else if (this_arg == "-s" || this_arg == "--start") {
    return TOKEN_START;
  } else if (this_arg == "-e" || this_arg == "--end") {
    return TOKEN_END;
  } else if (this_arg == "") {
    return TOKEN_EMPTY;
  } else if (this_arg == "now") {
    return TOKEN_NOW;
  } else if (this_arg == "boot") {
    return TOKEN_BOOT;
  } else if (this_arg == "shutdown") {
    return TOKEN_SHUTDOWN;
  } else if (this_arg == "wait") {
    return TOKEN_WAIT;
  } else if (this_arg.find_first_not_of("0123456789") == this_arg.npos) {
    return TOKEN_INT;
  }
  return TOKEN_STRING;
}

int parse_options(int argc, const char **argv, Options *options) {
  assert(options != NULL);
  for (int i = 1; i < argc; ++i) {
    switch (token_of(argv[i])) {
    case TOKEN_HELP:
      options->help = true;
      break;
    case TOKEN_VERSION:
      options->version = true;
      break;
    case TOKEN_END:
      if (i == argc - 1) {
        fprintf(stderr, "expected an argument after %s\n", argv[i]);
        return 1;
      }
      switch (token_of(argv[i + 1])) {
      case TOKEN_SHUTDOWN:
        options->end = TIME_SHUTDOWN;
        break;
      case TOKEN_NOW:
        options->end = TIME_NOW;
        break;
      case TOKEN_WAIT:
        options->end = TIME_WAIT;
        break;
      case TOKEN_INT:
        options->end = TIME_UNIX;
        options->end_sec = std::stoull(std::string(argv[i + 1]));
        break;
      default:
        fprintf(
            stderr,
            "expected 'now', 'shutdown', 'wait', or a timestamp, got '%s'\n",
            argv[i + 1]);
        return 1;
      }
      i++;
      break;
    case TOKEN_START:
      if (i == argc - 1) {
        fprintf(stderr, "expected an argument after %s\n", argv[i]);
        return 1;
      }
      switch (token_of(argv[i + 1])) {
      case TOKEN_BOOT:
        options->start = TIME_BOOT;
        break;
      case TOKEN_NOW:
        options->start = TIME_NOW;
        break;
      case TOKEN_INT:
        options->start = TIME_UNIX;
        options->start_sec = std::stoull(std::string(argv[i + 1]));
        break;
      default:
        fprintf(stderr, "expected 'boot', 'now', or a timestamp, got '%s'\n",
                argv[i + 1]);
        return 1;
      }
      i++;
      break;
    case TOKEN_OUTPUT:
      if (i == argc - 1 || token_of(argv[i + 1]) != TOKEN_STRING) {
        fprintf(stderr, "expected a filename after %s\n", argv[i]);
        return 1;
      }
      options->output_filename = std::string(argv[i + 1]);
      // skip past the next argument
      i++;
      break;
    default:
      fprintf(stderr, "unexpected argument '%s', see --help for usage\n",
              argv[i]);
      return 1;
    }
  }
  return 0;
}
