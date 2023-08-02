#include <utility>

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "cmdline.hpp"
#include "journal.hpp"

#include "fake_systemd.hpp"

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

void test_get_ts(uint64_t expected, uint64_t actual_usec, int journald_rval,
                 int expected_rval) {
  sd_journal j;
  j.usec = actual_usec;
  j.rval = journald_rval;
  uint64_t out = 0;
  REQUIRE(get_ts(&j, &out) == expected_rval);
  if (expected_rval != 0) {
    return;
  }
  REQUIRE(out == expected);
}

TEST_CASE("converts us to ns", "[journal]") { test_get_ts(10000, 10, 0, 0); }
TEST_CASE("passes through nonzero rval", "[journal]") {
  test_get_ts(0, 10, 3, 3);
}
TEST_CASE("fails on very large microseconds rval", "[journal]") {
  test_get_ts(0, UINT64_MAX / 500, 0, -ERANGE);
}

void test_get_transport(const std::map<std::string, std::string> &entries,
                        int journald_rval, Transport expected) {
  sd_journal j;
  j.rval = journald_rval;
  j.entries = entries;
  REQUIRE(get_transport(&j) == expected);
}

TEST_CASE("gets the transport value", "[get_transport]") {
  for (int i = 1; i < _TRANSPORT_COUNT; ++i) {
    Transport t = (Transport)(i);
    test_get_transport({{"_TRANSPORT", name_for_transport(t)}}, 0, t);
  }
}
TEST_CASE("return unknown for unknown transports", "[get_transport]") {
  test_get_transport({{"_TRANSPORT", "burgus"}}, 0, TRANSPORT_UNKNOWN);
}
TEST_CASE("return unkown on err return from journald", "[get_transport]") {
  test_get_transport({{"_TRANSPORT", "audit"}}, 1, TRANSPORT_UNKNOWN);
}
