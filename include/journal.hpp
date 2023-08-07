#ifndef JOURNAL_HPP
#define JOURNAL_HPP
#include <string>
#include <atomic>

#include <systemd/sd-journal.h>

#include "cmdline.hpp"

/**
 * @brief an enumeration of journal transport values used by journald. See
 * https://www.freedesktop.org/software/systemd/man/systemd.journal-fields.html
 * for a complete description of each.
 */
enum Transport {
  TRANSPORT_UNKNOWN, // Used if the _transport string in the entry is not recognized.
  TRANSPORT_AUDIT,
  TRANSPORT_DRIVER,
  TRANSPORT_SYSLOG,
  TRANSPORT_JOURNAL,
  TRANSPORT_STDOUT,
  TRANSPORT_KERNEL,
  _TRANSPORT_COUNT,
};

/**
 * @brief provides the name for a Transport as a static C string.
 */
const char *name_for_transport(Transport transport);

/**
 * @brief parses the Transport used by the current entry in the journal.
 */
Transport get_transport(sd_journal *j);

/**
 * @brief Get the real-time timestamp of the current journal entry as a count of nanoseconds since
 * the epoch.
 */
int get_ts(sd_journal *j, uint64_t *out);


/**
 * @brief maps systemd priorities to foxglove.Log levels.
 * https://wiki.archlinux.org/title/Systemd/Journal#Priority_level
 * https://github.com/foxglove/schemas/blob/main/schemas/jsonschema/Log.json
 */
int level_for_priority(const std::string &priority);

/**
 * @brief Serializes the current journal entry as a JSON `foxglove.Log`.
 */
std::string serialize_json(sd_journal *j, uint64_t timestamp);

/**
 * @brief filters the entries in the journal as neccessary given the Options provided by the user.
 */
int apply_boot_id_match(sd_journal *j, const Options& options);

/** Seeks to the start of the journal as specified by `start` and `start_secs`.
*/
int seek_to_start(sd_journal *j, TimePoint start, uint64_t start_secs);

/** Moves the journal cursor to the next journal entry.
 *
 * @returns a negative errno-style value if an error occurred.
 * @returns 0 when there are no more entries to read.
 * @returns 1 when the cursor points to a new valid entry.
*/
int next_journal_entry(sd_journal* j, TimePoint end, uint64_t end_secs);

#endif
