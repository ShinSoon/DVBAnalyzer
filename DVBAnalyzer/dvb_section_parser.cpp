#include "dvb_section_parser.h"

#include "dvb_crc32.h"
#include "dvb_descriptors.h"
#include "dvb_time.h"

namespace {

// Validate the common section framing and return the authoritative total size
// (3 + section_length). Returns 0 if the section is too short or the CRC fails.
size_t validatedSectionSize(const std::vector<uint8_t>& section) {
    if (section.size() < 12) return 0;  // smallest meaningful SI section
    const size_t section_length = ((section[1] & 0x0F) << 8) | section[2];
    const size_t total = 3 + section_length;
    if (total > section.size()) return 0;
    if (!dvbSectionCrcOk(section.data(), total)) return 0;
    return total;
}

uint16_t be16(const uint8_t* p) { return static_cast<uint16_t>((p[0] << 8) | p[1]); }

}  // namespace

bool parseSdtSection(const std::vector<uint8_t>& section,
                     TransportStreamId& tsid,
                     std::vector<ServiceInfo>& services) {
    const size_t total = validatedSectionSize(section);
    if (total == 0) return false;

    const uint8_t* s = section.data();
    const uint8_t table_id = s[0];
    if (table_id != kTableIdSdtActual && table_id != kTableIdSdtOther) return false;

    tsid.transport_stream_id = be16(s + 3);
    // s[5] version/current_next, s[6] section_number, s[7] last_section_number
    tsid.original_network_id = be16(s + 8);
    // s[10] reserved_future_use

    const size_t loopEnd = total - 4;  // stop before the 4 CRC bytes
    size_t pos = 11;
    while (pos + 5 <= loopEnd) {
        ServiceInfo svc;
        svc.service_id = be16(s + pos);
        // s[pos+2]: reserved(6), EIT_schedule_flag(1), EIT_present_following_flag(1)
        svc.eit_schedule_flag = (s[pos + 2] & 0x02) != 0;
        svc.eit_present_following_flag = (s[pos + 2] & 0x01) != 0;
        // s[pos+3..4]: running_status(3), free_CA_mode(1), descriptors_loop_length(12)
        svc.running_status = (s[pos + 3] >> 5) & 0x07;
        svc.free_ca_mode = ((s[pos + 3] >> 4) & 0x01) != 0;
        const size_t dll = ((s[pos + 3] & 0x0F) << 8) | s[pos + 4];

        const size_t descStart = pos + 5;
        if (descStart + dll > loopEnd) break;  // malformed loop length
        applyServiceDescriptors(s + descStart, dll, svc);

        services.push_back(svc);
        pos = descStart + dll;
    }
    return true;
}

bool parseEitSection(const std::vector<uint8_t>& section,
                     TransportStreamId& tsid,
                     uint16_t& serviceId,
                     std::vector<EventInfo>& events,
                     bool& isPresentFollowing) {
    const size_t total = validatedSectionSize(section);
    if (total == 0) return false;

    const uint8_t* s = section.data();
    const uint8_t table_id = s[0];
    if (!isEitPresentFollowing(table_id) && !isEitSchedule(table_id)) return false;
    isPresentFollowing = isEitPresentFollowing(table_id);

    serviceId = be16(s + 3);  // EIT carries service_id where PSI carries the table id extension
    // s[5] version/current_next, s[6] section_number, s[7] last_section_number
    tsid.transport_stream_id = be16(s + 8);
    tsid.original_network_id = be16(s + 10);
    // s[12] segment_last_section_number, s[13] last_table_id

    const size_t loopEnd = total - 4;
    size_t pos = 14;
    while (pos + 12 <= loopEnd) {
        EventInfo evt;
        evt.event_id = be16(s + pos);
        evt.start_time_millis = mjdBcdToEpochMillis(s + pos + 2);   // 5 bytes MJD+BCD
        evt.duration_seconds = bcdDurationToSeconds(s + pos + 7);   // 3 bytes BCD
        // s[pos+10..11]: running_status(3), free_CA_mode(1), descriptors_loop_length(12)
        const size_t dll = ((s[pos + 10] & 0x0F) << 8) | s[pos + 11];

        const size_t descStart = pos + 12;
        if (descStart + dll > loopEnd) break;
        applyEventDescriptors(s + descStart, dll, evt);

        events.push_back(evt);
        pos = descStart + dll;
    }
    return true;
}
