#ifndef BIT_READER_H
#define BIT_READER_H

#include <cstdint>
#include <cstddef>

// Minimal big-endian bit reader for the small sub-byte fields that appear all
// over DVB SI (e.g. running_status:3, free_CA_mode:1, descriptors_loop_length:12).
// Reads up to 32 bits at a time, MSB-first, the way the bitstream is laid out.
class BitReader {
public:
    BitReader(const uint8_t* data, size_t sizeBytes)
        : data_(data), sizeBits_(sizeBytes * 8), pos_(0) {}

    uint32_t read(unsigned numBits) {
        uint32_t value = 0;
        while (numBits--) {
            value = (value << 1) | nextBit();
        }
        return value;
    }

    void skip(unsigned numBits) { pos_ += numBits; }

    size_t bitPos() const { return pos_; }
    size_t bytePos() const { return pos_ / 8; }

private:
    uint32_t nextBit() {
        if (pos_ >= sizeBits_) { ++pos_; return 0; }
        const size_t byteIndex = pos_ >> 3;
        const unsigned bitIndex = 7 - (pos_ & 7);
        ++pos_;
        return (data_[byteIndex] >> bitIndex) & 0x1u;
    }

    const uint8_t* data_;
    size_t sizeBits_;
    size_t pos_;
};

#endif // BIT_READER_H
