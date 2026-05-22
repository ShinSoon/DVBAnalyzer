#ifndef DVB_SECTION_PARSER_H
#define DVB_SECTION_PARSER_H

#include <cstdint>
#include <vector>

#include "dvb_si_common.h"  // TransportStreamId
#include "dvb_sdt.h"        // ServiceInfo
#include "dvb_eit.h"        // EventInfo

// Well-known PIDs and table_ids (ETSI EN 300 468 / ISO 13818-1).
constexpr uint16_t kPidSdt = 0x0011;  // SDT and BAT share this PID
constexpr uint16_t kPidEit = 0x0012;  // EIT

constexpr uint8_t kTableIdSdtActual = 0x42;  // SDT, this transport stream
constexpr uint8_t kTableIdSdtOther  = 0x46;  // SDT, other transport stream

// EIT present/following and schedule sub-tables span ranges of table_ids.
inline bool isEitPresentFollowing(uint8_t tableId) { return tableId == 0x4E || tableId == 0x4F; }
inline bool isEitSchedule(uint8_t tableId)         { return tableId >= 0x50 && tableId <= 0x6F; }

// Parse one assembled SDT section. Validates CRC, fills tsid and the service
// list (including service_descriptor fields). Returns false on a bad section.
bool parseSdtSection(const std::vector<uint8_t>& section,
                     TransportStreamId& tsid,
                     std::vector<ServiceInfo>& services);

// Parse one assembled EIT section. Validates CRC, fills tsid/serviceId and the
// event list (including short_event_descriptor fields and MJD/BCD times).
// isPresentFollowing tells the caller which table the events belong to.
bool parseEitSection(const std::vector<uint8_t>& section,
                     TransportStreamId& tsid,
                     uint16_t& serviceId,
                     std::vector<EventInfo>& events,
                     bool& isPresentFollowing);

#endif // DVB_SECTION_PARSER_H
