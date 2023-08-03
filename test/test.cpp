#include <utility>

#define CATCH_CONFIG_MAIN
#include "vendor/catch.hpp"
#include "vendor/json.hpp"

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
  REQUIRE(options.end == expected_options.end);
  REQUIRE(options.end_sec == expected_options.end_sec);
  REQUIRE(options.start == expected_options.start);
  REQUIRE(options.start_sec == expected_options.start_sec);
  REQUIRE(options.version == expected_options.version);
  REQUIRE(options.help == expected_options.help);
  REQUIRE(options.output_filename == expected_options.output_filename);
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
  test_options({"exe", "--start", "12345"},
               Options{.start = TIME_UNIX, .start_sec = 12345}, 0);
}
TEST_CASE("starts now", "[cmdline]") {
  test_options({"exe", "--start", "now"}, Options{.start = TIME_NOW}, 0);
}
TEST_CASE("end at integer", "[cmdline]") {
  test_options({"exe", "--end", "12345"},
               Options{.end = TIME_UNIX, .end_sec = 12345}, 0);
}
TEST_CASE("set flag with and without argument", "[cmdline]") {
  test_options({"exe", "--start", "12345", "--version"},
               Options{.start = TIME_UNIX, .start_sec = 12345, .version = true},
               0);
}
TEST_CASE("set string and int flag", "[cmdline]") {
  test_options(
      {"exe", "--output", "123.mcap", "--start", "12345", "--end", "wait"},
      Options{.output_filename = "123.mcap",
              .start = TIME_UNIX,
              .start_sec = 12345,
              .end = TIME_WAIT},
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

void test_serialize_json(const std::map<std::string, std::string> &base_entries,
                         const nlohmann::json &extra, uint64_t timestamp) {
  sd_journal j;
  j.entries = base_entries;
  j.rval = 0;
  std::string res = serialize_json(&j, timestamp);
  nlohmann::json expected(base_entries);
  expected.merge_patch(extra);
  REQUIRE(res == expected.dump());
}

// using namespace nlohmann::literals;

TEST_CASE("with a message", "[serialize_json]") {
  test_serialize_json({{"MESSAGE", "foo"}}, R"({
    "message": "foo",
    "level": 0,
    "timestamp": {
      "sec": 10,
      "nsec": 50
    }
  })"_json,
                      10000000050ull);
}

TEST_CASE("with a priority", "[serialize_json]") {
  test_serialize_json({{"MESSAGE", "foo"}, {"PRIORITY", "7"}}, R"({
    "message": "foo",
    "level": 1,
    "timestamp": {
      "sec": 10,
      "nsec": 50
    }
  })"_json,
                      10000000050ull);
}

TEST_CASE("with a transport", "[serialize_json]") {
  test_serialize_json({{"MESSAGE", "foo"}, {"_TRANSPORT", "audit"}}, R"({
    "message": "foo",
    "level": 0,
    "name": "audit",
    "timestamp": {
      "sec": 10,
      "nsec": 50
    }
  })"_json,
                      10000000050ull);
}

TEST_CASE("with an executable", "[serialize_json]") {
  test_serialize_json(
      {
          {"MESSAGE", "foo"},
          {"_TRANSPORT", "audit"},
          {"_EXE", "/usr/bin/ldd"},
      },
      R"({
    "message": "foo",
    "level": 0,
    "name": "/usr/bin/ldd",
    "timestamp": {
      "sec": 10,
      "nsec": 50
    }
  })"_json,
      10000000050ull);
}

TEST_CASE("with a systemd unit", "[serialize_json]") {
  test_serialize_json(
      {
          {"MESSAGE", "foo"},
          {"_TRANSPORT", "audit"},
          {"_EXE", "/usr/bin/ldd"},
          {"_SYSTEMD_UNIT", "ldd.service"},
      },
      R"({
    "message": "foo",
    "level": 0,
    "name": "ldd.service",
    "timestamp": {
      "sec": 10,
      "nsec": 50
    }
  })"_json,
      10000000050ull);
}

TEST_CASE("sets debug file info", "[serialize_json]") {
  test_serialize_json(
      {
          {"MESSAGE", "foo"},
          {"_SYSTEMD_UNIT", "ldd.service"},
          {"CODE_FILE", "main.cpp"},
          {"CODE_LINE", "99"},
      },
      R"({
    "message": "foo",
    "level": 0,
    "name": "ldd.service",
    "file": "main.cpp",
    "line": 99,
    "timestamp": {
      "sec": 10,
      "nsec": 50
    }
  })"_json,
      10000000050ull);
}
