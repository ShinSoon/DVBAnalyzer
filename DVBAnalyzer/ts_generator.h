#ifndef TS_GENERATOR_H
#define TS_GENERATOR_H

#include <string>

// Builds a small but standards-shaped MPEG-2 Transport Stream containing SDT
// (PID 0x11) and EIT (PID 0x12) sections, and writes it to 'path'. The logical
// content mirrors simulated_dvb_si.txt so the binary path can be compared
// against the text path. Real CRC-32, MJD+BCD times and descriptor loops are
// emitted, so this doubles as a demonstration of the SI encoding side.
bool generateSampleTs(const std::string& path);

#endif // TS_GENERATOR_H
