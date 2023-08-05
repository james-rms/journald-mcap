#ifndef JOURNAL_HPP
#define JOURNAL_HPP
#include <string>
#include <atomic>

#include <systemd/sd-journal.h>

#include "cmdline.hpp"

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

const char *name_for_transport(Transport transport);

Transport get_transport(sd_journal *j);

int get_ts(sd_journal *j, uint64_t *out);


/**
 * @brief maps systemd priorities to foxglove.Log levels.
 * https://wiki.archlinux.org/title/Systemd/Journal#Priority_level
 * https://github.com/foxglove/schemas/blob/main/schemas/jsonschema/Log.json
 */
int level_for_priority(const std::string &priority);

std::string serialize_json(sd_journal *j, uint64_t timestamp);

int apply_boot_id_match(sd_journal *j, const Options& options);

int seek_to_start(sd_journal *j, TimePoint start, uint64_t start_secs);

int next_journal_entry(sd_journal* j, TimePoint end, uint64_t end_secs);

#endif
