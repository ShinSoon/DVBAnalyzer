#include "dvb_si_parser.h"
#include "ts_demux.h"
#include "dvb_section_parser.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <algorithm> // For std::sort
#include <set>       // For getTransportStreams()

// Helper implementation
std::vector<EventInfo>& DvbSiParser::getEventVector(
    std::map<TransportStreamId, std::map<uint16_t, std::vector<EventInfo>>>& eventMap,
    const TransportStreamId& tsid, uint16_t serviceId)
{
    // Find or create the map for the TS
    auto& serviceMap = eventMap[tsid]; // Creates entry if tsid doesn't exist
    // Find or create the vector for the service
    return serviceMap[serviceId]; // Creates entry if serviceId doesn't exist
}


bool DvbSiParser::parseDataFromFile(const std::string& filename) {
    services_by_ts_.clear();
    events_pf_by_service_.clear();
    events_schedule_by_service_.clear();

    std::cout << "Attempting to parse DVB SI data from file: \"" << filename << "\"" << std::endl;

    std::ifstream inputFile(filename);
    if (!inputFile.is_open()) {
        std::cerr << "Error: Could not open file: " << filename << std::endl;
        return false;
    }

    std::string line;
    int lineNumber = 0;

    while (std::getline(inputFile, line)) {
        lineNumber++;
        if (line.empty() || line[0] == '#') continue; // Skip empty lines and comments

        std::stringstream ssLine(line);
        std::string recordType;
        std::string part;

        if (!std::getline(ssLine, recordType, '|')) {
            std::cerr << "Warning (Line " << lineNumber << "): Malformed record (missing type): \"" << line << "\"" << std::endl;
            continue;
        }

        try {
            if (recordType == "SDT") { // Service Description Table entry
                // Format: SDT|ONID|TSID|ServiceID|EIT_S_flag|EIT_PF_flag|RunStatus|FreeCA|Type|ProviderName|ServiceName
                std::string onidStr, tsidStr, serviceIdStr, eitSFlagStr, eitPfFlagStr, runStatusStr, freeCaStr, typeStr, providerName, serviceName;
                int partsRead = 0;

                if (std::getline(ssLine, onidStr, '|')) partsRead++;
                if (std::getline(ssLine, tsidStr, '|')) partsRead++;
                if (std::getline(ssLine, serviceIdStr, '|')) partsRead++;
                if (std::getline(ssLine, eitSFlagStr, '|')) partsRead++;
                if (std::getline(ssLine, eitPfFlagStr, '|')) partsRead++;
                if (std::getline(ssLine, runStatusStr, '|')) partsRead++;
                if (std::getline(ssLine, freeCaStr, '|')) partsRead++;
                if (std::getline(ssLine, typeStr, '|')) partsRead++;
                if (std::getline(ssLine, providerName, '|')) partsRead++;
                if (std::getline(ssLine, serviceName)) partsRead++; // Read till end of line

                if (partsRead >= 10) { // Need at least 10 parts
                    TransportStreamId tsid;
                    tsid.original_network_id = static_cast<uint16_t>(std::stoul(onidStr));
                    tsid.transport_stream_id = static_cast<uint16_t>(std::stoul(tsidStr));

                    ServiceInfo service;
                    service.service_id = static_cast<uint16_t>(std::stoul(serviceIdStr));
                    service.eit_schedule_flag = (std::stoi(eitSFlagStr) != 0);
                    service.eit_present_following_flag = (std::stoi(eitPfFlagStr) != 0);
                    service.running_status = static_cast<uint8_t>(std::stoul(runStatusStr));
                    service.free_ca_mode = (std::stoi(freeCaStr) != 0);
                    service.service_type = static_cast<uint8_t>(std::stoul(typeStr));
                    service.provider_name = providerName;
                    service.service_name = serviceName;

                    services_by_ts_[tsid].push_back(service);
                }
                else {
                    std::cerr << "Warning (Line " << lineNumber << "): Incomplete SDT record (expected 10+ parts, got " << partsRead << "): \"" << line << "\"." << std::endl;
                }

            }
            else if (recordType == "EIT") { // Event Information Table entry
                // Format: EIT|ONID|TSID|ServiceID|EventID|StartTime|DurationSec|EventName|EventDesc|Type(PF/SCHED)
                std::string onidStr, tsidStr, serviceIdStr, eventIdStr, startTimeStr, durationStr, eventName, eventDesc, typeStr;
                int partsRead = 0;

                if (std::getline(ssLine, onidStr, '|')) partsRead++;
                if (std::getline(ssLine, tsidStr, '|')) partsRead++;
                if (std::getline(ssLine, serviceIdStr, '|')) partsRead++;
                if (std::getline(ssLine, eventIdStr, '|')) partsRead++;
                if (std::getline(ssLine, startTimeStr, '|')) partsRead++;
                if (std::getline(ssLine, durationStr, '|')) partsRead++;
                if (std::getline(ssLine, eventName, '|')) partsRead++;
                if (std::getline(ssLine, eventDesc, '|')) partsRead++; // Read potentially empty description
                if (std::getline(ssLine, typeStr)) partsRead++; // Read type at the end

                if (partsRead >= 9) { // Need at least 9 parts
                    TransportStreamId tsid;
                    tsid.original_network_id = static_cast<uint16_t>(std::stoul(onidStr));
                    tsid.transport_stream_id = static_cast<uint16_t>(std::stoul(tsidStr));
                    uint16_t serviceId = static_cast<uint16_t>(std::stoul(serviceIdStr));

                    EventInfo event;
                    event.event_id = static_cast<uint16_t>(std::stoul(eventIdStr));
                    event.start_time_millis = parseDateTimeString(startTimeStr);
                    event.duration_seconds = std::stoll(durationStr);
                    event.event_name = eventName;
                    event.short_description = eventDesc;

                    if (event.start_time_millis > 0 && event.duration_seconds >= 0) {
                        if (typeStr == "PF") {
                            getEventVector(events_pf_by_service_, tsid, serviceId).push_back(event);
                        }
                        else if (typeStr == "SCHED") {
                            getEventVector(events_schedule_by_service_, tsid, serviceId).push_back(event);
                        }
                        else {
                            std::cerr << "Warning (Line " << lineNumber << "): Unknown EIT type '" << typeStr << "'. Skipping." << std::endl;
                        }
                    }
                    else {
                        std::cerr << "Warning (Line " << lineNumber << "): Invalid time/duration for EIT record (Start: " << startTimeStr << ", Duration: " << durationStr << "). Event not added." << std::endl;
                    }
                }
                else {
                    std::cerr << "Warning (Line " << lineNumber << "): Incomplete EIT record (expected 9+ parts, got " << partsRead << "): \"" << line << "\"." << std::endl;
                }
            }
            else {
                std::cerr << "Warning (Line " << lineNumber << "): Unknown record type \"" << recordType << "\". Skipping." << std::endl;
            }
        }
        catch (const std::invalid_argument& e) {
            std::cerr << "Error (Line " << lineNumber << "): Parsing number failed: " << e.what() << ". Record: \"" << line << "\"" << std::endl;
        }
        catch (const std::out_of_range& e) {
            std::cerr << "Error (Line " << lineNumber << "): Number out of range: " << e.what() << ". Record: \"" << line << "\"" << std::endl;
        }
        catch (const std::exception& e) { // Catch other potential exceptions
            std::cerr << "Error (Line " << lineNumber << "): Unexpected exception: " << e.what() << ". Record: \"" << line << "\"" << std::endl;
        }
    } // end while getline

    sortAllEvents();

    inputFile.close();
    std::cout << "DVB SI Parsing complete. Found services for " << services_by_ts_.size() << " transport streams." << std::endl;
    return true;
}

void DvbSiParser::sortAllEvents() {
    auto sortByStart = [](std::map<TransportStreamId, std::map<uint16_t, std::vector<EventInfo>>>& eventMap) {
        for (auto& ts_pair : eventMap) {
            for (auto& service_pair : ts_pair.second) {
                std::sort(service_pair.second.begin(), service_pair.second.end(),
                    [](const EventInfo& a, const EventInfo& b) {
                        return a.start_time_millis < b.start_time_millis;
                    });
            }
        }
    };
    sortByStart(events_pf_by_service_);
    sortByStart(events_schedule_by_service_);
}

bool DvbSiParser::parseDataFromTsFile(const std::string& filename) {
    services_by_ts_.clear();
    events_pf_by_service_.clear();
    events_schedule_by_service_.clear();

    std::cout << "Attempting to parse binary DVB transport stream: \"" << filename << "\"" << std::endl;

    std::ifstream inputFile(filename, std::ios::binary);
    if (!inputFile.is_open()) {
        std::cerr << "Error: Could not open file: " << filename << std::endl;
        return false;
    }
    std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(inputFile)),
                                std::istreambuf_iterator<char>());
    inputFile.close();

    if (buffer.size() < kTsPacketSize) {
        std::cerr << "Error: file is smaller than one TS packet." << std::endl;
        return false;
    }

    // Demux the SI PIDs and reassemble their sections.
    TsDemux demux;
    demux.addPid(kPidSdt);
    demux.addPid(kPidEit);
    auto sections = demux.demux(buffer.data(), buffer.size());

    size_t sdtOk = 0, sdtBad = 0, eitOk = 0, eitBad = 0;

    // --- SDT sections (PID 0x11) ---
    for (const auto& section : sections[kPidSdt]) {
        TransportStreamId tsid;
        std::vector<ServiceInfo> services;
        if (parseSdtSection(section, tsid, services)) {
            auto& dest = services_by_ts_[tsid];
            for (const auto& svc : services) dest.push_back(svc);
            ++sdtOk;
        } else {
            ++sdtBad;
        }
    }

    // --- EIT sections (PID 0x12) ---
    for (const auto& section : sections[kPidEit]) {
        TransportStreamId tsid;
        uint16_t serviceId = 0;
        std::vector<EventInfo> events;
        bool isPf = false;
        if (parseEitSection(section, tsid, serviceId, events, isPf)) {
            auto& dest = isPf ? events_pf_by_service_ : events_schedule_by_service_;
            auto& vec = getEventVector(dest, tsid, serviceId);
            for (const auto& evt : events) vec.push_back(evt);
            ++eitOk;
        } else {
            ++eitBad;
        }
    }

    sortAllEvents();

    std::cout << "Demux complete: " << sdtOk << " SDT and " << eitOk
              << " EIT sections parsed (CRC-validated)";
    if (sdtBad || eitBad) {
        std::cout << "; dropped " << (sdtBad + eitBad) << " bad/unknown section(s)";
    }
    std::cout << "." << std::endl;
    std::cout << "Found services for " << services_by_ts_.size() << " transport streams." << std::endl;
    return true;
}

std::vector<TransportStreamId> DvbSiParser::getTransportStreams() const {
    // Union the keys of all three stores so a TS is listed even if it only
    // carried EIT (events) without an SDT, or vice versa. std::set keeps them
    // unique and ordered via TransportStreamId::operator<.
    std::set<TransportStreamId> unique;
    for (const auto& pair : services_by_ts_) unique.insert(pair.first);
    for (const auto& pair : events_pf_by_service_) unique.insert(pair.first);
    for (const auto& pair : events_schedule_by_service_) unique.insert(pair.first);
    return std::vector<TransportStreamId>(unique.begin(), unique.end());
}

std::vector<ServiceInfo> DvbSiParser::getServices(const TransportStreamId& tsid) const {
    auto it = services_by_ts_.find(tsid);
    if (it != services_by_ts_.end()) {
        return it->second;
    }
    return {}; // Return empty vector if TS not found
}

bool DvbSiParser::getServiceInfo(const TransportStreamId& tsid, uint16_t serviceId, ServiceInfo& outInfo) const {
    auto it = services_by_ts_.find(tsid);
    if (it != services_by_ts_.end()) {
        for (const auto& service : it->second) {
            if (service.service_id == serviceId) {
                outInfo = service;
                return true;
            }
        }
    }
    return false; // Not found
}


std::vector<EventInfo> DvbSiParser::getPresentFollowingEvents(const TransportStreamId& tsid, uint16_t serviceId) const {
    auto ts_it = events_pf_by_service_.find(tsid);
    if (ts_it != events_pf_by_service_.end()) {
        auto service_it = ts_it->second.find(serviceId);
        if (service_it != ts_it->second.end()) {
            return service_it->second; // Return the vector of P/F events
        }
    }
    return {}; // Return empty if TS or Service not found in P/F map
}

std::vector<EventInfo> DvbSiParser::getScheduledEvents(const TransportStreamId& tsid, uint16_t serviceId) const {
    auto ts_it = events_schedule_by_service_.find(tsid);
    if (ts_it != events_schedule_by_service_.end()) {
        auto service_it = ts_it->second.find(serviceId);
        if (service_it != ts_it->second.end()) {
            return service_it->second; // Return the vector of scheduled events
        }
    }
    return {}; // Return empty if TS or Service not found in Schedule map
}


std::vector<EventInfo> DvbSiParser::getEventsForTimeRange(const TransportStreamId& tsid, uint16_t serviceId, long long rangeStartMillis, long long rangeEndMillis) const {
    std::vector<EventInfo> overlappingEvents;
    if (rangeStartMillis >= rangeEndMillis) return overlappingEvents;

    // Check both P/F and Schedule tables
    const std::vector<EventInfo>* eventSources[] = {
        nullptr, // Placeholder for P/F
        nullptr  // Placeholder for Schedule
    };

    auto ts_pf_it = events_pf_by_service_.find(tsid);
    if (ts_pf_it != events_pf_by_service_.end()) {
        auto service_pf_it = ts_pf_it->second.find(serviceId);
        if (service_pf_it != ts_pf_it->second.end()) {
            eventSources[0] = &(service_pf_it->second);
        }
    }

    auto ts_sched_it = events_schedule_by_service_.find(tsid);
    if (ts_sched_it != events_schedule_by_service_.end()) {
        auto service_sched_it = ts_sched_it->second.find(serviceId);
        if (service_sched_it != ts_sched_it->second.end()) {
            eventSources[1] = &(service_sched_it->second);
        }
    }

    std::map<uint16_t, EventInfo> uniqueEvents; // Use map to avoid duplicates by event ID

    for (const auto* sourceVector : eventSources) {
        if (sourceVector) {
            for (const auto& event : *sourceVector) {
                long long eventEndMillis = event.getEndTimeMillis();
                // Check for overlap: Event starts before range ends AND Event ends after range starts
                if (event.start_time_millis < rangeEndMillis && eventEndMillis > rangeStartMillis) {
                    uniqueEvents[event.event_id] = event; // Add/overwrite based on event ID
                }
            }
        }
    }

    // Convert map values back to vector
    for (const auto& pair : uniqueEvents) {
        overlappingEvents.push_back(pair.second);
    }

    // Sort the final list by start time
    std::sort(overlappingEvents.begin(), overlappingEvents.end(),
        [](const EventInfo& a, const EventInfo& b) {
            return a.start_time_millis < b.start_time_millis;
        });

    return overlappingEvents;
}