#include "vendor/json.hpp"


#include <systemd/sd-id128.h>

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

int match_current_entry_boot_id(sd_journal *j) {
  const void* data = NULL;
  size_t length = 0;
  int err = sd_journal_get_data(j, "_BOOT_ID", &data, &length);
  if (err != 0) {
    return err;
  }
  return sd_journal_add_match(j, data, length);
}

int apply_boot_id_match(sd_journal *j, const Options &options) {
  int err = 0;
  if (options.start == TIME_BOOT) {
    if (options.end == TIME_UNIX) {
      err = sd_journal_seek_realtime_usec(j, options.end_sec * 1'000'000);
      if (err != 0) {
        return err;
      }
      err = sd_journal_previous(j);
      if (err < 0) {
        return err;
      }
      if (err == 0) {
        // there are no entries before end_sec, which is awkward. it's safe to return 0 here,
        // the subsequent seek to head and read forward will return no records before end_sec
        // and the resulting MCAP will contain no entries.
        return 0;
      }
      return match_current_entry_boot_id(j);
      // select boot ID based on entry closest to end
    } else {
      sd_id128_t boot_id;
      err = sd_id128_get_boot(&boot_id);
      if (err != 0) {
        return err;
      }
      // use a buffer big enough to fit a `_BOOT_ID=a0b1...` hex-encoded string. 
      char boot_id_match[sizeof("_BOOT_ID=") + (sizeof(sd_id128_t) * 2)] = {0};
      size_t len_written = snprintf(boot_id_match, sizeof(boot_id_match), "_BOOT_ID=" SD_ID128_FORMAT_STR, SD_ID128_FORMAT_VAL(boot_id));
      assert(len_written < sizeof(boot_id_match));
      return sd_journal_add_match(j, (const void*)boot_id_match, len_written);
    }
  } else if (options.end == TIME_SHUTDOWN && options.start == TIME_UNIX) {
    // select boot ID based on entry closest to start
    err = sd_journal_seek_realtime_usec(j, options.start_sec * 1'000'000);
    if (err != 0) {
      return err;
    }
    err = sd_journal_next(j);
    if (err < 0) {
      return err;
    }
    if (err == 0) {
      // there are no entries after start_sec. this means the start time is in the current boot
      // and recording will end before the next boot, so no need to apply a boot ID filter.
      return 0;
    }
    return match_current_entry_boot_id(j);
    // select boot ID based on entry closest to end
  }
  // in all other cases, no need to filter by boot ID.
  return 0;
}

int seek_to_start(sd_journal *j, TimePoint start, uint64_t start_sec) {
  switch (start) {
    case TIME_BOOT:
      return sd_journal_seek_head(j);
    case TIME_NOW:
      return sd_journal_seek_tail(j);
    case TIME_UNIX:
      return sd_journal_seek_realtime_usec(j, start_sec * 1'000'000);
    default:
      assert(false); // no other valid start time points
  }
  return 0;
}

int next_journal_entry(sd_journal *j, TimePoint end, uint64_t end_sec, std::atomic_bool* signalled) {
  int err = 0;
  switch (end) {
    case TIME_SHUTDOWN:
    case TIME_NOW:
      return sd_journal_next(j);
    case TIME_WAIT:
      // wait for the next entry
      while (!*signalled) {
        err = sd_journal_next(j);
        if (err != 0) {
          return err;
        }
        // there are no more entries in the journal right now, wait for more.
        err = sd_journal_wait(j, 1'000'000);
        // negative return indicates an error, bail out.
        if (err < 0) {
          return err;
        }
        // APPEND and INVALIDATE both indicate that new entries are available.
        if (err == SD_JOURNAL_APPEND || err == SD_JOURNAL_INVALIDATE) {
          return 1;
        }
        // NOP should be the only other possible return code. try waiting again.
        assert(err == SD_JOURNAL_NOP);
      }
    case TIME_UNIX:
    {
      err = sd_journal_next(j);
      // if there are no more entries or an error occurred, return.
      if (err <= 0) {
        return err;
      }
      uint64_t ts_usec = 0; 
      err = sd_journal_get_realtime_usec(j, &ts_usec);
      if (err != 0) {
        return err;
      }
      // if this entry is after nend sec, return "no more entries"
      if (ts_usec > (end_sec * 1'000'000)) {
        return 0;
      }
      // otherwise, return "more entries".
      return 1;
    }
    default:
      assert(false); // no other valid time points for end
  }
  return 0;
}
