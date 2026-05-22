#ifndef DVB_SI_PARSER_H
#define DVB_SI_PARSER_H

#include <string>
#include <vector>
#include <map>
#include "dvb_si_common.h"
#include "dvb_sdt.h"
#include "dvb_eit.h"

class DvbSiParser {
public:
    DvbSiParser() = default;

    // Parses simulated SI data from the pipe-delimited text format.
    bool parseDataFromFile(const std::string& filename);

    // Parses real binary DVB SI from an MPEG-2 Transport Stream file:
    // demuxes PID 0x11 (SDT) and 0x12 (EIT), reassembles sections, validates
    // CRC-32, and decodes the SDT/EIT tables into the same internal stores.
    bool parseDataFromTsFile(const std::string& filename);

    // --- Enumeration ---
    // List every Transport Stream we have data for (from SDT or EIT), sorted.
    std::vector<TransportStreamId> getTransportStreams() const;

    // --- SDT Related ---
    // Get info for all services found in a specific Transport Stream
    std::vector<ServiceInfo> getServices(const TransportStreamId& tsid) const;
    // Get info for a specific service
    bool getServiceInfo(const TransportStreamId& tsid, uint16_t serviceId, ServiceInfo& outInfo) const;

    // --- EIT Related ---
    // Get Present/Following events for a specific service
    std::vector<EventInfo> getPresentFollowingEvents(const TransportStreamId& tsid, uint16_t serviceId) const;
    // Get scheduled events for a specific service
    std::vector<EventInfo> getScheduledEvents(const TransportStreamId& tsid, uint16_t serviceId) const;
    // Get events for a service within a specific time range
    std::vector<EventInfo> getEventsForTimeRange(const TransportStreamId& tsid, uint16_t serviceId, long long rangeStartMillis, long long rangeEndMillis) const;


private:
    // Internal storage
    // Map: TS Key -> Vector of Services in that TS
    std::map<TransportStreamId, std::vector<ServiceInfo>> services_by_ts_;

    // Map: TS Key -> Map: Service ID -> Vector of Events for that Service
    // We store Present/Following and Schedule separately as they come from different EIT tables
    std::map<TransportStreamId, std::map<uint16_t, std::vector<EventInfo>>> events_pf_by_service_;
    std::map<TransportStreamId, std::map<uint16_t, std::vector<EventInfo>>> events_schedule_by_service_;

    // Helper to get or create the event vector for a specific service
    std::vector<EventInfo>& getEventVector(std::map<TransportStreamId, std::map<uint16_t, std::vector<EventInfo>>>& eventMap,
        const TransportStreamId& tsid, uint16_t serviceId);

    // Sort every stored event list by start time (shared by both ingest paths).
    void sortAllEvents();
};

#endif // DVB_SI_PARSER_H