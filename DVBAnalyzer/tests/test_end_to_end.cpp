#include <gtest/gtest.h>

#include <cstdio>
#include <iostream>
#include <sstream>
#include <streambuf>
#include <vector>

#include "dvb_si_parser.h"
#include "dvb_si_common.h"  // parseDateTimeString
#include "ts_generator.h"

// Full pipeline: generate a real binary transport stream, then demux + parse it
// back through the public API. This exercises TS packetisation, section
// reassembly, CRC validation, descriptor loops and MJD/BCD time decoding at once.
class EndToEnd : public ::testing::Test {
protected:
    void SetUp() override {
        // The parser/generator log progress to cout/cerr; redirect both into a
        // sink so the test report stays clean, then restore in TearDown.
        oldCout_ = std::cout.rdbuf(sink_.rdbuf());
        oldCerr_ = std::cerr.rdbuf(sink_.rdbuf());
        ASSERT_TRUE(generateSampleTs(kPath));
        ASSERT_TRUE(parser_.parseDataFromTsFile(kPath));
    }
    void TearDown() override {
        std::cout.rdbuf(oldCout_);
        std::cerr.rdbuf(oldCerr_);
        std::remove(kPath);
    }

    static constexpr const char* kPath = "dvbtests_sample.ts";
    DvbSiParser parser_;
    std::ostringstream sink_;
    std::streambuf* oldCout_ = nullptr;
    std::streambuf* oldCerr_ = nullptr;
    const TransportStreamId ts1_{1001, 10};  // TSID=1001, ONID=10
    const TransportStreamId ts2_{2002, 20};
};

TEST_F(EndToEnd, ServicesDecodedFromSdt) {
    auto services = parser_.getServices(ts1_);
    ASSERT_EQ(services.size(), 3u);
    EXPECT_EQ(services[0].service_id, 101);
    EXPECT_EQ(services[0].service_name, "Channel ONE HD");
    EXPECT_EQ(services[0].provider_name, "MYBC");
    EXPECT_EQ(static_cast<int>(services[0].service_type), 0x11);
    EXPECT_EQ(services[2].service_name, "Radio ONE");
    EXPECT_EQ(static_cast<int>(services[2].running_status), 1);
}

TEST_F(EndToEnd, SecondTransportStreamIsSeparate) {
    auto services = parser_.getServices(ts2_);
    ASSERT_EQ(services.size(), 1u);
    EXPECT_EQ(services[0].service_name, "Movie Central UHD");
    EXPECT_FALSE(services[0].free_ca_mode);  // pay TV in the sample data
}

TEST_F(EndToEnd, ScheduleEventsDecodedWithCorrectTime) {
    auto sched = parser_.getScheduledEvents(ts1_, 101);
    ASSERT_EQ(sched.size(), 3u);
    EXPECT_EQ(sched[0].event_name, "Gardening Tips");
    EXPECT_EQ(sched[0].duration_seconds, 3600);
    EXPECT_EQ(sched[0].start_time_millis, parseDateTimeString("2024-03-15 11:30:00"));
    EXPECT_EQ(sched[0].short_description, "How to prepare your garden.");
}

TEST_F(EndToEnd, PresentFollowingSeparateFromSchedule) {
    auto pf = parser_.getPresentFollowingEvents(ts1_, 101);
    ASSERT_EQ(pf.size(), 2u);
    EXPECT_EQ(pf[0].event_name, "Morning News");
}

TEST_F(EndToEnd, TimeRangeOverlapQuery) {
    const long long start = parseDateTimeString("2024-03-15 11:00:00");
    const long long end   = parseDateTimeString("2024-03-15 13:00:00");
    auto events = parser_.getEventsForTimeRange(ts1_, 101, start, end);

    // Cooking Show (P/F, 11:00), Gardening Tips (11:30), Feature Film (12:30).
    ASSERT_EQ(events.size(), 3u);
    EXPECT_EQ(events[0].event_name, "Cooking Show");
    EXPECT_EQ(events[1].event_name, "Gardening Tips");
    EXPECT_EQ(events[2].event_name, "Feature Film: The Parser");
}

TEST_F(EndToEnd, UnknownServiceReturnsEmpty) {
    EXPECT_TRUE(parser_.getScheduledEvents(ts1_, 9999).empty());
    EXPECT_TRUE(parser_.getServices(TransportStreamId{4242, 99}).empty());
}
