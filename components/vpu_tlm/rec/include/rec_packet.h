#ifndef REC_PACKET_H
#define REC_PACKET_H

#include <systemc>
#include <cstdint>
#include <vector>
#include <bitset>
#include <ostream>
#include <string>
#include <iomanip>
#include <sstream>
#include "tlm.h"

enum class RecCmd : uint8_t { RESIDUAL = 0, COEFF = 1, PRE = 2, READ_REQ = 3 };

enum class PredType : uint8_t { INTRA = 0, MC = 1 };

// RecPacket: typed metadata + owned payload buffer.
// Field widths in comments reflect RTL signals (for reference).
struct RecPacket {
        RecCmd cmd;               // RESIDUAL/COEFF/PRE/READ_REQ
        uint8_t block_idx;        // high bits of 4x4 block y/x: [y11:8|x11:8]
        uint8_t x;                // low 8 bits of 4x4 block x
        uint8_t y;                // low 8 bits of 4x4 block y
        uint8_t size;             // RTL: 2 bits (0:4x4,1:8x8,2:16x16,3:32x32)
        uint8_t sel;              // RTL: 2 bits (TYPE_Y/U/V)
        uint8_t qp;               // RTL: 6 bits
        // NOTE: `type` (single-bit INTRA/INTER) was removed and merged with
        // `pred_type`. The TLM header now stores `pred_type` at byte offset 7
        // (previously occupied by the legacy `type` bit), and the extended
        // header no longer contains `pred_type`.
        uint8_t pred_type;        // engine/prediction type: PredType
        uint8_t mode;             // RTL: 6-bit mode id (planar=0,DC=1,angular..)
        uint8_t pre_sel;          // RTL: 2-bit pre_sel
        uint8_t i4x4_x;           // 4-bit 4x4 x position inside larger block
        uint8_t i4x4_y;           // 4-bit 4x4 y position inside larger block
        std::bitset<256> cbf_mask; // per-block CBF mask (match RTL up to 256 bits)
        std::vector<uint8_t> data; // owned contiguous payload bytes (copy from trans.get_data_ptr())

        RecPacket()
            : cmd(RecCmd::RESIDUAL), block_idx(0), x(0), y(0),
                size(0), sel(0), qp(0), pred_type(static_cast<uint8_t>(PredType::INTRA)),
                mode(1), pre_sel(0), i4x4_x(0), i4x4_y(0), cbf_mask(), data() {}
};

inline std::ostream& operator<<(std::ostream& os, const RecPacket& p) {
    // Header lines
    os << "RecPacket {" << std::endl;

    // cmd as text
    os << "  cmd: ";
    switch (p.cmd) {
        case RecCmd::RESIDUAL: os << "RESIDUAL"; break;
        case RecCmd::COEFF:    os << "COEFF";    break;
        case RecCmd::PRE:      os << "PRE";      break;
        case RecCmd::READ_REQ: os << "READ_REQ"; break;
        default:               os << static_cast<int>(p.cmd); break;
    }
    os << ", idx: " << static_cast<int>(p.block_idx)
       << ", pos: (" << static_cast<int>(p.x) << "," << static_cast<int>(p.y) << ")" << std::endl;
    os << "  size: ";
    switch (p.size) {
        case 0: os << "4x4"; break;
        case 1: os << "8x8"; break;
        case 2: os << "16x16"; break;
        case 3: os << "32x32"; break;
        default: os << static_cast<int>(p.size); break;
    }
        os << ", sel: " << static_cast<int>(p.sel)
            << ", qp: " << static_cast<int>(p.qp) << std::endl;

     os << "  pred_type: ";
     switch (static_cast<PredType>(p.pred_type)) {
          case PredType::INTRA: os << "INTRA"; break;
          case PredType::MC:    os << "MC";    break;
          default:              os << static_cast<int>(p.pred_type); break;
     }
     os << ", mode: " << static_cast<int>(p.mode)
         << ", pre_sel: " << static_cast<int>(p.pre_sel)
         << ", i4x4: (" << static_cast<int>(p.i4x4_x) << "," << static_cast<int>(p.i4x4_y) << ")" << std::endl;

    // cbf_mask as compact hex
    std::string bits = p.cbf_mask.to_string(); // MSB..LSB
    std::string hex;
    hex.reserve(bits.size() / 4);
    for (size_t i = 0; i < bits.size(); i += 4) {
        int v = (bits[i]-'0')<<3 | (bits[i+1]-'0')<<2 | (bits[i+2]-'0')<<1 | (bits[i+3]-'0');
        char ch = (v < 10) ? ('0' + v) : ('A' + (v - 10));
        hex.push_back(ch);
    }
    // trim leading zeros
    size_t first = hex.find_first_not_of('0');
    if (first == std::string::npos) hex = "0";
    else hex = hex.substr(first);
    os << "  cbf_mask: 0x" << hex << std::endl;

    os << "  data_len: " << p.data.size() << std::endl;
    if (!p.data.empty()) {
        os << "  data:" << std::endl;
        const size_t rows = 4, cols = 4;
        size_t total = p.data.size();
        size_t idx = 0;
        for (size_t r = 0; r < rows; ++r) {
            os << "    [";
            for (size_t c = 0; c < cols; ++c) {
                if (idx < total) os << std::setw(4) << static_cast<int>(p.data[idx]);
                else os << std::setw(4) << 0;
                ++idx;
                if (c + 1 < cols) os << ",";
            }
            os << "]" << std::endl;
        }
    }

    os << "}";
    return os;
}

// Helpers to pack/unpack RecPacket to/from a contiguous byte buffer used on the TLM wire.
inline std::vector<uint8_t> packRecPacket(const RecPacket &p) {
    std::vector<uint8_t> buf;
    // header: original 8 bytes + 5 extended bytes
    // header: original 8 bytes (pred_type reused at byte 7) + 4 extended bytes
    // encode cbf_mask as 32 bytes (256 bits) when present; reserve accordingly
    const size_t cbf_bytes = 32;
    buf.reserve(12 + cbf_bytes + p.data.size());
    buf.push_back(static_cast<uint8_t>(p.cmd));
    buf.push_back(p.block_idx);
    buf.push_back(p.x);
    buf.push_back(p.y);
    buf.push_back(p.size);
    buf.push_back(p.sel);
    buf.push_back(p.qp);
    // reuse legacy 8th byte to encode `pred_type`
    buf.push_back(p.pred_type);
    // extended header now contains: mode, pre_sel, i4x4_x, i4x4_y
    buf.push_back(p.mode);
    buf.push_back(p.pre_sel);
    buf.push_back(p.i4x4_x);
    buf.push_back(p.i4x4_y);
    // Serialize cbf_mask as 32 bytes, little-bit-order within each byte
    for (size_t byte = 0; byte < 32; ++byte) {
        uint8_t val = 0;
        for (size_t bit = 0; bit < 8; ++bit) {
            size_t idx = byte * 8 + bit;
            if (p.cbf_mask.test(idx)) val |= static_cast<uint8_t>(1u << bit);
        }
        buf.push_back(val);
    }

    if (!p.data.empty()) buf.insert(buf.end(), p.data.begin(), p.data.end());
    return buf;
}

inline RecPacket unpackRecPacket(const uint8_t *buf, size_t len) {
    RecPacket p;
    if (!buf || len < 8) return p; // return default if too small
    p.cmd = static_cast<RecCmd>(buf[0]);
    p.block_idx = buf[1];
    p.x = buf[2];
    p.y = buf[3];
    p.size = buf[4];
    p.sel = buf[5];
    p.qp = buf[6];
    // pred_type now occupies the legacy byte-7 slot
    p.pred_type = buf[7];
    // extended header: present if length >= 12 (8 + 4)
    if (len >= 12) {
        p.mode = buf[8];
        p.pre_sel = buf[9];
        p.i4x4_x = buf[10];
        p.i4x4_y = buf[11];
        // if we have at least 32 more bytes, treat them as cbf_mask
        const size_t cbf_bytes = 32;
        if (len >= 12 + cbf_bytes) {
            // parse cbf_mask from bytes [12 .. 12+31]
            for (size_t byte = 0; byte < cbf_bytes; ++byte) {
                uint8_t val = buf[12 + byte];
                for (size_t bit = 0; bit < 8; ++bit) {
                    size_t idx = byte * 8 + bit;
                    bool bitset = ((val >> bit) & 0x1u) != 0;
                    if (bitset) p.cbf_mask.set(idx);
                    else p.cbf_mask.reset(idx);
                }
            }
            size_t payload_len = (len > 12 + cbf_bytes) ? (len - (12 + cbf_bytes)) : 0;
            if (payload_len) p.data.assign(buf + 12 + cbf_bytes, buf + 12 + cbf_bytes + payload_len);
        } else {
            // no cbf_mask serialized, payload starts at offset 12
            size_t payload_len = (len > 12) ? (len - 12) : 0;
            if (payload_len) p.data.assign(buf + 12, buf + 12 + payload_len);
        }
    } else {
        // backward-compat: no extended header, payload starts at offset 8
        size_t payload_len = (len > 8) ? (len - 8) : 0;
        if (payload_len) p.data.assign(buf + 8, buf + 8 + payload_len);
    }
    return p;
}

inline RecPacket unpackRecPacket(const tlm::tlm_generic_payload &trans) {
    const uint8_t *buf = reinterpret_cast<const uint8_t*>(trans.get_data_ptr());
    return unpackRecPacket(buf, trans.get_data_length());
}

#endif
