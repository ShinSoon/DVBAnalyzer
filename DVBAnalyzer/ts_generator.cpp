#include "ts_generator.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "dvb_crc32.h"
#include "dvb_section_parser.h"  // PIDs and table_ids
#include "ts_demux.h"            // kTsPacketSize, kTsSyncByte
#include "dvb_time.h"
#include "dvb_si_common.h"       // parseDateTimeString (UTC)

namespace {

using Bytes = std::vector<uint8_t>;

void push16(Bytes& b, uint16_t v) {
    b.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    b.push_back(static_cast<uint8_t>(v & 0xFF));
}

void pushStr(Bytes& b, const std::string& s) {
    b.insert(b.end(), s.begin(), s.end());
}

// Fill in section_length and append the trailing CRC-32 over the whole section.
void finalizeSection(Bytes& s) {
    const size_t section_length = (s.size() - 3) + 4;  // bytes after byte 2, incl CRC
    s[1] = static_cast<uint8_t>(0xF0 | ((section_length >> 8) & 0x0F));  // syntax=1, reserved=111
    s[2] = static_cast<uint8_t>(section_length & 0xFF);
    const uint32_t crc = dvbCrc32(s.data(), s.size());
    s.push_back(static_cast<uint8_t>((crc >> 24) & 0xFF));
    s.push_back(static_cast<uint8_t>((crc >> 16) & 0xFF));
    s.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
    s.push_back(static_cast<uint8_t>(crc & 0xFF));
}

Bytes buildServiceDescriptor(uint8_t serviceType, const std::string& provider,
                             const std::string& name) {
    Bytes body;
    body.push_back(serviceType);
    body.push_back(static_cast<uint8_t>(provider.size()));
    pushStr(body, provider);
    body.push_back(static_cast<uint8_t>(name.size()));
    pushStr(body, name);

    Bytes d;
    d.push_back(0x48);  // service_descriptor tag
    d.push_back(static_cast<uint8_t>(body.size()));
    d.insert(d.end(), body.begin(), body.end());
    return d;
}

Bytes buildShortEventDescriptor(const std::string& name, const std::string& text) {
    Bytes body;
    body.push_back('e'); body.push_back('n'); body.push_back('g');  // ISO-639 language
    body.push_back(static_cast<uint8_t>(name.size()));
    pushStr(body, name);
    body.push_back(static_cast<uint8_t>(text.size()));
    pushStr(body, text);

    Bytes d;
    d.push_back(0x4D);  // short_event_descriptor tag
    d.push_back(static_cast<uint8_t>(body.size()));
    d.insert(d.end(), body.begin(), body.end());
    return d;
}

struct GenService {
    uint16_t service_id;
    bool eit_sched, eit_pf;
    uint8_t running_status;
    bool free_ca;
    uint8_t service_type;
    std::string provider;
    std::string name;
};

struct GenEvent {
    uint16_t event_id;
    std::string startTime;  // "YYYY-MM-DD HH:MM:SS" UTC
    long long durationSec;
    std::string name;
    std::string desc;
};

Bytes buildSdtSection(uint16_t onid, uint16_t tsid, const std::vector<GenService>& services) {
    Bytes s;
    s.push_back(kTableIdSdtActual);  // table_id 0x42
    s.push_back(0); s.push_back(0);  // section_length placeholder
    push16(s, tsid);                 // transport_stream_id
    s.push_back(0xC1);               // reserved(2)=11, version=0, current_next=1
    s.push_back(0x00);               // section_number
    s.push_back(0x00);               // last_section_number
    push16(s, onid);                 // original_network_id
    s.push_back(0xFF);               // reserved_future_use

    for (const auto& svc : services) {
        push16(s, svc.service_id);
        // reserved(6)=111111, EIT_schedule_flag, EIT_present_following_flag
        s.push_back(static_cast<uint8_t>(0xFC | (svc.eit_sched ? 0x02 : 0) | (svc.eit_pf ? 0x01 : 0)));

        const Bytes desc = buildServiceDescriptor(svc.service_type, svc.provider, svc.name);
        const uint16_t dll = static_cast<uint16_t>(desc.size());
        // running_status(3), free_CA_mode(1), descriptors_loop_length(12)
        s.push_back(static_cast<uint8_t>((svc.running_status << 5) |
                                         ((svc.free_ca ? 1 : 0) << 4) |
                                         ((dll >> 8) & 0x0F)));
        s.push_back(static_cast<uint8_t>(dll & 0xFF));
        s.insert(s.end(), desc.begin(), desc.end());
    }

    finalizeSection(s);
    return s;
}

Bytes buildEitSection(uint16_t onid, uint16_t tsid, uint16_t serviceId, uint8_t tableId,
                      const std::vector<GenEvent>& events) {
    Bytes s;
    s.push_back(tableId);
    s.push_back(0); s.push_back(0);  // section_length placeholder
    push16(s, serviceId);            // service_id (table id extension)
    s.push_back(0xC1);               // reserved, version, current_next
    s.push_back(0x00);               // section_number
    s.push_back(0x00);               // last_section_number
    push16(s, tsid);                 // transport_stream_id
    push16(s, onid);                 // original_network_id
    s.push_back(0x00);               // segment_last_section_number
    s.push_back(tableId);            // last_table_id

    for (const auto& ev : events) {
        push16(s, ev.event_id);
        uint8_t mjdBcd[5];
        epochMillisToMjdBcd(parseDateTimeString(ev.startTime), mjdBcd);
        s.insert(s.end(), mjdBcd, mjdBcd + 5);
        uint8_t dur[3];
        secondsToBcdDuration(ev.durationSec, dur);
        s.insert(s.end(), dur, dur + 3);

        const Bytes desc = buildShortEventDescriptor(ev.name, ev.desc);
        const uint16_t dll = static_cast<uint16_t>(desc.size());
        // running_status(3)=4 (running), free_CA_mode(1)=0, descriptors_loop_length(12)
        s.push_back(static_cast<uint8_t>((4 << 5) | (0 << 4) | ((dll >> 8) & 0x0F)));
        s.push_back(static_cast<uint8_t>(dll & 0xFF));
        s.insert(s.end(), desc.begin(), desc.end());
    }

    finalizeSection(s);
    return s;
}

// Split one section into 188-byte TS packets on the given PID. The first packet
// carries PUSI and a pointer_field; the rest are continuations; the tail of the
// last packet is padded with 0xFF stuffing.
void packetizeSection(uint16_t pid, const Bytes& section, uint8_t& continuityCounter,
                      Bytes& out) {
    Bytes payload;
    payload.push_back(0x00);  // pointer_field: section starts immediately
    payload.insert(payload.end(), section.begin(), section.end());

    size_t pos = 0;
    bool first = true;
    while (pos < payload.size()) {
        Bytes pkt;
        pkt.reserve(kTsPacketSize);
        pkt.push_back(kTsSyncByte);
        uint8_t b1 = static_cast<uint8_t>((pid >> 8) & 0x1F);
        if (first) b1 |= 0x40;  // payload_unit_start_indicator
        pkt.push_back(b1);
        pkt.push_back(static_cast<uint8_t>(pid & 0xFF));
        pkt.push_back(static_cast<uint8_t>(0x10 | (continuityCounter & 0x0F)));  // payload only
        continuityCounter = (continuityCounter + 1) & 0x0F;

        const size_t space = kTsPacketSize - 4;
        const size_t chunk = std::min(space, payload.size() - pos);
        pkt.insert(pkt.end(), payload.begin() + pos, payload.begin() + pos + chunk);
        pos += chunk;
        while (pkt.size() < kTsPacketSize) pkt.push_back(0xFF);  // stuffing

        out.insert(out.end(), pkt.begin(), pkt.end());
        first = false;
    }
}

}  // namespace

bool generateSampleTs(const std::string& path) {
    // --- Logical content, mirroring simulated_dvb_si.txt ---
    const std::vector<GenService> ts1Services = {
        {101, true, true, 4, true,  0x11, "MYBC",    "Channel ONE HD"},
        {102, true, true, 4, true,  0x01, "MYBC",    "Channel TWO SD"},
        {103, true, true, 1, true,  0x02, "MYBC",    "Radio ONE"},
    };
    const std::vector<GenService> ts2Services = {
        {201, true, true, 4, false, 0x19, "PayView", "Movie Central UHD"},
    };

    const std::vector<GenEvent> svc101pf = {
        {5001, "2024-03-15 10:00:00", 3600, "Morning News", "The latest news headlines."},
        {5002, "2024-03-15 11:00:00", 1800, "Cooking Show", "Making simple snacks."},
    };
    const std::vector<GenEvent> svc101sched = {
        {5003, "2024-03-15 11:30:00", 3600, "Gardening Tips",        "How to prepare your garden."},
        {5004, "2024-03-15 12:30:00", 7200, "Feature Film: The Parser", "A gripping tale of data."},
        {5005, "2024-03-15 14:30:00", 3600, "Sports Round-Up",       "Highlights from the week."},
    };
    const std::vector<GenEvent> svc102pf = {
        {6001, "2024-03-15 10:30:00", 5400, "Cartoon Hour", "Classic cartoons."},
        {6002, "2024-03-15 12:00:00", 3600, "Quiz Time",    "Test your knowledge."},
    };
    const std::vector<GenEvent> svc102sched = {
        {6003, "2024-03-15 13:00:00", 3600, "Documentary", "Exploring the Amazon."},
        {6004, "2024-03-15 14:00:00", 1800, "Local News",  "Updates from the region."},
    };
    const std::vector<GenEvent> svc201pf = {
        {7001, "2024-03-15 10:00:00", 9000, "Blockbuster Movie", "Action adventure premiere."},
        {7002, "2024-03-15 12:30:00", 8100, "Sci-Fi Classic",    "Journey to another world."},
    };
    const std::vector<GenEvent> svc201sched = {
        {7003, "2024-03-15 14:45:00", 7200, "Animated Feature", "Family fun adventure."},
    };

    // --- Build sections ---
    std::vector<Bytes> sdtSections;
    sdtSections.push_back(buildSdtSection(10, 1001, ts1Services));
    sdtSections.push_back(buildSdtSection(20, 2002, ts2Services));

    const uint8_t kEitPf = 0x4E;     // present/following, actual TS
    const uint8_t kEitSched = 0x50;  // schedule, actual TS
    std::vector<Bytes> eitSections;
    eitSections.push_back(buildEitSection(10, 1001, 101, kEitPf, svc101pf));
    eitSections.push_back(buildEitSection(10, 1001, 101, kEitSched, svc101sched));
    eitSections.push_back(buildEitSection(10, 1001, 102, kEitPf, svc102pf));
    eitSections.push_back(buildEitSection(10, 1001, 102, kEitSched, svc102sched));
    eitSections.push_back(buildEitSection(20, 2002, 201, kEitPf, svc201pf));
    eitSections.push_back(buildEitSection(20, 2002, 201, kEitSched, svc201sched));

    // --- Packetize ---
    Bytes ts;
    uint8_t ccSdt = 0, ccEit = 0;
    for (const auto& sec : sdtSections) packetizeSection(kPidSdt, sec, ccSdt, ts);
    for (const auto& sec : eitSections) packetizeSection(kPidEit, sec, ccEit, ts);

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        std::cerr << "Error: could not open '" << path << "' for writing." << std::endl;
        return false;
    }
    out.write(reinterpret_cast<const char*>(ts.data()), static_cast<std::streamsize>(ts.size()));
    std::cout << "Wrote " << ts.size() << " bytes (" << ts.size() / kTsPacketSize
              << " TS packets) to \"" << path << "\"." << std::endl;
    return true;
}
