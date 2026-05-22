#include "ts_demux.h"

bool TsPacket::parse(const uint8_t* packet, TsPacket& out) {
    if (packet[0] != kTsSyncByte) {
        return false;
    }

    out.transport_error = (packet[1] & 0x80) != 0;
    out.payload_unit_start = (packet[1] & 0x40) != 0;
    out.pid = static_cast<uint16_t>(((packet[1] & 0x1F) << 8) | packet[2]);

    const uint8_t adaptation_field_control = (packet[3] >> 4) & 0x03;
    out.continuity_counter = packet[3] & 0x0F;

    size_t offset = 4;
    if (adaptation_field_control & 0x2) {            // adaptation field present
        const uint8_t adaptation_field_length = packet[4];
        offset = 5 + adaptation_field_length;
    }

    if ((adaptation_field_control & 0x1) && offset < kTsPacketSize) {  // payload present
        out.has_payload = true;
        out.payload = packet + offset;
        out.payload_len = kTsPacketSize - offset;
    } else {
        out.has_payload = false;
        out.payload = nullptr;
        out.payload_len = 0;
    }
    return true;
}

void SectionAssembler::resetCurrent() {
    current_.clear();
    expected_ = 0;
}

void SectionAssembler::appendByte(uint8_t b, std::vector<std::vector<uint8_t>>& outSections) {
    current_.push_back(b);

    // Once we have the 3-byte header we know the full section length:
    //   section_length (12 bits) counts everything after byte 2, so the total
    //   section size is 3 + section_length.
    if (expected_ == 0 && current_.size() >= 3) {
        const size_t section_length = ((current_[1] & 0x0F) << 8) | current_[2];
        expected_ = 3 + section_length;
    }

    if (expected_ != 0 && current_.size() >= expected_) {
        outSections.push_back(current_);  // exactly 'expected_' bytes here
        resetCurrent();
    }
}

void SectionAssembler::addPayload(bool payloadUnitStart, const uint8_t* data, size_t len,
                                  std::vector<std::vector<uint8_t>>& outSections) {
    size_t i = 0;

    if (payloadUnitStart) {
        if (len == 0) return;
        const uint8_t pointer_field = data[0];
        i = 1;
        if (collecting_) {
            // The bytes before the pointer belong to the tail of the previous
            // section; consume them, then drop anything still dangling.
            const size_t end = i + pointer_field;
            for (; i < end && i < len; ++i) {
                appendByte(data[i], outSections);
            }
            resetCurrent();
        } else {
            i += pointer_field;  // jump to the first section start
        }
        collecting_ = true;
    }

    for (; i < len; ++i) {
        if (!collecting_) break;
        // 0xFF where a new section's table_id would be is stuffing: rest is padding.
        if (current_.empty() && data[i] == 0xFF) {
            collecting_ = false;
            break;
        }
        appendByte(data[i], outSections);
    }
}

std::map<uint16_t, std::vector<std::vector<uint8_t>>>
TsDemux::demux(const uint8_t* buffer, size_t len) {
    std::map<uint16_t, std::vector<std::vector<uint8_t>>> result;
    std::map<uint16_t, SectionAssembler> assemblers;

    for (size_t pos = 0; pos + kTsPacketSize <= len; pos += kTsPacketSize) {
        TsPacket pkt;
        if (!TsPacket::parse(buffer + pos, pkt)) {
            continue;  // lost sync on this packet; a real demux would resync
        }
        if (pkt.transport_error || !pkt.has_payload) continue;
        if (pids_.find(pkt.pid) == pids_.end()) continue;

        assemblers[pkt.pid].addPayload(pkt.payload_unit_start, pkt.payload, pkt.payload_len,
                                       result[pkt.pid]);
    }

    return result;
}
