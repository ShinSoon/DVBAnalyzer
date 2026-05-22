#include <gtest/gtest.h>

#include <cstdint>

#include "dvb_time.h"

// --- makeUtcEpochMillis: anchored to well-known UTC instants ---

TEST(DvbTime, UnixEpochAnchor) {
    EXPECT_EQ(makeUtcEpochMillis(1970, 1, 1, 0, 0, 0), 0LL);
}

TEST(DvbTime, Y2kAnchor) {
    // 2000-01-01 00:00:00 UTC == 946684800 seconds since the epoch.
    EXPECT_EQ(makeUtcEpochMillis(2000, 1, 1, 0, 0, 0), 946684800000LL);
}

TEST(DvbTime, HandlesLeapDay) {
    // 2024 is a leap year; 2024-02-29 must be a valid distinct day.
    const long long feb29 = makeUtcEpochMillis(2024, 2, 29, 0, 0, 0);
    const long long mar01 = makeUtcEpochMillis(2024, 3, 1, 0, 0, 0);
    EXPECT_EQ(mar01 - feb29, 86400000LL);
}

// --- MJD encoding: 1970-01-01 is Modified Julian Date 40587 (0x9E8B) ---

TEST(DvbTime, MjdEpochEncoding) {
    uint8_t out[5];
    epochMillisToMjdBcd(0, out);
    EXPECT_EQ(out[0], 0x9E);  // MJD high byte
    EXPECT_EQ(out[1], 0x8B);  // MJD low byte  (0x9E8B == 40587)
    EXPECT_EQ(out[2], 0x00);  // HH (BCD)
    EXPECT_EQ(out[3], 0x00);  // MM (BCD)
    EXPECT_EQ(out[4], 0x00);  // SS (BCD)
}

// --- MJD+BCD round-trip: encode then decode must be identity (second-aligned) ---

TEST(DvbTime, MjdBcdRoundTrip) {
    const char* samples[] = {
        "1970-01-01 00:00:00",
        "2000-01-01 12:00:00",
        "2024-03-15 11:30:45",
        "2024-12-31 23:59:59",
    };
    for (const char* s : samples) {
        std::tm t{};
        std::istringstream ss(s);
        ss >> std::get_time(&t, "%Y-%m-%d %H:%M:%S");
        const long long original = makeUtcEpochMillis(t.tm_year + 1900, t.tm_mon + 1,
                                                      t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
        uint8_t buf[5];
        epochMillisToMjdBcd(original, buf);
        EXPECT_EQ(mjdBcdToEpochMillis(buf), original) << "round-trip failed for " << s;
    }
}

// --- BCD duration codec ---

TEST(DvbTime, BcdDurationDecode) {
    const uint8_t oneHourThirty[3] = {0x01, 0x30, 0x00};  // 01:30:00 BCD
    EXPECT_EQ(bcdDurationToSeconds(oneHourThirty), 5400LL);
}

TEST(DvbTime, BcdDurationRoundTrip) {
    const long long durations[] = {0, 1800, 3600, 7200, 9000, 86399};
    for (long long d : durations) {
        uint8_t buf[3];
        secondsToBcdDuration(d, buf);
        EXPECT_EQ(bcdDurationToSeconds(buf), d) << "round-trip failed for " << d;
    }
}

TEST(DvbTime, FormatUtcIsStable) {
    const long long t = makeUtcEpochMillis(2024, 3, 15, 11, 30, 0);
    EXPECT_EQ(formatUtcTime(t), "2024-03-15 11:30:00 UTC");
}
