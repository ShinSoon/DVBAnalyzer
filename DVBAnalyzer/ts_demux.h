#ifndef TS_DEMUX_H
#define TS_DEMUX_H

#include <cstdint>
#include <cstddef>
#include <vector>
#include <map>
#include <set>

// MPEG-2 Transport Stream packet size (no FEC / 204-byte variants here).
constexpr size_t kTsPacketSize = 188;
constexpr uint8_t kTsSyncByte = 0x47;

// A parsed view over one 188-byte TS packet header. Payload points INTO the
// original buffer (no copy).
struct TsPacket {
    bool transport_error = false;       // transport_error_indicator
    bool payload_unit_start = false;    // PUSI: a new PSI/SI section starts here
    uint16_t pid = 0x1FFF;              // 13-bit packet identifier
    uint8_t continuity_counter = 0;
    bool has_payload = false;
    const uint8_t* payload = nullptr;
    size_t payload_len = 0;

    // Parse a single 188-byte packet. Returns false if the sync byte is wrong.
    static bool parse(const uint8_t* packet, TsPacket& out);
};

// Reassembles complete PSI/SI sections for one PID out of the (possibly
// fragmented) payloads of successive TS packets. Handles the pointer_field that
// follows PUSI and trailing 0xFF stuffing between sections.
class SectionAssembler {
public:
    // Feed one packet's payload. Any sections completed by this payload are
    // appended to outSections.
    void addPayload(bool payloadUnitStart, const uint8_t* data, size_t len,
                    std::vector<std::vector<uint8_t>>& outSections);

private:
    void appendByte(uint8_t b, std::vector<std::vector<uint8_t>>& outSections);
    void resetCurrent();

    std::vector<uint8_t> current_;  // bytes of the section currently being built
    size_t expected_ = 0;           // full section size once the header is seen
    bool collecting_ = false;       // have we synced to a section start yet?
};

// Demultiplexes a whole in-memory transport stream, returning the completed
// sections for each PID of interest. PID -> list of section byte buffers.
class TsDemux {
public:
    void addPid(uint16_t pid) { pids_.insert(pid); }

    std::map<uint16_t, std::vector<std::vector<uint8_t>>>
    demux(const uint8_t* buffer, size_t len);

private:
    std::set<uint16_t> pids_;
};

#endif // TS_DEMUX_H
