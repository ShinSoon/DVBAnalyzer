#ifndef DVB_DESCRIPTORS_H
#define DVB_DESCRIPTORS_H

#include <cstdint>
#include <cstddef>
#include <string>

#include "dvb_sdt.h"  // ServiceInfo
#include "dvb_eit.h"  // EventInfo

// DVB descriptor tags we understand (ETSI EN 300 468, section 6.2).
constexpr uint8_t kTagServiceDescriptor    = 0x48;
constexpr uint8_t kTagShortEventDescriptor = 0x4D;

// Decode a DVB SI text field. The first byte may be a character-table control
// code; for the Latin alphabets used here we simply skip any leading control
// byte and return the rest as-is.
std::string decodeDvbText(const uint8_t* data, size_t len);

// Walk a descriptor loop (a run of tag/length/value triplets) and copy the
// fields we recognise into the target struct.
void applyServiceDescriptors(const uint8_t* loop, size_t len, ServiceInfo& service);
void applyEventDescriptors(const uint8_t* loop, size_t len, EventInfo& event);

#endif // DVB_DESCRIPTORS_H
