#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <optional>

#include <systemd/sd-journal.h>
#include <systemd/sd-id128.h>

struct sd_journal {
    int rval;
    std::map<std::string, std::string> fields;
    std::optional<std::map<std::string, std::string>::iterator> field_it;
    char field_buffer[1024] = {0};
    std::vector<uint64_t> entry_timestamps = {100};
    int entry_cursor = 0;
    bool entry_cursor_valid = true;
    std::vector<std::string> matches;
};
