#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <optional>

#include <systemd/sd-journal.h>

struct sd_journal {
    uint64_t usec;
    int rval;
    std::map<std::string, std::string> entries;
    std::optional<std::map<std::string, std::string>::iterator> entry_it;
    char entry_buffer[1024] = {0};
};
