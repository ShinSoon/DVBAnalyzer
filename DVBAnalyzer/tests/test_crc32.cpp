#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "dvb_crc32.h"

// CRC-32/MPEG-2 has a well-known catalogue check value: the CRC of the ASCII
// string "123456789" is 0x0376E6E7 (poly 0x04C11DB7, init 0xFFFFFFFF, no
// reflection, no final XOR). This pins our implementation to the standard.
TEST(Crc32, KnownAnswerCheckValue) {
    const char* msg = "123456789";
    const uint32_t crc = dvbCrc32(reinterpret_cast<const uint8_t*>(msg), 9);
    EXPECT_EQ(crc, 0x0376E6E7u);
}

TEST(Crc32, EmptyInputIsInitialValue) {
    EXPECT_EQ(dvbCrc32(nullptr, 0), 0xFFFFFFFFu);
}

// The property the parser actually relies on: a section that carries its own
// correct CRC in the trailing 4 bytes has a total CRC remainder of zero.
TEST(Crc32, AppendingCrcMakesRemainderZero) {
    std::vector<uint8_t> data = {0x42, 0xF0, 0x11, 0x00, 0x01, 0x02, 0x03, 0x04};
    const uint32_t crc = dvbCrc32(data.data(), data.size());
    data.push_back(static_cast<uint8_t>((crc >> 24) & 0xFF));
    data.push_back(static_cast<uint8_t>((crc >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>(crc & 0xFF));

    EXPECT_TRUE(dvbSectionCrcOk(data.data(), data.size()));
}

TEST(Crc32, SingleBitErrorIsDetected) {
    std::vector<uint8_t> data = {0x42, 0xF0, 0x11, 0x00, 0x01, 0x02, 0x03, 0x04};
    const uint32_t crc = dvbCrc32(data.data(), data.size());
    data.push_back(static_cast<uint8_t>((crc >> 24) & 0xFF));
    data.push_back(static_cast<uint8_t>((crc >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>(crc & 0xFF));

    data[4] ^= 0x01;  // corrupt one payload bit
    EXPECT_FALSE(dvbSectionCrcOk(data.data(), data.size()));
}
