#ifndef DVB_CRC32_H
#define DVB_CRC32_H

#include <cstdint>
#include <cstddef>

// MPEG-2 / DVB section CRC-32.
// Polynomial 0x04C11DB7, initial value 0xFFFFFFFF, MSB-first, no input/output
// reflection and no final XOR. This is the CRC used by all PSI/SI sections
// (PAT/PMT/SDT/EIT/...). See ISO/IEC 13818-1 Annex B and ETSI EN 300 468.
uint32_t dvbCrc32(const uint8_t* data, size_t len);

// A received section is valid when the CRC computed over the WHOLE section
// (header + body + the trailing 4 CRC bytes) equals zero.
inline bool dvbSectionCrcOk(const uint8_t* section, size_t len) {
    return len >= 4 && dvbCrc32(section, len) == 0;
}

#endif // DVB_CRC32_H
