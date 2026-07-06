#ifndef FETCH_PACKET_H
#define FETCH_PACKET_H

#include <systemc>
#include <cstdint>
#include <vector>
#include "tlm.h"

// Minimal TLM packet for fetch-style requests.
// This is intentionally small and illustrative: it demonstrates
// how to pack/unpack a fetch request into a tlm payload buffer.

enum class FetchCmd : uint8_t {
    LOAD = 0,       // read a rectangular pixel region
    LOAD_RESP = 1,  // response to LOAD (contains raw bytes)
    WRITE_4x4 = 2,  // write a 4x4 block into DB memory
    GET_EDGE = 3,   // request top/left/right/down reference vectors
    EDGE_RESP = 4   // response to GET_EDGE
};

struct FetchPacket {
    FetchCmd cmd = FetchCmd::LOAD;
    uint8_t plane = 0;      // 0=Y,1=U,2=V
    uint32_t x_px = 0;
    uint32_t y_px = 0;
    uint16_t width = 0;
    uint16_t height = 0;
    uint8_t margin = 0;     // extra pixels around region
    uint8_t downsample = 1; // 1=full,2=half,...
    uint8_t align_px = 1;   // alignment hint in pixels
    uint32_t req_id = 0;    // application tag

    // payload: for WRITE_4x4 contains 16 bytes (4x4 row-major)
    // for LOAD_RESP contains width*height bytes row-major
    std::vector<uint8_t> data;

    FetchPacket() = default;
};

// Serialization helpers (little-endian)
static inline void append_u16_le(std::vector<uint8_t>& buf, uint16_t v) {
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}
static inline void append_u32_le(std::vector<uint8_t>& buf, uint32_t v) {
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}
static inline uint16_t read_u16_le(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}
static inline uint32_t read_u32_le(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

inline std::vector<uint8_t> packFetchPacket(const FetchPacket &p) {
    std::vector<uint8_t> buf;
    buf.reserve(16 + p.data.size());
    buf.push_back(static_cast<uint8_t>(p.cmd));
    buf.push_back(p.plane);
    append_u32_le(buf, p.x_px);
    append_u32_le(buf, p.y_px);
    append_u16_le(buf, p.width);
    append_u16_le(buf, p.height);
    buf.push_back(p.margin);
    buf.push_back(p.downsample);
    buf.push_back(p.align_px);
    append_u32_le(buf, p.req_id);
    if (!p.data.empty()) buf.insert(buf.end(), p.data.begin(), p.data.end());
    return buf;
}

inline FetchPacket unpackFetchPacket(const uint8_t *buf, size_t len) {
    FetchPacket p;
    if (!buf || len < 16) return p; // minimal header is 16 bytes

    p.cmd = static_cast<FetchCmd>(buf[0]);
    p.plane = buf[1];
    p.x_px = read_u32_le(buf + 2);
    p.y_px = read_u32_le(buf + 6);
    p.width = read_u16_le(buf + 10);
    p.height = read_u16_le(buf + 12);
    p.margin = buf[14];
    p.downsample = buf[15];
    // next byte is align_px at offset 16? keep legacy: align at 16, req_id at 17..20
    if (len >= 21) {
        p.align_px = buf[16];
        p.req_id = read_u32_le(buf + 17);
        size_t payload_off = 21;
        if (len > payload_off) p.data.assign(buf + payload_off, buf + len);
    } else if (len >= 17) {
        // if req_id missing (old callers) treat rest as payload
        if (len > 16) p.data.assign(buf + 16, buf + len);
    }
    return p;
}

inline FetchPacket unpackFetchPacket(const tlm::tlm_generic_payload &trans) {
    const uint8_t *buf = reinterpret_cast<const uint8_t*>(trans.get_data_ptr());
    return unpackFetchPacket(buf, trans.get_data_length());
}

#endif
