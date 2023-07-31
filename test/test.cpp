#include <utility>

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "cmdline.hpp"

void test_options(std::vector<const char *> args,
                  const Options &expected_options, int expected_rval) {
  Options options;
  int rval = parse_options(args.size(), args.data(), &options);
  REQUIRE(rval == expected_rval);
  if (expected_rval != 0) {
    // do not assert on options if expected rval is nonzero
    return;
  }
  REQUIRE(options.end_sec == expected_options.end_sec);
  REQUIRE(options.start_sec == expected_options.start_sec);
  REQUIRE(options.version == expected_options.version);
  REQUIRE(options.help == expected_options.help);
  REQUIRE(options.start_now == expected_options.start_now);
  REQUIRE(options.output_filename == expected_options.output_filename);
  REQUIRE(options.watch == expected_options.watch);
}

TEST_CASE("empty", "[cmdline]") { test_options({"exe"}, Options{}, 0); }
TEST_CASE("unknown flag", "[cmdline]") {
  test_options({"exe", "--unknown"}, Options{}, 1);
}
TEST_CASE("version flag", "[cmdline]") {
  test_options({"exe", "--version"}, Options{.version = true}, 0);
}
TEST_CASE("help flag", "[cmdline]") {
  test_options({"exe", "--help"}, Options{.help = true}, 0);
}
TEST_CASE("no arg for version", "[cmdline]") {
  test_options({"exe", "--version", "12345"}, Options{}, 1);
}
TEST_CASE("not an integer", "[cmdline]") {
  test_options({"exe", "--start", "wubbus"}, Options{}, 1);
}
TEST_CASE("parses integers", "[cmdline]") {
  test_options({"exe", "--start", "12345"}, Options{.start_sec = 12345}, 0);
}
TEST_CASE("starts now", "[cmdline]") {
  test_options({"exe", "--start", "now"}, Options{.start_now = true}, 0);
}
TEST_CASE("end at integer", "[cmdline]") {
  test_options({"exe", "--end", "12345"}, Options{.end_sec = 12345}, 0);
}
TEST_CASE("set flag with and without argument", "[cmdline]") {
  test_options({"exe", "--start", "12345", "--watch"},
               Options{.start_sec = 12345, .watch = true}, 0);
}
TEST_CASE("set string and int flag", "[cmdline]") {
  test_options(
      {"exe", "--output", "123.mcap", "--start", "12345", "--watch"},
      Options{.output_filename = "123.mcap", .start_sec = 12345, .watch = true},
      0);
}
