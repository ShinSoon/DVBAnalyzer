#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "dvb_crc32.h"
#include "dvb_section_parser.h"

namespace {

// Build a minimal but valid SDT section carrying one service with a
// service_descriptor, terminated by a correct CRC-32.
std::vector<uint8_t> buildMinimalSdt() {
    std::vector<uint8_t> s;
    s.push_back(kTableIdSdtActual);   // table_id 0x42
    s.push_back(0); s.push_back(0);   // section_length placeholder
    s.push_back(0x03); s.push_back(0xE9);  // transport_stream_id = 1001
    s.push_back(0xC1);                // reserved/version/current_next
    s.push_back(0x00);               // section_number
    s.push_back(0x00);               // last_section_number
    s.push_back(0x00); s.push_back(0x0A);  // original_network_id = 10
    s.push_back(0xFF);               // reserved_future_use

    // --- one service: id 101 ---
    s.push_back(0x00); s.push_back(0x65);  // service_id = 101
    s.push_back(0xFF);               // reserved(6), EIT_schedule=1, EIT_pf=1

    const std::vector<uint8_t> desc = {0x48, 0x05, 0x11, 0x01, 'P', 0x01, 'N'};
    const uint16_t dll = static_cast<uint16_t>(desc.size());  // 7
    s.push_back(static_cast<uint8_t>((4 << 5) | (1 << 4) | ((dll >> 8) & 0x0F)));  // run=4, freeCA=1
    s.push_back(static_cast<uint8_t>(dll & 0xFF));
    s.insert(s.end(), desc.begin(), desc.end());

    // section_length counts everything after byte 2, including the 4 CRC bytes.
    const size_t section_length = (s.size() - 3) + 4;
    s[1] = static_cast<uint8_t>(0xF0 | ((section_length >> 8) & 0x0F));
    s[2] = static_cast<uint8_t>(section_length & 0xFF);

    const uint32_t crc = dvbCrc32(s.data(), s.size());
    s.push_back(static_cast<uint8_t>((crc >> 24) & 0xFF));
    s.push_back(static_cast<uint8_t>((crc >> 16) & 0xFF));
    s.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
    s.push_back(static_cast<uint8_t>(crc & 0xFF));
    return s;
}

}  // namespace

TEST(SdtSection, ParsesValidSection) {
    const std::vector<uint8_t> section = buildMinimalSdt();

    TransportStreamId tsid;
    std::vector<ServiceInfo> services;
    ASSERT_TRUE(parseSdtSection(section, tsid, services));

    EXPECT_EQ(tsid.transport_stream_id, 1001);
    EXPECT_EQ(tsid.original_network_id, 10);
    ASSERT_EQ(services.size(), 1u);

    const ServiceInfo& svc = services[0];
    EXPECT_EQ(svc.service_id, 101);
    EXPECT_EQ(static_cast<int>(svc.service_type), 0x11);
    EXPECT_EQ(svc.provider_name, "P");
    EXPECT_EQ(svc.service_name, "N");
    EXPECT_EQ(static_cast<int>(svc.running_status), 4);
    EXPECT_TRUE(svc.free_ca_mode);
    EXPECT_TRUE(svc.eit_schedule_flag);
    EXPECT_TRUE(svc.eit_present_following_flag);
}

TEST(SdtSection, RejectsCorruptedCrc) {
    std::vector<uint8_t> section = buildMinimalSdt();
    section.back() ^= 0xFF;  // corrupt the final CRC byte

    TransportStreamId tsid;
    std::vector<ServiceInfo> services;
    EXPECT_FALSE(parseSdtSection(section, tsid, services));
}

TEST(SdtSection, RejectsWrongTableId) {
    std::vector<uint8_t> section = buildMinimalSdt();
    section[0] = 0x00;  // not an SDT table_id (CRC now wrong too)

    TransportStreamId tsid;
    std::vector<ServiceInfo> services;
    EXPECT_FALSE(parseSdtSection(section, tsid, services));
}

TEST(SdtSection, RejectsTooShort) {
    const std::vector<uint8_t> tiny(5, 0x00);
    TransportStreamId tsid;
    std::vector<ServiceInfo> services;
    EXPECT_FALSE(parseSdtSection(tiny, tsid, services));
}
