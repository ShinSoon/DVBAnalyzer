#ifndef DVB_EIT_H
#define DVB_EIT_H

#include "dvb_si_common.h"

// Represents information about a single event (program) from the EIT
struct EventInfo {
    uint16_t event_id = 0;
    long long start_time_millis = 0; // UTC milliseconds since epoch
    long long duration_seconds = 0;

    // --- From Short Event Descriptor (example) ---
    std::string event_name;
    std::string short_description; // Text description

    void print() const;
    long long getEndTimeMillis() const { return start_time_millis + duration_seconds * 1000; }
};

#endif // DVB_EIT_H