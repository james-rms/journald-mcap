#include <stdint.h>
#include <systemd/sd-journal.h>


#define MCAP_IMPLEMENTATION
#include "mcap/writer.hpp"

namespace journal2mcap
{
enum class Error {
    SUCCESS = 0,
    CANNOT_DETERMINE_TOPIC = 1,
    TIME_VALUE_TOO_LARGE = 2,
    CANNOT_DETERMINE_PID = 3,
    CANNOT_DETERMINE_TOPIC = 4,
};

void print_error(Error err, const std::string& failure);

/**
 * @brief Get the nanosecond timestamp for the current journal entry
 */
Error get_ts(sd_journal* j, uint64_t* out);

/**
 * @brief determine the topic for this journal entry in the MCAP file.
 */
Error get_topic_for_entry(sd_journal *j, std::string& out);

/**
 * @brief returns true if this message originated from the kernel, rather than a userspace process.
 */
Error is_kernel_entry(sd_journal *j, std::string& out);

/**
 * @brief writes the journal entry process PID into `pid`, if the entry contains a PID.
 */
Error get_entry_pid(sd_journal *j, int32_t* pid);

bool find_channel_for_entry(ChannelState* channel_state, sd_journal* j, uint16_t* out);

Error set_channel_for_entry(ChannelState* channel_state, sd_journal* j, mcap::Channel* out);

Error set_message_for_entry(sd_journal* j, const ChannelState& channel_state, mcap::Message* out);

}
