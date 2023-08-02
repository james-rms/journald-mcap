#include "fake_systemd.hpp"
#include <cstdio>

int sd_journal_get_data(sd_journal *j, const char *key, const void **data,
                        size_t *length) {
  auto it = j->entries.find(key);
  if (it == j->entries.end()) {
    return -ENOENT;
  }
  *length = snprintf(j->entry_buffer, sizeof(j->entry_buffer), "%s=%s",
                     it->first.c_str(), it->second.c_str());
  *data = (const void *)(j->entry_buffer);
  return j->rval;
}

int sd_journal_get_realtime_usec(sd_journal *j, uint64_t *out) {
  *out = j->usec;
  return j->rval;
}
void sd_journal_restart_data(sd_journal *j) {
  j->entry_it = j->entries.begin();
  return;
}

int sd_journal_enumerate_available_data(sd_journal *j, const void **data,
                                        size_t *length) {
  if (j->entry_it == std::nullopt) {
    sd_journal_restart_data(j);
  }
  if (*(j->entry_it) == j->entries.end()) {
    return 0;
  }
  auto [key, val] = **(j->entry_it);
  *length = snprintf(j->entry_buffer, sizeof(j->entry_buffer), "%s=%s",
                     key.c_str(), val.c_str());
  *data = (const void *)(j->entry_buffer);
  (*(j->entry_it))++;
  if (j->rval < 0) {
    return j->rval;
  }
  return 1;
}
