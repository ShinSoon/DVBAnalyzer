#ifndef DVB_SDT_H
#define DVB_SDT_H

#include "dvb_si_common.h"

// Represents information about a single service (channel) from the SDT
struct ServiceInfo {
    uint16_t service_id = 0;          // Unique ID for the service within the TS
    bool eit_schedule_flag = false;
    bool eit_present_following_flag = false;
    uint8_t running_status = 0;
    bool free_ca_mode = false;       // True if not scrambled

    // --- From Service Descriptor (example) ---
    uint8_t service_type = 0;         // e.g., 0x01=Digital TV, 0x02=Digital Radio
    std::string provider_name;
    std::string service_name;

    void print() const;
};

#endif // DVB_SDT_H