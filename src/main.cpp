#include <sstream>
#include <string_view>

#include <systemd/sd-id128.h>
#include <systemd/sd-journal.h>

#define MCAP_IMPLEMENTATION
#include "mcap/writer.hpp"

#include "json.hpp"

// https://www.freedesktop.org/software/systemd/man/systemd.journal-fields.html
enum Transport {
  TRANSPORT_UNKNOWN,
  TRANSPORT_AUDIT,
  TRANSPORT_DRIVER,
  TRANSPORT_SYSLOG,
  TRANSPORT_JOURNAL,
  TRANSPORT_STDOUT,
  TRANSPORT_KERNEL,
  _TRANSPORT_COUNT,
};

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

Transport transport_for_name(const std::string_view &name) {
  for (int i = 0; i < _TRANSPORT_COUNT; ++i) {
    const char *candidate = name_for_transport((Transport)(i));
    if (name == candidate) {
      return (Transport)(i);
    }
  }
  return TRANSPORT_UNKNOWN;
}

static const char *SCHEMA_TEXT = R"({
  "title": "foxglove.Log",
  "description": "A log message",
  "$comment": "Generated by https://github.com/foxglove/schemas",
  "type": "object",
  "properties": {
    "timestamp": {
      "type": "object",
      "title": "time",
      "properties": {
        "sec": {
          "type": "integer",
          "minimum": 0
        },
        "nsec": {
          "type": "integer",
          "minimum": 0,
          "maximum": 999999999
        }
      },
      "description": "Timestamp of log message"
    },
    "level": {
      "title": "foxglove.LogLevel",
      "description": "Log level",
      "oneOf": [
        {
          "title": "UNKNOWN",
          "const": 0
        },
        {
          "title": "DEBUG",
          "const": 1
        },
        {
          "title": "INFO",
          "const": 2
        },
        {
          "title": "WARNING",
          "const": 3
        },
        {
          "title": "ERROR",
          "const": 4
        },
        {
          "title": "FATAL",
          "const": 5
        }
      ]
    },
    "message": {
      "type": "string",
      "description": "Log message"
    },
    "name": {
      "type": "string",
      "description": "Process or node name"
    },
    "file": {
      "type": "string",
      "description": "Filename"
    },
    "line": {
      "type": "integer",
      "minimum": 0,
      "description": "Line number in the file"
    }
  }
})";

int get_ts(sd_journal *j, uint64_t* out) {
  int err = sd_journal_get_realtime_usec(j, out);
  if (err != 0) {
    return err;
  }
  if (*out >= (UINT64_MAX / 1000ull)) {
    return -EINVAL;
  }
  *out *= 1000ULL;
  return 0;
}

Transport get_transport(sd_journal *j) {
  size_t length = 0;
  const char *data = NULL;
  int ret =
      sd_journal_get_data(j, "_TRANSPORT", (const void **)(&data), &length);
  if (ret != 0) {
    return TRANSPORT_UNKNOWN;
  }
  const size_t key_start = sizeof("_TRANSPORT=") - 1;
  if (length > key_start) {
    return transport_for_name(
        std::string_view(data + key_start, length - key_start));
  }
  return TRANSPORT_UNKNOWN;
}

std::string get_topic(Transport transport) {
  std::stringstream ss;
  ss << "/journald/";
  ss << name_for_transport(transport);
  return ss.str();
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
    out["line"] = line_it.value();
  }
  // set level
  if (auto priority_it = out.find("PRIORITY"); priority_it != out.end()) {
    out["level"] = level_for_priority(priority_it.value());
  } else {
    out["level"] = 0;
  }
  return out.dump();
}

int main(int argc, char **argv) {
  sd_journal *j;
  mcap::McapWriter writer;
  mcap::Message message;
  mcap::Channel channel;

  const char *output_filename = "out.mcap";

  {
    sd_id128_t boot_id;
    int err = sd_id128_get_boot(&boot_id);
    if (err != 0) {
      perror("failed to get boot ID");
      return err;
    }
    err = sd_journal_open(&j, SD_JOURNAL_LOCAL_ONLY);
    if (err != 0) {
      perror("failed to open journal");
      return err;
    }
    char boot_id_match_str[8 + 1 + 32 + 1] = {0};
    snprintf(boot_id_match_str, sizeof(boot_id_match_str),
             "_BOOT_ID=" SD_ID128_FORMAT_STR, SD_ID128_FORMAT_VAL(boot_id));
    err = sd_journal_add_match(j, boot_id_match_str, 0);
    if (err != 0) {
      perror("failed to add boot ID match");
      return err;
    }
    err = sd_journal_seek_head(j);
    if (err != 0) {
      perror("failed to seek to beginning");
      return err;
    }
  }
  {
    auto options = mcap::McapWriterOptions("");
    const auto res = writer.open(output_filename, options);
    if (!res.ok()) {
      fprintf(stderr, "failed to open %s for writing: %s\n", output_filename,
              res.message.c_str());
      return 1;
    }
  }
  // write schema
  mcap::SchemaId schema_id;
  {
    mcap::Schema schema("foxglove.Log", "jsonschema", SCHEMA_TEXT);
    writer.addSchema(schema);
    schema_id = schema.id;
  }

  std::vector<mcap::ChannelId> transport_channel_ids(_TRANSPORT_COUNT, 0);
  std::vector<uint32_t> sequence_counts(_TRANSPORT_COUNT, 0);

  for (size_t i = 0; i < _TRANSPORT_COUNT; ++i) {
    mcap::Channel channel(get_topic((Transport)(i)), "json", schema_id);
    writer.addChannel(channel);
    transport_channel_ids[i] = channel.id;
  }

  while (sd_journal_next(j) > 0) {
    uint64_t ts = 0;
    int err = get_ts(j, &ts);
    if (err != 0) {
      perror("failed to read entry timestamp");
      return err;
    }
    message.logTime = ts;
    message.publishTime = ts;
    Transport transport = get_transport(j);
    message.sequence = sequence_counts[transport];
    sequence_counts[transport]++;
    message.channelId = transport_channel_ids[transport];
    std::string json = serialize_json(j, ts);
    message.data = (const std::byte *)(json.c_str());
    message.dataSize = json.size();
    auto res = writer.write(message);
    if (!res.ok()) {
      fprintf(stderr, "failed to write message: %s\n", res.message.c_str());
      writer.close();
      return 1;
    }
  }
  writer.close();
  sd_journal_close(j);
  return 0;
}
