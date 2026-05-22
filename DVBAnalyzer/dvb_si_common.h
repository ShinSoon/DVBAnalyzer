#ifndef DVB_SI_COMMON_H
#define DVB_SI_COMMON_H

#include <string>
#include <vector>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>

#include "dvb_time.h" // UTC-correct time helpers (shared with the binary path)

// Structure to hold basic transport stream identification
struct TransportStreamId {
    uint16_t transport_stream_id = 0;
    uint16_t original_network_id = 0;

    // Needed for using as a map key
    bool operator<(const TransportStreamId& other) const {
        if (original_network_id != other.original_network_id) {
            return original_network_id < other.original_network_id;
        }
        return transport_stream_id < other.transport_stream_id;
    }
};

// Parse "YYYY-MM-DD HH:MM:SS" (interpreted as UTC, like DVB) to epoch millis.
// NOTE: this used to call mktime()/localtime(), which silently shifted every
// timestamp by the machine's timezone. It now goes through makeUtcEpochMillis()
// so the text path and the binary (MJD/BCD) path produce identical epochs.
inline long long parseDateTimeString(const std::string& dateTimeStr) {
    std::tm t{};
    std::istringstream ss(dateTimeStr);
    ss >> std::get_time(&t, "%Y-%m-%d %H:%M:%S");
    if (ss.fail()) {
        std::cerr << "Error parsing date/time string: " << dateTimeStr << std::endl;
        return 0;
    }
    return makeUtcEpochMillis(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                              t.tm_hour, t.tm_min, t.tm_sec);
}

// Format epoch milliseconds as a readable UTC string.
inline std::string formatTime(long long timeMillis) {
    return formatUtcTime(timeMillis);
}


#endif // DVB_SI_COMMON_H