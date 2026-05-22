#include "dvb_descriptors.h"

std::string decodeDvbText(const uint8_t* data, size_t len) {
    if (len == 0) return std::string();

    size_t start = 0;
    const uint8_t first = data[0];
    if (first < 0x20) {
        // Character-table selection bytes (ETSI EN 300 468 Annex A).
        if (first == 0x10) start = 3;       // 3-byte selector
        else if (first == 0x1F) start = 2;  // 2-byte selector
        else start = 1;                     // single control byte
    }
    if (start >= len) return std::string();
    return std::string(reinterpret_cast<const char*>(data + start), len - start);
}

void applyServiceDescriptors(const uint8_t* loop, size_t len, ServiceInfo& service) {
    size_t pos = 0;
    while (pos + 2 <= len) {
        const uint8_t tag = loop[pos];
        const uint8_t dlen = loop[pos + 1];
        const uint8_t* body = loop + pos + 2;
        if (pos + 2 + dlen > len) break;  // truncated descriptor

        if (tag == kTagServiceDescriptor && dlen >= 2) {
            // service_descriptor: service_type, provider_name, service_name.
            service.service_type = body[0];
            const uint8_t providerLen = body[1];
            size_t off = 2;
            if (off + providerLen <= dlen) {
                service.provider_name = decodeDvbText(body + off, providerLen);
                off += providerLen;
                if (off < dlen) {
                    const uint8_t nameLen = body[off];
                    ++off;
                    if (off + nameLen <= dlen) {
                        service.service_name = decodeDvbText(body + off, nameLen);
                    }
                }
            }
        }
        pos += 2 + dlen;
    }
}

void applyEventDescriptors(const uint8_t* loop, size_t len, EventInfo& event) {
    size_t pos = 0;
    while (pos + 2 <= len) {
        const uint8_t tag = loop[pos];
        const uint8_t dlen = loop[pos + 1];
        const uint8_t* body = loop + pos + 2;
        if (pos + 2 + dlen > len) break;

        if (tag == kTagShortEventDescriptor && dlen >= 5) {
            // short_event_descriptor: ISO-639 lang (3) + event_name + text.
            size_t off = 3;  // skip language code
            const uint8_t nameLen = body[off++];
            if (off + nameLen <= dlen) {
                event.event_name = decodeDvbText(body + off, nameLen);
                off += nameLen;
                if (off < dlen) {
                    const uint8_t textLen = body[off++];
                    if (off + textLen <= dlen) {
                        event.short_description = decodeDvbText(body + off, textLen);
                    }
                }
            }
        }
        pos += 2 + dlen;
    }
}
