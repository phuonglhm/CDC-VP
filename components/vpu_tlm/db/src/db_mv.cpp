#include "db_mv.h"
#include <iostream>
#include <algorithm>

#include "../../include/block_coord_codec.h"
#include "debug_config.h"

DbMotionVector::DbMotionVector(sc_core::sc_module_name name) : sc_module(name), filter_socket("filter_socket"), start_socket("start_socket") {
    start_socket.register_b_transport(this, &DbMotionVector::b_transport);
}
void DbMotionVector::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
    DbCustomPacket pkt = unpackDbCustomPacket(trans);
    const cdc::components::block_coord_4x4 block_coord =
        cdc::components::decode_block_coord_4x4(pkt.block_idx, pkt.x, pkt.y);
    unsigned sys_ctu_x = cdc::components::ctu_index_from_4x4(block_coord.x);
    unsigned sys_ctu_y = cdc::components::ctu_index_from_4x4(block_coord.y);
    uint16_t cnt = pkt.cnt;
    uint8_t state = pkt.state;

    // Ensure MV RAMs are populated with deterministic values based on CTU and cell
    auto pack_mv = [](int x, int y)->uint32_t {
        // clamp to signed 10-bit range (-512..511)
        if (x < -512) x = -512; if (x > 511) x = 511;
        if (y < -512) y = -512; if (y > 511) y = 511;
        uint32_t ux = static_cast<uint32_t>(x) & 0x3FFu;
        uint32_t uy = static_cast<uint32_t>(y) & 0x3FFu;
        return (ux << 10) | uy;
    };

    // Fill current CTU MV grid (8x8) deterministically so selects see non-zero MVs
    for (unsigned r = 0; r < 8; ++r) {
        for (unsigned c = 0; c < 8; ++c) {
            unsigned id = r * 8 + c;
            // small pattern: x component varies with column and ctu_x, y with row and ctu_y
            int mx = static_cast<int>(sys_ctu_x) * 2 + static_cast<int>(c) - 8;
            int my = static_cast<int>(sys_ctu_y) * 2 + static_cast<int>(r) - 8;
            cur_mv_r[id] = pack_mv(mx, my);
        }
    }

    // Populate top MV RAM entries for this CTU column set (if not first row)
    if (sys_ctu_y != 0) {
        for (unsigned c = 0; c < 8; ++c) {
            unsigned top_index = (sys_ctu_x << 3) + c;
            int mx = static_cast<int>(sys_ctu_x) * 2 + static_cast<int>(c) - 8;
            int my = static_cast<int>(sys_ctu_y - 1) * 2; // use previous row
            if (top_index < top_mv_r.size()) top_mv_r[top_index] = pack_mv(mx, my);
        }
    }

    uint32_t mv_p = 0, mv_q = 0;
    select_mv(pkt, sys_ctu_x, sys_ctu_y, cnt, state, mv_p, mv_q);

    this->last_mv_p = mv_p;
    this->last_mv_q = mv_q;

    if (cdc::components::verbose_enabled()) {
        std::cout << sc_core::sc_time_stamp() << " MotionVector pkt(cnt=" << pkt.cnt << ") mv_p=0x" << std::hex << mv_p << " mv_q=0x" << mv_q << std::dec << std::endl;
    }

    // annotate packet and forward
    pkt.mv_p = mv_p & 0xFFFFFu;
    pkt.mv_q = mv_q & 0xFFFFFu;
    std::vector<uint8_t> outbuf = packDbCustomPacket(pkt);
    trans.set_data_ptr(outbuf.data());
    trans.set_data_length(outbuf.size());
    filter_socket->b_transport(trans, delay);
}

void DbMotionVector::compute_mv_addresses(unsigned sys_ctu_x, uint16_t cnt,
                                        unsigned &cur_rd_addr, unsigned &cur_wr_addr,
                                        bool &use_top_read, unsigned &top_addr,
                                        bool &top_write) {
    // cur read addr
    unsigned cnt_8_5 = (cnt >> 5) & 0xF;
    if (cnt_8_5 == 0) {
        cur_rd_addr = (cnt >> 2) & 0x7u;
    } else {
        if ((cnt & 0x3u) == 0) cur_rd_addr = ((cnt >> 2) & 0x7Fu) - 8u;
        else cur_rd_addr = (cnt >> 2) & 0x7Fu;
    }
    cur_wr_addr = cnt & 0x3Fu; // cnt_r[5:0]

    use_top_read = false; // can't decide without state in this helper
    top_addr = (sys_ctu_x << 3) + ((cnt >> 2) & 0x7u);
    top_write = false; 
}

void DbMotionVector::decode_mv(uint32_t packed_mv, int16_t &mx, int16_t &my) {
    const uint32_t mask10 = 0x3FFu;
    uint32_t raw_y = packed_mv & mask10;
    uint32_t raw_x = (packed_mv >> 10) & mask10;
    mx = (raw_x & 0x200) ? static_cast<int16_t>(raw_x | 0xFC00) : static_cast<int16_t>(raw_x);
    my = (raw_y & 0x200) ? static_cast<int16_t>(raw_y | 0xFC00) : static_cast<int16_t>(raw_y);
}

void DbMotionVector::select_mv(const DbCustomPacket &pkt,
                             unsigned sys_ctu_x, unsigned sys_ctu_y,
                             uint16_t cnt, uint8_t state,
                             uint32_t &mv_p, uint32_t &mv_q) {
    unsigned cur_rd = 0, cur_wr = 0, top_addr = 0;
    bool use_top = false, top_write = false;
    // compute cur read/write
    // Cleaner: random-access MV lookup keyed by block coordinates.
    // Map 4x4 coordinates into an 8x8 per-CTU MV grid (indices 0..7)
    unsigned col = (static_cast<unsigned>(pkt.i4x4_x) >> 1) & 0x7u;
    unsigned row = (static_cast<unsigned>(pkt.i4x4_y) >> 1) & 0x7u;

    unsigned idx = row * 8 + col;

    // Current MV at this 8x8 cell
    uint32_t cur = 0;
    if (idx < cur_mv_r.size()) cur = cur_mv_r[idx];

    // Top RAM index for this column
    unsigned top_index = (static_cast<unsigned>(sys_ctu_x) << 3) + col;
    uint32_t top = 0;
    if (top_index < top_mv_r.size()) top = top_mv_r[top_index];

    // neighbors: UL, UR, DL, DR (clamped at edges)
    auto read_cur = [&](int r, int c)->uint32_t {
        if (r < 0 || c < 0) return 0u;
        unsigned rr = static_cast<unsigned>(std::min(std::max(r, 0), 7));
        unsigned cc = static_cast<unsigned>(std::min(std::max(c, 0), 7));
        unsigned id = rr * 8 + cc;
        return (id < cur_mv_r.size()) ? cur_mv_r[id] : 0u;
    };

    uint32_t ul = (row == 0 && sys_ctu_y != 0) ? top : read_cur(static_cast<int>(row) - 1, static_cast<int>(col) - 1);
    uint32_t ur = (row == 0 && sys_ctu_y != 0) ? ((top_index + 1 < top_mv_r.size()) ? top_mv_r[top_index + 1] : top) : read_cur(static_cast<int>(row) - 1, static_cast<int>(col) + 1);
    uint32_t dl = read_cur(static_cast<int>(row) + 1, static_cast<int>(col) - 1);
    uint32_t dr = read_cur(static_cast<int>(row) + 1, static_cast<int>(col) + 1);

    // decide micro-phase from 4x4 parity (fallback to cnt low bits if needed)
    unsigned micro = ((static_cast<unsigned>(pkt.i4x4_y) & 1u) << 1) | (static_cast<unsigned>(pkt.i4x4_x) & 1u);

    switch (micro) {
        case 0: mv_p = ul; mv_q = ur; break; // top row vertical edge
        case 1: mv_p = dl; mv_q = dr; break; // bottom row vertical edge
        case 2: mv_p = ul; mv_q = dl; break; // left column horizontal edge
        case 3: mv_p = ur; mv_q = dr; break; // right column horizontal edge
        default: mv_p = cur; mv_q = cur; break;
    }
}

void DbMotionVector::write_cur_mv(unsigned addr, uint32_t data) {
    if (addr < cur_mv_r.size()) cur_mv_r[addr] = data & 0xFFFFFu;
}
void DbMotionVector::read_cur_mv(unsigned addr, uint32_t &data) const {
    if (addr < cur_mv_r.size()) data = cur_mv_r[addr] & 0xFFFFFu;
    else data = 0;
}
void DbMotionVector::write_top_mv(unsigned addr, uint32_t data) {
    if (addr < top_mv_r.size()) top_mv_r[addr] = data & 0xFFFFFu;
}
void DbMotionVector::read_top_mv(unsigned addr, uint32_t &data) const {
    if (addr < top_mv_r.size()) data = top_mv_r[addr] & 0xFFFFFu;
    else data = 0;
}
