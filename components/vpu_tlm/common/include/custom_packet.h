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
    uint8_t block_idx;
    uint8_t x;
    uint8_t y;
    uint8_t size;
    uint8_t sel;
    uint8_t qp;
    uint8_t pred_type;
    uint8_t mode;
    uint8_t pre_sel;
    uint8_t i4x4_x;
    uint8_t i4x4_y;
    uint16_t cnt;
    uint8_t state;
    std::bitset<256> cbf_mask;
    std::vector<uint8_t> data;

    // db fields
    std::bitset<21> mb_partition;
    std::bitset<42> mb_p_pu_mode;

    // Filter metadata
    uint8_t bs{0};
    uint8_t qp_p{0};
    uint8_t qp_q{0};
    bool cbf_p{false};
    bool cbf_q{false};
    bool tu_edge{false};
    bool pu_edge{false};
    bool is_ver{true};
    uint32_t mv_p{0};
    uint32_t mv_q{0};

    CustomPacket()
        : cmd(CustomCmd::RESIDUAL), block_idx(0), x(0), y(0), size(0), sel(0), qp(0), pred_type(static_cast<uint8_t>(PredType::INTRA)),
          mode(1), pre_sel(0), i4x4_x(0), i4x4_y(0), cnt(0), state(0), cbf_mask(), data(), mb_partition(), mb_p_pu_mode() {}
};

inline std::ostream& operator<<(std::ostream& os, const CustomPacket& p) {
    os << "CustomPacket {" << std::endl;
    os << "  cmd: ";
    switch (p.cmd) { case CustomCmd::RESIDUAL: os << "RESIDUAL"; break; case CustomCmd::COEFF: os << "COEFF"; break; case CustomCmd::PRE: os << "PRE"; break; case CustomCmd::READ_REQ: os << "READ_REQ"; break; default: os << static_cast<int>(p.cmd); break; }
    os << ", idx: " << static_cast<int>(p.block_idx) << ", pos: (" << static_cast<int>(p.x) << "," << static_cast<int>(p.y) << ")" << std::endl;
    os << "  size: ";
    switch (p.size) { case 0: os << "4x4"; break; case 1: os << "8x8"; break; case 2: os << "16x16"; break; case 3: os << "32x32"; break; default: os << static_cast<int>(p.size); break; }
    os << ", sel: " << static_cast<int>(p.sel) << ", qp: " << static_cast<int>(p.qp) << std::endl;
    os << "  pred_type: ";
    switch (static_cast<PredType>(p.pred_type)) { case PredType::INTRA: os << "INTRA"; break; case PredType::MC: os << "MC"; break; default: os << static_cast<int>(p.pred_type); break; }
    os << ", mode: " << static_cast<int>(p.mode) << ", pre_sel: " << static_cast<int>(p.pre_sel) << ", i4x4: (" << static_cast<int>(p.i4x4_x) << "," << static_cast<int>(p.i4x4_y) << ")" << std::endl;

    unsigned long long part_val = 0ULL;
    for (size_t i = 0; i < 21; ++i) if (p.mb_partition.test(i)) part_val |= (1ULL << i);
    unsigned long long pu_val = 0ULL;
    for (size_t i = 0; i < 42; ++i) if (p.mb_p_pu_mode.test(i)) pu_val |= (1ULL << i);
    std::ostringstream oss; oss << std::hex << part_val;
    os << "  mb_partition: 0x" << oss.str() << std::dec << std::endl;
    std::ostringstream oss2; oss2 << std::hex << pu_val;
    os << "  mb_p_pu_mode: 0x" << oss2.str() << std::dec << std::endl;

    const size_t cbf_bytes = 32;
    uint8_t bytes[cbf_bytes];
    for (size_t byte = 0; byte < cbf_bytes; ++byte) {
        uint8_t val = 0;
        for (size_t bit = 0; bit < 8; ++bit) {
            size_t idx = byte * 8 + bit;
            if (p.cbf_mask.test(idx)) val |= static_cast<uint8_t>(1u << bit);
        }
        bytes[byte] = val;
    }
    int msb = -1;
    for (int b = static_cast<int>(cbf_bytes) - 1; b >= 0; --b) if (bytes[b] != 0) { msb = b; break; }
    if (msb < 0) os << "  cbf_mask: 0x0" << std::endl;
    else {
        std::ostringstream hoss; hoss << std::hex << std::uppercase;
        for (int b = msb; b >= 0; --b) { hoss << std::setw(2) << std::setfill('0') << static_cast<unsigned>(bytes[b]); }
        os << "  cbf_mask: 0x" << hoss.str() << std::dec << std::nouppercase << std::endl;
    }

    os << "  cnt: " << p.cnt << ", state: " << static_cast<int>(p.state) << std::endl;
    os << "  data_len: " << p.data.size() << std::endl;
    if (!p.data.empty()) {
        os << "  data:" << std::endl;
        const size_t rows = 4, cols = 4; size_t total = p.data.size(); size_t idx = 0;
        for (size_t r = 0; r < rows; ++r) {
            os << "    [";
            for (size_t c = 0; c < cols; ++c) {
                if (idx < total) os << std::setw(4) << static_cast<int>(p.data[idx]); else os << std::setw(4) << 0;
                ++idx; if (c + 1 < cols) os << ",";
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
    const size_t meta_bytes = 10; // bs, qp_p, qp_q, flags, mv_p(3), mv_q(3)
    buf.reserve(15 + meta_bytes + partition_bytes + cbf_bytes + p.data.size());
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
    buf.push_back(static_cast<uint8_t>(p.cnt & 0xFF));
    buf.push_back(static_cast<uint8_t>((p.cnt >> 8) & 0xFF));
    buf.push_back(p.state);

    buf.push_back(p.bs);
    buf.push_back(p.qp_p);
    buf.push_back(p.qp_q);
    uint8_t flags = 0; flags |= (p.cbf_p ? 0x1u : 0x0u); flags |= (p.cbf_q ? 0x2u : 0x0u); flags |= (p.tu_edge ? 0x4u : 0x0u); flags |= (p.pu_edge ? 0x8u : 0x0u); flags |= (p.is_ver ? 0x10u : 0x0u);
    buf.push_back(flags);
    buf.push_back(static_cast<uint8_t>(p.mv_p & 0xFFu)); buf.push_back(static_cast<uint8_t>((p.mv_p >> 8) & 0xFFu)); buf.push_back(static_cast<uint8_t>((p.mv_p >> 16) & 0xFFu));
    buf.push_back(static_cast<uint8_t>(p.mv_q & 0xFFu)); buf.push_back(static_cast<uint8_t>((p.mv_q >> 8) & 0xFFu)); buf.push_back(static_cast<uint8_t>((p.mv_q >> 16) & 0xFFu));

    for (size_t byte = 0; byte < mb_part_bytes; ++byte) {
        uint8_t val = 0;
        for (size_t bit = 0; bit < 8; ++bit) { size_t idx = byte * 8 + bit; if (idx < 21 && p.mb_partition.test(idx)) val |= static_cast<uint8_t>(1u << bit); }
        buf.push_back(val);
    }
    for (size_t byte = 0; byte < mb_p_pu_bytes; ++byte) {
        uint8_t val = 0;
        for (size_t bit = 0; bit < 8; ++bit) { size_t idx = byte * 8 + bit; if (idx < 42 && p.mb_p_pu_mode.test(idx)) val |= static_cast<uint8_t>(1u << bit); }
        buf.push_back(val);
    }
    for (size_t byte = 0; byte < 32; ++byte) {
        uint8_t val = 0; for (size_t bit = 0; bit < 8; ++bit) { size_t idx = byte * 8 + bit; if (p.cbf_mask.test(idx)) val |= static_cast<uint8_t>(1u << bit); }
        buf.push_back(val);
    }
    if (!p.data.empty()) buf.insert(buf.end(), p.data.begin(), p.data.end());
    return buf;
}

inline CustomPacket unpackCustomPacket(const uint8_t *buf, size_t len) {
    CustomPacket p;
    if (!buf || len < 8) return p;
    p.cmd = static_cast<CustomCmd>(buf[0]); p.block_idx = buf[1]; p.x = buf[2]; p.y = buf[3]; p.size = buf[4]; p.sel = buf[5]; p.qp = buf[6]; p.pred_type = buf[7];
    size_t base = 8;
    if (len >= 12) { p.mode = buf[8]; p.pre_sel = buf[9]; p.i4x4_x = buf[10]; p.i4x4_y = buf[11]; base = 12; }
    if (len >= 15) { p.cnt = static_cast<uint16_t>(buf[12]) | (static_cast<uint16_t>(buf[13]) << 8); p.state = buf[14]; base = 15; }

    const size_t mb_part_bytes = (21 + 7) / 8; const size_t mb_p_pu_bytes = (42 + 7) / 8; const size_t partition_bytes = mb_part_bytes + mb_p_pu_bytes; const size_t cbf_bytes = 32; const size_t meta_bytes = 10;

    if (len >= base + meta_bytes + partition_bytes + cbf_bytes) {
        size_t mbase = base;
        p.bs = buf[mbase + 0]; p.qp_p = buf[mbase + 1]; p.qp_q = buf[mbase + 2]; uint8_t flags = buf[mbase + 3]; p.cbf_p = (flags & 0x1u) != 0; p.cbf_q = (flags & 0x2u) != 0; p.tu_edge = (flags & 0x4u) != 0; p.pu_edge = (flags & 0x8u) != 0; p.is_ver = (flags & 0x10u) != 0; p.mv_p = static_cast<uint32_t>(buf[mbase + 4]) | (static_cast<uint32_t>(buf[mbase + 5]) << 8) | (static_cast<uint32_t>(buf[mbase + 6]) << 16); p.mv_q = static_cast<uint32_t>(buf[mbase + 7]) | (static_cast<uint32_t>(buf[mbase + 8]) << 8) | (static_cast<uint32_t>(buf[mbase + 9]) << 16);
        size_t part_off = base + meta_bytes;
        for (size_t byte = 0; byte < mb_part_bytes; ++byte) { uint8_t val = buf[part_off + byte]; for (size_t bit = 0; bit < 8; ++bit) { size_t idx = byte * 8 + bit; if (idx < 21) { bool b = ((val >> bit) & 0x1u) != 0; if (b) p.mb_partition.set(idx); else p.mb_partition.reset(idx); } } }
        for (size_t byte = 0; byte < mb_p_pu_bytes; ++byte) { uint8_t val = buf[part_off + mb_part_bytes + byte]; for (size_t bit = 0; bit < 8; ++bit) { size_t idx = byte * 8 + bit; if (idx < 42) { bool b = ((val >> bit) & 0x1u) != 0; if (b) p.mb_p_pu_mode.set(idx); else p.mb_p_pu_mode.reset(idx); } } }
        size_t cbf_off = part_off + partition_bytes; for (size_t byte = 0; byte < cbf_bytes; ++byte) { uint8_t val = buf[cbf_off + byte]; for (size_t bit = 0; bit < 8; ++bit) { size_t idx = byte * 8 + bit; bool b = ((val >> bit) & 0x1u) != 0; if (b) p.cbf_mask.set(idx); else p.cbf_mask.reset(idx); } }
        size_t payload_len = (len > cbf_off + cbf_bytes) ? (len - (cbf_off + cbf_bytes)) : 0; if (payload_len) p.data.assign(buf + cbf_off + cbf_bytes, buf + cbf_off + cbf_bytes + payload_len);
    }
    else if (len >= base + partition_bytes + cbf_bytes) {
        for (size_t byte = 0; byte < mb_part_bytes; ++byte) { uint8_t val = buf[base + byte]; for (size_t bit = 0; bit < 8; ++bit) { size_t idx = byte * 8 + bit; if (idx < 21) { bool b = ((val >> bit) & 0x1u) != 0; if (b) p.mb_partition.set(idx); else p.mb_partition.reset(idx); } } }
        for (size_t byte = 0; byte < mb_p_pu_bytes; ++byte) { uint8_t val = buf[base + mb_part_bytes + byte]; for (size_t bit = 0; bit < 8; ++bit) { size_t idx = byte * 8 + bit; if (idx < 42) { bool b = ((val >> bit) & 0x1u) != 0; if (b) p.mb_p_pu_mode.set(idx); else p.mb_p_pu_mode.reset(idx); } } }
        size_t cbf_off = base + partition_bytes; for (size_t byte = 0; byte < cbf_bytes; ++byte) { uint8_t val = buf[cbf_off + byte]; for (size_t bit = 0; bit < 8; ++bit) { size_t idx = byte * 8 + bit; bool b = ((val >> bit) & 0x1u) != 0; if (b) p.cbf_mask.set(idx); else p.cbf_mask.reset(idx); } }
        size_t payload_len = (len > cbf_off + cbf_bytes) ? (len - (cbf_off + cbf_bytes)) : 0; if (payload_len) p.data.assign(buf + cbf_off + cbf_bytes, buf + cbf_off + cbf_bytes + payload_len);
    }
    else if (len >= base + cbf_bytes) {
        for (size_t byte = 0; byte < cbf_bytes; ++byte) { uint8_t val = buf[base + byte]; for (size_t bit = 0; bit < 8; ++bit) { size_t idx = byte * 8 + bit; bool b = ((val >> bit) & 0x1u) != 0; if (b) p.cbf_mask.set(idx); else p.cbf_mask.reset(idx); } }
        size_t payload_len = (len > base + cbf_bytes) ? (len - (base + cbf_bytes)) : 0; if (payload_len) p.data.assign(buf + base + cbf_bytes, buf + base + cbf_bytes + payload_len);
    }
    else { size_t payload_len = (len > base) ? (len - base) : 0; if (payload_len) p.data.assign(buf + base, buf + base + payload_len); }
    return p;
}

inline CustomPacket unpackCustomPacket(const tlm::tlm_generic_payload &trans) {
    const uint8_t *buf = reinterpret_cast<const uint8_t*>(trans.get_data_ptr());
    return unpackCustomPacket(buf, trans.get_data_length());
}

#endif
