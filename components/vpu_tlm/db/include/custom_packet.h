#ifndef CUSTOM_PACKET_H
#define CUSTOM_PACKET_H

#include <systemc>
#include <cstdint>
#include <vector>
#include <bitset>
#include <ostream>
#include <string>
#include <iomanip>
#include <sstream>
#include "tlm.h"

enum class CustomCmd : uint8_t { RESIDUAL = 0, COEFF = 1, PRE = 2, READ_REQ = 3 };

enum class PredType : uint8_t { INTRA = 0, MC = 1 };

struct CustomPacket {
        CustomCmd cmd;           
        uint8_t block_idx;        // RTL: 5 bits
        uint8_t x;                // RTL: 4 bits
        uint8_t y;                // RTL: 4 bits
        uint8_t size;             // RTL: 2 bits (0:4x4,1:8x8,2:16x16,3:32x32)
        uint8_t sel;              // RTL: 2 bits (TYPE_Y/U/V)
        uint8_t qp;               // RTL: 6 bits
        uint8_t pred_type;        // engine/prediction type: PredType
        uint8_t mode;             // RTL: 6-bit mode id (planar=0,DC=1,angular..)
        uint8_t pre_sel;          // RTL: 2-bit pre_sel
        uint8_t i4x4_x;           // 4-bit 4x4 x position inside larger block
        uint8_t i4x4_y;           // 4-bit 4x4 y position inside larger block
        uint16_t cnt;             // scan counter / runtime counter (9-bit used by db_bs)
        uint8_t state;            // runtime state (3-bit in RTL)
        std::bitset<256> cbf_mask; // per-block CBF mask (match RTL up to 256 bits)
        std::vector<uint8_t> data; // owned contiguous payload bytes

        //for db
        std::bitset<21> mb_partition;   // RTL: [20:0]
        std::bitset<42> mb_p_pu_mode;  //RTL: 42 bits

        CustomPacket()
            : cmd(CustomCmd::RESIDUAL), block_idx(0), x(0), y(0),
                size(0), sel(0), qp(0), pred_type(static_cast<uint8_t>(PredType::INTRA)),
                mode(1), pre_sel(0), i4x4_x(0), i4x4_y(0), cnt(0), state(0), cbf_mask(), data(), mb_partition(), mb_p_pu_mode() {}
};

inline std::ostream& operator<<(std::ostream& os, const CustomPacket& p) {
    // Header lines
    os << "CustomPacket {" << std::endl;

    // cmd as text
    os << "  cmd: ";
    switch (p.cmd) {
        case CustomCmd::RESIDUAL: os << "RESIDUAL"; break;
        case CustomCmd::COEFF:    os << "COEFF";    break;
        case CustomCmd::PRE:      os << "PRE";      break;
        case CustomCmd::READ_REQ: os << "READ_REQ"; break;
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

    // print partition metadata (compact hex)
    unsigned long long part_val = 0ULL;
    for (size_t i = 0; i < 21; ++i) if (p.mb_partition.test(i)) part_val |= (1ULL << i);
    unsigned long long pu_val = 0ULL;
    for (size_t i = 0; i < 42; ++i) if (p.mb_p_pu_mode.test(i)) pu_val |= (1ULL << i);
    std::ostringstream oss;
    oss << std::hex << part_val;
    os << "  mb_partition: 0x" << oss.str() << std::dec << std::endl;
    std::ostringstream oss2;
    oss2 << std::hex << pu_val;
    os << "  mb_p_pu_mode: 0x" << oss2.str() << std::dec << std::endl;

    // cbf_mask as compact hex (existing behavior)
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

    os << "  cnt: " << p.cnt << ", state: " << static_cast<int>(p.state) << std::endl;

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

inline std::vector<uint8_t> packCustomPacket(const CustomPacket &p) {
    std::vector<uint8_t> buf;
    const size_t cbf_bytes = 32;
    const size_t mb_part_bytes = (21 + 7) / 8; // 3
    const size_t mb_p_pu_bytes = (42 + 7) / 8; // 6
    const size_t partition_bytes = mb_part_bytes + mb_p_pu_bytes; // 9
    buf.reserve(15 + partition_bytes + cbf_bytes + p.data.size());
    buf.push_back(static_cast<uint8_t>(p.cmd));
    buf.push_back(p.block_idx);
    buf.push_back(p.x);
    buf.push_back(p.y);
    buf.push_back(p.size);
    buf.push_back(p.sel);
    buf.push_back(p.qp);
    buf.push_back(p.pred_type);
    buf.push_back(p.mode);
    buf.push_back(p.pre_sel);
    buf.push_back(p.i4x4_x);
    buf.push_back(p.i4x4_y);
    // new fields: cnt (2 bytes, little-endian), state (1 byte)
    buf.push_back(static_cast<uint8_t>(p.cnt & 0xFF));
    buf.push_back(static_cast<uint8_t>((p.cnt >> 8) & 0xFF));
    buf.push_back(p.state);

    // Serialize mb_partition (little-bit-order within each byte)
    for (size_t byte = 0; byte < mb_part_bytes; ++byte) {
        uint8_t val = 0;
        for (size_t bit = 0; bit < 8; ++bit) {
            size_t idx = byte * 8 + bit;
            if (idx < 21 && p.mb_partition.test(idx)) val |= static_cast<uint8_t>(1u << bit);
        }
        buf.push_back(val);
    }
    // Serialize mb_p_pu_mode (little-bit-order)
    for (size_t byte = 0; byte < mb_p_pu_bytes; ++byte) {
        uint8_t val = 0;
        for (size_t bit = 0; bit < 8; ++bit) {
            size_t idx = byte * 8 + bit;
            if (idx < 42 && p.mb_p_pu_mode.test(idx)) val |= static_cast<uint8_t>(1u << bit);
        }
        buf.push_back(val);
    }

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

inline CustomPacket unpackCustomPacket(const uint8_t *buf, size_t len) {
    CustomPacket p;
    if (!buf || len < 8) return p; // return default if too small
    p.cmd = static_cast<CustomCmd>(buf[0]);
    p.block_idx = buf[1];
    p.x = buf[2];
    p.y = buf[3];
    p.size = buf[4];
    p.sel = buf[5];
    p.qp = buf[6];
    p.pred_type = buf[7];
    // extended header: present if length >= 12 (8 + 4)
    if (len >= 12) {
        p.mode = buf[8];
        p.pre_sel = buf[9];
        p.i4x4_x = buf[10];
        p.i4x4_y = buf[11];

        // detect presence of new fields (cnt+state). If present, header base moves to 15
        size_t base = 12;
        if (len >= 15) {
            p.cnt = static_cast<uint16_t>(buf[12]) | (static_cast<uint16_t>(buf[13]) << 8);
            p.state = buf[14];
            base = 15;
        }

        const size_t mb_part_bytes = (21 + 7) / 8; // 3
        const size_t mb_p_pu_bytes = (42 + 7) / 8; // 6
        const size_t partition_bytes = mb_part_bytes + mb_p_pu_bytes; // 9
        const size_t cbf_bytes = 32;

        // extended header (base) + partition_bytes + cbf_bytes
        if (len >= base + partition_bytes + cbf_bytes) {
            // parse mb_partition from bytes [base .. base+mb_part_bytes-1]
            for (size_t byte = 0; byte < mb_part_bytes; ++byte) {
                uint8_t val = buf[base + byte];
                for (size_t bit = 0; bit < 8; ++bit) {
                    size_t idx = byte * 8 + bit;
                    if (idx < 21) {
                        bool bitset = ((val >> bit) & 0x1u) != 0;
                        if (bitset) p.mb_partition.set(idx);
                        else p.mb_partition.reset(idx);
                    }
                }
            }
            // parse mb_p_pu_mode from following bytes
            for (size_t byte = 0; byte < mb_p_pu_bytes; ++byte) {
                uint8_t val = buf[base + mb_part_bytes + byte];
                for (size_t bit = 0; bit < 8; ++bit) {
                    size_t idx = byte * 8 + bit;
                    if (idx < 42) {
                        bool bitset = ((val >> bit) & 0x1u) != 0;
                        if (bitset) p.mb_p_pu_mode.set(idx);
                        else p.mb_p_pu_mode.reset(idx);
                    }
                }
            }
            // parse cbf_mask at offset base + partition_bytes
            size_t cbf_off = base + partition_bytes;
            for (size_t byte = 0; byte < cbf_bytes; ++byte) {
                uint8_t val = buf[cbf_off + byte];
                for (size_t bit = 0; bit < 8; ++bit) {
                    size_t idx = byte * 8 + bit;
                    bool bitset = ((val >> bit) & 0x1u) != 0;
                    if (bitset) p.cbf_mask.set(idx);
                    else p.cbf_mask.reset(idx);
                }
            }
            size_t payload_len = (len > cbf_off + cbf_bytes) ? (len - (cbf_off + cbf_bytes)) : 0;
            if (payload_len) p.data.assign(buf + cbf_off + cbf_bytes, buf + cbf_off + cbf_bytes + payload_len);
        }
        // fallback: extended header present but no partition; cbf at offset base
        else if (len >= base + cbf_bytes) {
            for (size_t byte = 0; byte < cbf_bytes; ++byte) {
                uint8_t val = buf[base + byte];
                for (size_t bit = 0; bit < 8; ++bit) {
                    size_t idx = byte * 8 + bit;
                    bool bitset = ((val >> bit) & 0x1u) != 0;
                    if (bitset) p.cbf_mask.set(idx);
                    else p.cbf_mask.reset(idx);
                }
            }
            size_t payload_len = (len > base + cbf_bytes) ? (len - (base + cbf_bytes)) : 0;
            if (payload_len) p.data.assign(buf + base + cbf_bytes, buf + base + cbf_bytes + payload_len);
        }
        // extended header present but no cbf_mask: payload starts at offset base
        else {
            size_t payload_len = (len > base) ? (len - base) : 0;
            if (payload_len) p.data.assign(buf + base, buf + base + payload_len);
        }
    } else {
        // backward-compat: no extended header, payload starts at offset 8
        size_t payload_len = (len > 8) ? (len - 8) : 0;
        if (payload_len) p.data.assign(buf + 8, buf + 8 + payload_len);
    }
    return p;
}

inline CustomPacket unpackCustomPacket(const tlm::tlm_generic_payload &trans) {
    const uint8_t *buf = reinterpret_cast<const uint8_t*>(trans.get_data_ptr());
    return unpackCustomPacket(buf, trans.get_data_length());
}
#endif