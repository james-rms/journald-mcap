#include "fake_systemd.hpp"
#include <cstdio>
#include <algorithm>

int sd_journal_get_data(sd_journal *j, const char *key, const void **data,
                        size_t *length) {
  if (!j->entry_cursor_valid) {
   fprintf(stderr, "attempted to access entry with invalid cursor\n");
   return -EADDRNOTAVAIL;
  }
  auto it = j->fields.find(key);
  if (it == j->fields.end()) {
    return -ENOENT;
  }
  *length = snprintf(j->field_buffer, sizeof(j->field_buffer), "%s=%s",
                     it->first.c_str(), it->second.c_str());
  *data = (const void *)(j->field_buffer);
  return j->rval;
}

int sd_journal_get_realtime_usec(sd_journal *j, uint64_t *out) {
  if (!j->entry_cursor_valid) {
   fprintf(stderr, "attempted to access entry with invalid cursor\n");
   return -EADDRNOTAVAIL;
  }
  *out = j->entry_timestamps[j->entry_cursor];
  return j->rval;
}
void sd_journal_restart_data(sd_journal *j) {
  j->field_it = j->fields.begin();
  return;
}

int sd_journal_enumerate_available_data(sd_journal *j, const void **data,
                                        size_t *length) {
  if (!j->entry_cursor_valid) {
   fprintf(stderr, "attempted to access entry with invalid cursor\n");
   return -EADDRNOTAVAIL;
  }
  if (j->field_it == std::nullopt) {
    sd_journal_restart_data(j);
  }
  if (*(j->field_it) == j->fields.end()) {
    return 0;
  }
  auto [key, val] = **(j->field_it);
  *length = snprintf(j->field_buffer, sizeof(j->field_buffer), "%s=%s",
                     key.c_str(), val.c_str());
  *data = (const void *)(j->field_buffer);
  (*(j->field_it))++;
  if (j->rval < 0) {
    return j->rval;
  }
  return 1;
}

int sd_journal_seek_head(sd_journal *j) {
  j->entry_cursor_valid = false;
  j->entry_cursor = 0;
  return 0;
}

int sd_journal_seek_tail(sd_journal *j) {
  j->entry_cursor_valid = false;
  j->entry_cursor = j->entry_timestamps.size() - 1;
  return 0;
}

int sd_journal_seek_realtime_usec(sd_journal *j, uint64_t usec) {
  j->entry_cursor_valid = false;
  for (j->entry_cursor = 0; j->entry_cursor < (int)(j->entry_timestamps.size()); j->entry_cursor++) {
    if (j->entry_timestamps[j->entry_cursor] < usec) {
      return 0;
    }
  }
  return 0;
}

int sd_journal_next(sd_journal *j) {
  if (!j->entry_cursor_valid) {
    j->entry_cursor_valid = true;
  } else {
    j->entry_cursor++;
  }
  if (j->entry_cursor < 0 || j->entry_cursor >= (int)(j->entry_timestamps.size())) {
    j->entry_cursor_valid = false;
    return 0;
  }
  return 1;
}

int sd_journal_previous(sd_journal *j) {
  if (!j->entry_cursor_valid) {
    j->entry_cursor_valid = true;
  } else {
    j->entry_cursor--;
  }
  if (j->entry_cursor < 0 || j->entry_cursor >= (int)(j->entry_timestamps.size())) {
    j->entry_cursor_valid = false;
    return 0;
  }
  return 1;
}

int sd_journal_add_match(sd_journal *j, const void* data, size_t len) {
  j->matches.push_back(std::string((const char*)(data), len));
  return 0;
}


int sd_id128_get_boot(sd_id128_t * out) {
  *out = SD_ID128_MAKE(00,01, 02, 03, 04, 05, 06, 07, 08, 09, 0a, 0b, 0c, 0d, 0e, 0f);
  return 0;
}
