#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "dvb_descriptors.h"

TEST(DvbText, StripsLeadingControlByte) {
    // A single character-table control byte (< 0x20) must be dropped.
    const uint8_t withControl[] = {0x05, 'H', 'i'};
    EXPECT_EQ(decodeDvbText(withControl, sizeof(withControl)), "Hi");
}

TEST(DvbText, PassesPlainText) {
    const uint8_t plain[] = {'A', 'B', 'C'};
    EXPECT_EQ(decodeDvbText(plain, sizeof(plain)), "ABC");
}

TEST(DvbText, EmptyIsEmpty) {
    EXPECT_EQ(decodeDvbText(nullptr, 0), "");
}

TEST(Descriptors, ServiceDescriptorFields) {
    // service_descriptor (tag 0x48): service_type, provider_name, service_name.
    //   body = type(0x19) | provLen(7)"PayView" | nameLen(9)"Movie HD!"
    std::vector<uint8_t> loop = {
        0x48, 0x00,                                   // tag, length (filled below)
        0x19,                                         // service_type = UHD
        0x07, 'P', 'a', 'y', 'V', 'i', 'e', 'w',      // provider_name
        0x09, 'M', 'o', 'v', 'i', 'e', ' ', 'H', 'D', '!',  // service_name
    };
    loop[1] = static_cast<uint8_t>(loop.size() - 2);  // descriptor length

    ServiceInfo svc;
    applyServiceDescriptors(loop.data(), loop.size(), svc);

    EXPECT_EQ(static_cast<int>(svc.service_type), 0x19);
    EXPECT_EQ(svc.provider_name, "PayView");
    EXPECT_EQ(svc.service_name, "Movie HD!");
}

TEST(Descriptors, ShortEventDescriptorFields) {
    // short_event_descriptor (tag 0x4D): lang(3), event_name, text.
    std::vector<uint8_t> loop = {
        0x4D, 0x00,                          // tag, length (filled below)
        'e', 'n', 'g',                       // ISO-639 language code
        0x04, 'N', 'e', 'w', 's',            // event_name = "News"
        0x06, 'H', 'e', 'a', 'd', 'e', 'r',  // text = "Header"
    };
    loop[1] = static_cast<uint8_t>(loop.size() - 2);

    EventInfo evt;
    applyEventDescriptors(loop.data(), loop.size(), evt);

    EXPECT_EQ(evt.event_name, "News");
    EXPECT_EQ(evt.short_description, "Header");
}

TEST(Descriptors, UnknownTagIsSkipped) {
    // An unrecognised descriptor (tag 0x99) before a known one must be skipped
    // over using its length, not mis-parsed.
    std::vector<uint8_t> loop = {
        0x99, 0x02, 0xAA, 0xBB,              // unknown descriptor
        0x4D, 0x00, 'e', 'n', 'g', 0x02, 'O', 'K', 0x00,  // short_event
    };
    loop[5] = static_cast<uint8_t>(loop.size() - 6);  // length of the 0x4D descriptor

    EventInfo evt;
    applyEventDescriptors(loop.data(), loop.size(), evt);
    EXPECT_EQ(evt.event_name, "OK");
}
