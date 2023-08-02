#include "vendor/json.hpp"

#include "journal.hpp"

const char *name_for_transport(Transport transport) {
  switch (transport) {
  case TRANSPORT_AUDIT:
    return "audit";
  case TRANSPORT_DRIVER:
    return "driver";
  case TRANSPORT_SYSLOG:
    return "syslog";
  case TRANSPORT_JOURNAL:
    return "journal";
  case TRANSPORT_STDOUT:
    return "stdout";
  case TRANSPORT_KERNEL:
    return "kernel";
  case TRANSPORT_UNKNOWN:
  case _TRANSPORT_COUNT:
  default:
    return "unknown";
  }
}

Transport get_transport(sd_journal *j) {
  size_t length = 0;
  const char *data = NULL;
  int ret =
      sd_journal_get_data(j, "_TRANSPORT", (const void **)(&data), &length);
  if (ret != 0) {
    return TRANSPORT_UNKNOWN;
  }
  std::string_view data_view(data, length);
  std::string_view prefix = std::string_view("_TRANSPORT=");
  if (data_view.substr(0, prefix.size()) != prefix) {
    return TRANSPORT_UNKNOWN;
  }
  std::string_view transport_name = data_view.substr(prefix.size(), length);
  for (int i = 0; i < _TRANSPORT_COUNT; ++i) {
    const char *candidate = name_for_transport((Transport)(i));
    if (transport_name == candidate) {
      return (Transport)(i);
    }
  }
  return TRANSPORT_UNKNOWN;
}

int get_ts(sd_journal *j, uint64_t *out) {
  int err = sd_journal_get_realtime_usec(j, out);
  if (err != 0) {
    return err;
  }
  if (*out >= (UINT64_MAX / 1000ull)) {
    return -ERANGE;
  }
  *out *= 1000ULL;
  return 0;
}

/**
 * @brief maps systemd priorities to foxglove.Log levels.
 * https://wiki.archlinux.org/title/Systemd/Journal#Priority_level
 * https://github.com/foxglove/schemas/blob/main/schemas/jsonschema/Log.json
 */
int level_for_priority(const std::string &priority) {
  if (priority == "0" || priority == "1" || priority == "2") {
    // FATAL
    return 5;
  }
  if (priority == "3") {
    // ERROR
    return 4;
  }
  if (priority == "4") {
    // WARNING
    return 3;
  }
  if (priority == "5" || priority == "6") {
    // INFO
    return 2;
  }
  if (priority == "7") {
    // DEBUG
    return 1;
  }
  // UNKNOWN
  return 0;
}

std::string serialize_json(sd_journal *j, uint64_t timestamp) {
  nlohmann::json out;
  const void *data;
  size_t length;
  SD_JOURNAL_FOREACH_DATA(j, data, length) {
    std::string_view data_view((const char *)data, length);
    size_t eq_pos = data_view.find('=');
    if (eq_pos == data_view.npos) {
      continue;
    }
    if (eq_pos >= length - 1) {
      continue;
    }
    std::string_view key = data_view.substr(0, eq_pos);
    std::string_view val = data_view.substr(eq_pos + 1, length);
    out[std::string(key)] = std::string(val);
  }
  // set timestamp
  nlohmann::json time_json;
  time_json["sec"] = timestamp / 1000000000ull;
  time_json["nsec"] = timestamp % 1000000000ull;
  out["timestamp"] = time_json;

  // set message
  auto message_it = out.find("MESSAGE");
  if (message_it != out.end()) {
    out["message"] = message_it.value();
  }

  // set name
  auto name_it = out.find("_SYSTEMD_UNIT");
  if (name_it == out.end()) {
    name_it = out.find("_EXE");
  }
  if (name_it == out.end()) {
    name_it = out.find("_TRANSPORT");
  }
  if (name_it != out.end()) {
    out["name"] = name_it.value();
  }

  // set file
  if (auto file_it = out.find("CODE_FILE"); file_it != out.end()) {
    out["file"] = file_it.value();
  }
  // set line
  if (auto line_it = out.find("CODE_LINE"); line_it != out.end()) {
    out["line"] = std::stoull(std::string(line_it.value()));
  }
  // set level
  if (auto priority_it = out.find("PRIORITY"); priority_it != out.end()) {
    out["level"] = level_for_priority(priority_it.value());
  } else {
    out["level"] = 0;
  }
  return out.dump();
}
