#include <string>

#include <systemd/sd-journal.h>

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
