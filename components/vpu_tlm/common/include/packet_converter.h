#ifndef PACKET_CONVERTER_H
#define PACKET_CONVERTER_H

#include "custom_packet.h"
#include <cstdint>
#include <vector>

// Forward declare RecPacket to avoid forcing a hard include from `rec`.
struct RecPacket;

// Lightweight packet conversion helpers. These map only the common
// read-request case (CustomCmd::READ_REQ) into a FetchPacket GET_EDGE
// request. The converter is intentionally conservative: unsupported
// command kinds return false so callers can fall back to alternative
// dispatching.

namespace cdc::components {
namespace packet_converter {

inline uint32_t recSizeToPixels(uint8_t size4x4) {
    switch (size4x4) {
        case 0: return 4u;
        case 1: return 8u;
        case 2: return 16u;
        case 3: return 32u;
        default: return 4u;
    }
}

// Convert a CustomPacket into a serialized FetchPacket buffer. Returns true
// when a conversion was performed (currently only READ_REQ -> GET_EDGE).
// On success `outbuf` contains the packed FetchPacket ready for a TLM send.
bool custom_to_fetch_buf(const CustomPacket &in, std::vector<uint8_t> &outbuf);

// Convert a RecPacket READ_REQ into a serialized FetchPacket GET_EDGE.
// On success `outbuf` contains the packed FetchPacket ready for TLM send.
bool rec_to_fetch_buf(const RecPacket &in, std::vector<uint8_t> &outbuf);

} // namespace packet_converter
} // namespace cdc::components

#endif
