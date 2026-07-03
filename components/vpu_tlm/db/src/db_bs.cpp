#include "db_bs.h"
#include "custom_packet.h"
#include <iostream>
#include <vector>

BorderStrength::BorderStrength(sc_core::sc_module_name name) : sc_module(name), filter_socket("filter_socket"), start_socket("start_socket") {
    start_socket.register_b_transport(this, &BorderStrength::b_transport);
}

void BorderStrength::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
    CustomPacket pkt = unpackCustomPacket(trans);
    std::cout << sc_core::sc_time_stamp() << "---------------BorderStrength Packet (internal)--------------\n" << pkt << std::endl;
    unsigned sys_ctu_x = pkt.x;
    unsigned sys_ctu_y = pkt.y;
    uint16_t cnt = pkt.cnt;
    uint8_t state = pkt.state;
    bool tu_edge = false, pu_edge = false;
    edge_detect(pkt, sys_ctu_x, sys_ctu_y, cnt, state, tu_edge, pu_edge);

    bool cbf_p = false, cbf_q = false;
    cbf_select(pkt, sys_ctu_x, sys_ctu_y, cnt, state, cbf_p, cbf_q);

    uint8_t qp_p = 0, qp_q = 0;
    select_qp(pkt, sys_ctu_x, sys_ctu_y, cnt, state, qp_p, qp_q);

    std::cout << "  tu_edge=" << tu_edge << " pu_edge=" << pu_edge << " cbf_p=" << cbf_p << " cbf_q=" << cbf_q << " qp_p=" << static_cast<int>(qp_p) << " qp_q=" << static_cast<int>(qp_q) << std::endl;

    // forward transaction unchanged for now
    filter_socket->b_transport(trans, delay);
}

void BorderStrength::compute_tu_masks(const std::bitset<21> &mb_partition, EdgeMasks &out) {
    // Translate db_tu_edge.v combinational mapping.
    out.v.fill(0);
    out.h.fill(0);

    auto set_v = [&](int idx, int bit, bool v) {
        if (v) out.v[idx] |= static_cast<uint8_t>(1u << bit);
        else out.v[idx] &= static_cast<uint8_t>(~(1u << bit));
    };
    auto set_h_nibble = [&](int hidx, int nib, bool v) {
        uint16_t mask = static_cast<uint16_t>(0xFu << (nib * 4));
        uint16_t val = static_cast<uint16_t>((v ? 0xFu : 0x0u) << (nib * 4));
        out.h[hidx] = static_cast<uint16_t>((out.h[hidx] & ~mask) | val);
    };

    // v0..v3: bits [0,2,4,6] and h1 nibbles
    bool c0 = mb_partition.test(0) && mb_partition.test(1) && mb_partition.test(5);
    for (int i = 0; i < 4; ++i) set_v(i, 0, c0);
    set_h_nibble(0, 0, c0);

    bool c1 = mb_partition.test(0) && mb_partition.test(1) && mb_partition.test(6);
    for (int i = 0; i < 4; ++i) set_v(i, 2, c1);
    set_h_nibble(0, 1, c1);

    bool c2 = mb_partition.test(0) && mb_partition.test(2) && mb_partition.test(9);
    for (int i = 0; i < 4; ++i) set_v(i, 4, c2);
    set_h_nibble(0, 2, c2);

    bool c3 = mb_partition.test(0) && mb_partition.test(2) && mb_partition.test(10);
    for (int i = 0; i < 4; ++i) set_v(i, 6, c3);
    set_h_nibble(0, 3, c3);

    // v4..v7 -> h3
    bool c4 = mb_partition.test(0) && mb_partition.test(1) && mb_partition.test(7);
    for (int i = 4; i < 8; ++i) set_v(i, 0, c4);
    set_h_nibble(2, 0, c4);

    bool c5 = mb_partition.test(0) && mb_partition.test(1) && mb_partition.test(8);
    for (int i = 4; i < 8; ++i) set_v(i, 2, c5);
    set_h_nibble(2, 1, c5);

    bool c6 = mb_partition.test(0) && mb_partition.test(2) && mb_partition.test(11);
    for (int i = 4; i < 8; ++i) set_v(i, 4, c6);
    set_h_nibble(2, 2, c6);

    bool c7 = mb_partition.test(0) && mb_partition.test(2) && mb_partition.test(12);
    for (int i = 4; i < 8; ++i) set_v(i, 6, c7);
    set_h_nibble(2, 3, c7);

    // v8..v11 -> h5
    bool c8  = mb_partition.test(0) && mb_partition.test(3) && mb_partition.test(13);
    for (int i = 8; i < 12; ++i) set_v(i, 0, c8);
    set_h_nibble(4, 0, c8);

    bool c9  = mb_partition.test(0) && mb_partition.test(3) && mb_partition.test(14);
    for (int i = 8; i < 12; ++i) set_v(i, 2, c9);
    set_h_nibble(4, 1, c9);

    bool c10 = mb_partition.test(0) && mb_partition.test(4) && mb_partition.test(17);
    for (int i = 8; i < 12; ++i) set_v(i, 4, c10);
    set_h_nibble(4, 2, c10);

    bool c11 = mb_partition.test(0) && mb_partition.test(4) && mb_partition.test(18);
    for (int i = 8; i < 12; ++i) set_v(i, 6, c11);
    set_h_nibble(4, 3, c11);

    // v12..v15 -> h7
    bool c12 = mb_partition.test(0) && mb_partition.test(3) && mb_partition.test(15);
    for (int i = 12; i < 16; ++i) set_v(i, 0, c12);
    set_h_nibble(6, 0, c12);

    bool c13 = mb_partition.test(0) && mb_partition.test(3) && mb_partition.test(16);
    for (int i = 12; i < 16; ++i) set_v(i, 2, c13);
    set_h_nibble(6, 1, c13);

    bool c14 = mb_partition.test(0) && mb_partition.test(4) && mb_partition.test(19);
    for (int i = 12; i < 16; ++i) set_v(i, 4, c14);
    set_h_nibble(6, 2, c14);

    bool c15 = mb_partition.test(0) && mb_partition.test(4) && mb_partition.test(20);
    for (int i = 12; i < 16; ++i) set_v(i, 6, c15);
    set_h_nibble(6, 3, c15);

    // 16x16 groups: bits 1 and 5
    bool g1 = mb_partition.test(0) && mb_partition.test(1);
    for (int i = 0; i < 8; ++i) set_v(i, 1, g1);

    bool g2 = mb_partition.test(0) && mb_partition.test(2);
    for (int i = 0; i < 8; ++i) set_v(i, 5, g2);

    bool g3 = mb_partition.test(0) && mb_partition.test(3);
    for (int i = 8; i < 16; ++i) set_v(i, 1, g3);

    bool g4 = mb_partition.test(0) && mb_partition.test(4);
    for (int i = 8; i < 16; ++i) set_v(i, 5, g4);

    // bit3 set (32x32 tu edge) - per RTL it's always 1
    for (int i = 0; i < 16; ++i) set_v(i, 3, true);

    // h2/h6 and h4
    // h2 low byte
    out.h[1] = 0;
    if (mb_partition.test(0) && mb_partition.test(1)) out.h[1] |= 0x00FFu;
    // h2 high byte
    if (mb_partition.test(0) && mb_partition.test(2)) out.h[1] |= 0xFF00u;

    // h6 low/high
    out.h[5] = 0;
    if (mb_partition.test(0) && mb_partition.test(3)) out.h[5] |= 0x00FFu;
    if (mb_partition.test(0) && mb_partition.test(4)) out.h[5] |= 0xFF00u;

    // h4 always 0xffff per RTL
    out.h[3] = 0xFFFFu;
}

void BorderStrength::compute_pu_masks(const std::bitset<21> &mb_partition, const std::bitset<42> &mb_p_pu_mode, EdgeMasks &out) {
    // Translate db_pu_edge.v combinational mapping
    out.v.fill(0);
    out.h.fill(0);

    auto set_v = [&](int idx, int bit, bool v) {
        if (v) out.v[idx] |= static_cast<uint8_t>(1u << bit);
        else out.v[idx] &= static_cast<uint8_t>(~(1u << bit));
    };
    auto set_h_nibble = [&](int hidx, int nib, bool v) {
        uint16_t mask = static_cast<uint16_t>(0xFu << (nib * 4));
        uint16_t val = static_cast<uint16_t>((v ? 0xFu : 0x0u) << (nib * 4));
        out.h[hidx] = static_cast<uint16_t>((out.h[hidx] & ~mask) | val);
    };

    // h1 group (v0..v3)
    for (int seg = 0; seg < 4; ++seg) {
        int pidx0 = 0; // top-level checks alternate per nibble
        // mapping per nibble: segments -> partition indices and mb_p_pu_mode positions
        int part_a = 0, part_b = 0, part_c = 0;
        int pu_high = 0, pu_low = 0; // bit positions for mb_p_pu_mode
        if (seg == 0) { part_a = 0; part_b = 1; part_c = 5; pu_low = 10; pu_high = 11; }
        if (seg == 1) { part_a = 0; part_b = 1; part_c = 6; pu_low = 12; pu_high = 13; }
        if (seg == 2) { part_a = 0; part_b = 2; part_c = 9; pu_low = 18; pu_high = 19; }
        if (seg == 3) { part_a = 0; part_b = 2; part_c = 10; pu_low = 20; pu_high = 21; }

        bool v_all_8x8 = mb_partition.test(part_a) && mb_partition.test(part_b) && mb_partition.test(part_c);
        bool base_ok = mb_partition.test(part_a) && mb_partition.test(part_b);
        // v bits indices 0,2,4,6 for the first four (v0..v3)
        for (int i = 0; i < 4; ++i) {
            if (!mb_partition.test(part_a) || !mb_partition.test(part_b)) {
                set_v(i, seg*2, false);
            } else if (mb_partition.test(part_c)) {
                set_v(i, seg*2, true);
            } else {
                // 16x16: consult mb_p_pu_mode[pu_high:pu_low]
                int bit0 = mb_p_pu_mode.test(pu_low) ? 1 : 0;
                int bit1 = mb_p_pu_mode.test(pu_high) ? 1 : 0;
                int code = (bit1 << 1) | bit0;
                bool vbit = (code == 2 || code == 3) ? true : false;
                set_v(i, seg*2, vbit);
            }
        }
        // h1 nibble
        if (!mb_partition.test(part_a) || !mb_partition.test(part_b)) set_h_nibble(0, seg, false);
        else if (mb_partition.test(part_c)) set_h_nibble(0, seg, true);
        else {
            int bit0 = mb_p_pu_mode.test(pu_low) ? 1 : 0;
            int bit1 = mb_p_pu_mode.test(pu_high) ? 1 : 0;
            int code = (bit1 << 1) | bit0;
            bool nib = (code == 1 || code == 3) ? true : false; // 01 or 11 -> 0xF
            set_h_nibble(0, seg, nib);
        }
    }

    // h3 group (v4..v7)
    for (int seg = 0; seg < 4; ++seg) {
        int p_a, p_b, p_c, pu_low, pu_high;
        if (seg==0) { p_a=0; p_b=1; p_c=7; pu_low=14; pu_high=15; }
        if (seg==1) { p_a=0; p_b=1; p_c=8; pu_low=16; pu_high=17; }
        if (seg==2) { p_a=0; p_b=2; p_c=11; pu_low=22; pu_high=23; }
        if (seg==3) { p_a=0; p_b=2; p_c=12; pu_low=24; pu_high=25; }
        for (int i = 4; i < 8; ++i) {
            if (!mb_partition.test(p_a) || !mb_partition.test(p_b)) set_v(i, seg*2, false);
            else if (mb_partition.test(p_c)) set_v(i, seg*2, true);
            else {
                int bit0 = mb_p_pu_mode.test(pu_low) ? 1 : 0;
                int bit1 = mb_p_pu_mode.test(pu_high) ? 1 : 0;
                int code = (bit1<<1)|bit0; bool vbit = (code==2||code==3);
                set_v(i, seg*2, vbit);
            }
        }
        if (!mb_partition.test(p_a) || !mb_partition.test(p_b)) set_h_nibble(2, seg, false);
        else if (mb_partition.test(p_c)) set_h_nibble(2, seg, true);
        else { int b0=mb_p_pu_mode.test(pu_low)?1:0; int b1=mb_p_pu_mode.test(pu_high)?1:0; int code=(b1<<1)|b0; set_h_nibble(2, seg, (code==1||code==3)); }
    }

    // h5 group (v8..v11)
    for (int seg = 0; seg < 4; ++seg) {
        int p_a,p_b,p_c,pu_low,pu_high;
        if (seg==0){p_a=0;p_b=3;p_c=13;pu_low=26;pu_high=27;}
        if (seg==1){p_a=0;p_b=3;p_c=14;pu_low=28;pu_high=29;}
        if (seg==2){p_a=0;p_b=4;p_c=17;pu_low=34;pu_high=35;}
        if (seg==3){p_a=0;p_b=4;p_c=18;pu_low=36;pu_high=37;}
        for (int i = 8; i < 12; ++i) {
            if (!mb_partition.test(p_a) || !mb_partition.test(p_b)) set_v(i, seg*2, false);
            else if (mb_partition.test(p_c)) set_v(i, seg*2, true);
            else { int b0=mb_p_pu_mode.test(pu_low)?1:0; int b1=mb_p_pu_mode.test(pu_high)?1:0; int code=(b1<<1)|b0; set_v(i, seg*2, (code==2||code==3)); }
        }
        if (!mb_partition.test(p_a) || !mb_partition.test(p_b)) set_h_nibble(4, seg, false);
        else if (mb_partition.test(p_c)) set_h_nibble(4, seg, true);
        else { int b0=mb_p_pu_mode.test(pu_low)?1:0; int b1=mb_p_pu_mode.test(pu_high)?1:0; int code=(b1<<1)|b0; set_h_nibble(4, seg, (code==1||code==3)); }
    }

    // h7 group (v12..v15)
    for (int seg = 0; seg < 4; ++seg) {
        int p_a,p_b,p_c,pu_low,pu_high;
        if (seg==0){p_a=0;p_b=3;p_c=15;pu_low=30;pu_high=31;}
        if (seg==1){p_a=0;p_b=3;p_c=16;pu_low=32;pu_high=33;}
        if (seg==2){p_a=0;p_b=4;p_c=19;pu_low=38;pu_high=39;}
        if (seg==3){p_a=0;p_b=4;p_c=20;pu_low=40;pu_high=41;}
        for (int i = 12; i < 16; ++i) {
            if (!mb_partition.test(p_a) || !mb_partition.test(p_b)) set_v(i, seg*2, false);
            else if (mb_partition.test(p_c)) set_v(i, seg*2, true);
            else { int b0=mb_p_pu_mode.test(pu_low)?1:0; int b1=mb_p_pu_mode.test(pu_high)?1:0; int code=(b1<<1)|b0; set_v(i, seg*2, (code==2||code==3)); }
        }
        if (!mb_partition.test(p_a) || !mb_partition.test(p_b)) set_h_nibble(6, seg, false);
        else if (mb_partition.test(p_c)) set_h_nibble(6, seg, true);
        else { int b0=mb_p_pu_mode.test(pu_low)?1:0; int b1=mb_p_pu_mode.test(pu_high)?1:0; int code=(b1<<1)|b0; set_h_nibble(6, seg, (code==1||code==3)); }
    }

    if (!mb_partition.test(0)) out.h[1] &= 0xFF00u; // leave high
    else if (!mb_partition.test(1)) {
        int b0 = mb_p_pu_mode.test(2) ? 1 : 0; int b1 = mb_p_pu_mode.test(3) ? 1:0; int code=(b1<<1)|b0;
        out.h[1] |= (code==1 || code==3) ? 0x00FFu : 0x0000u;
        if (code==2 || code==3) for (int i=0;i<8;++i) set_v(i,1,true); else for (int i=0;i<8;++i) set_v(i,1,false);
    } else {
        out.h[1] |= 0x00FFu; for (int i=0;i<8;++i) set_v(i,1,true);
    }
    // h2 high
    if (!mb_partition.test(0)) out.h[1] &= 0x00FFu;
    else if (!mb_partition.test(2)) {
        int b0 = mb_p_pu_mode.test(4)?1:0; int b1 = mb_p_pu_mode.test(5)?1:0; int code=(b1<<1)|b0;
        out.h[1] |= (code==1 || code==3) ? 0xFF00u : 0x0000u;
        if (code==2 || code==3) for (int i=0;i<8;++i) set_v(i,5,true); else for (int i=0;i<8;++i) set_v(i,5,false);
    } else { out.h[1] |= 0xFF00u; for (int i=0;i<8;++i) set_v(i,5,true); }

    // h6 low/high (pu side)
    if (!mb_partition.test(0)) out.h[5] &= 0xFF00u; else if (!mb_partition.test(3)) {
        int b0=mb_p_pu_mode.test(6)?1:0; int b1=mb_p_pu_mode.test(7)?1:0; int code=(b1<<1)|b0; out.h[5] |= (code==1||code==3)?0x00FFu:0x0000u; if (code==2||code==3) for (int i=8;i<16;++i) set_v(i,1,true); else for (int i=8;i<16;++i) set_v(i,1,false);
    } else { out.h[5] |= 0x00FFu; for (int i=8;i<16;++i) set_v(i,1,true); }

    if (!mb_partition.test(0)) out.h[5] &= 0x00FFu; else if (!mb_partition.test(4)) {
        int b0=mb_p_pu_mode.test(8)?1:0; int b1=mb_p_pu_mode.test(9)?1:0; int code=(b1<<1)|b0; out.h[5] |= (code==1||code==3)?0xFF00u:0x0000u; if (code==2||code==3) for (int i=8;i<16;++i) set_v(i,5,true); else for (int i=8;i<16;++i) set_v(i,5,false);
    } else { out.h[5] |= 0xFF00u; for (int i=8;i<16;++i) set_v(i,5,true); }

    // h4 mapping (32x32 pu edge): handled later by higher-level selection; default to 0xffff when top-level split
    out.h[3] = 0xFFFFu;
}


void BorderStrength::edge_detect(const CustomPacket &pkt,
                    unsigned sys_ctu_x, unsigned sys_ctu_y,
                    uint16_t cnt, uint8_t state,
                    bool &tu_edge, bool &pu_edge) {
    EdgeMasks tu_m, pu_m;
    compute_tu_masks(pkt.mb_partition, tu_m);
    compute_pu_masks(pkt.mb_partition, pkt.mb_p_pu_mode, pu_m);

    // blocks_per_tu = number of 4x4 blocks per TU along one axis (1,2,4,8)
    unsigned blocks_per_tu = 1u << (pkt.size & 0x3);
    unsigned x4 = static_cast<unsigned>(pkt.i4x4_x) & 0xF;
    unsigned y4 = static_cast<unsigned>(pkt.i4x4_y) & 0xF;

    bool at_vert_tu = ((x4 + 1) % blocks_per_tu) == 0;
    bool at_hori_tu = ((y4 + 1) % blocks_per_tu) == 0;
    tu_edge = at_vert_tu || at_hori_tu;

    bool pu_v = pu_m.v[x4 % 16];
    bool pu_h = (pu_m.h[(y4 / 2) % 8] != 0);
    pu_edge = pu_v || pu_h || tu_edge;
}

void BorderStrength::cbf_select(const CustomPacket &pkt,
                    unsigned sys_ctu_x, unsigned sys_ctu_y,
                    uint16_t cnt, uint8_t state,
                    bool &cbf_p, bool &cbf_q) {
    auto bit = [&](int idx)->int { return pkt.cbf_mask.test(idx) ? 1 : 0; };

    // build cbf_top_w per RTL ordering (MSB..LSB as in db_bs.v)
    const int top_map[16] = {255,254,251,250,239,238,235,234,191,190,187,186,175,174,171,170};
    uint16_t cbf_top_w = 0;
    for (int i = 0; i < 16; ++i) {
        cbf_top_w = static_cast<uint16_t>((cbf_top_w << 1) | (bit(top_map[i]) & 1));
    }

    // read top register (emulate RAM read): if first row, top is zero
    uint16_t top_reg = (sys_ctu_y != 0) ? cbf_top_r[sys_ctu_x & 0x3F] : 0u;

    // helper to pack 17 bits (first element -> MSB)
    auto pack17 = [&](const std::vector<int> &vals)->uint32_t {
        uint32_t out = 0;
        for (size_t i = 0; i < vals.size(); ++i) out = (out << 1) | (vals[i] & 1);
        return out;
    };

    uint32_t cbf_h_p[8] = {0}, cbf_h_q[8] = {0};

    // cbf_h0_p = {cbf_tl, top_reg[0], top_reg[1], ..., top_reg[15]}
    {
        std::vector<int> v;
        v.push_back(static_cast<int>(cbf_tl & 1));
        for (int i = 0; i < 16; ++i) v.push_back((top_reg >> i) & 1);
        cbf_h_p[0] = pack17(v);
    }

    // Explicit mapping for p and q groups (match db_bs.v)
    cbf_h_p[1] = pack17(std::vector<int>{ static_cast<int>(cbf_left[1] & 1), bit(2),bit(3),bit(6),bit(7),bit(18),bit(19),bit(22),bit(23),bit(66),bit(67),bit(70),bit(71),bit(82),bit(83),bit(86),bit(87) });
    cbf_h_p[2] = pack17(std::vector<int>{ static_cast<int>(cbf_left[3] & 1), bit(10),bit(11),bit(14),bit(15),bit(26),bit(27),bit(30),bit(31),bit(74),bit(75),bit(78),bit(79),bit(90),bit(91),bit(94),bit(95) });
    cbf_h_p[3] = pack17(std::vector<int>{ static_cast<int>(cbf_left[5] & 1), bit(34),bit(35),bit(38),bit(39),bit(50),bit(51),bit(54),bit(55),bit(98),bit(99),bit(102),bit(103),bit(114),bit(115),bit(118),bit(119) });
    cbf_h_p[4] = pack17(std::vector<int>{ static_cast<int>(cbf_left[7] & 1), bit(42),bit(43),bit(46),bit(47),bit(58),bit(59),bit(62),bit(63),bit(106),bit(107),bit(110),bit(111),bit(122),bit(123),bit(126),bit(127) });
    cbf_h_p[5] = pack17(std::vector<int>{ static_cast<int>(cbf_left[9] & 1), bit(130),bit(131),bit(134),bit(135),bit(146),bit(147),bit(150),bit(151),bit(194),bit(195),bit(198),bit(199),bit(210),bit(211),bit(214),bit(215) });
    cbf_h_p[6] = pack17(std::vector<int>{ static_cast<int>(cbf_left[11] & 1), bit(138),bit(139),bit(142),bit(143),bit(154),bit(155),bit(158),bit(159),bit(202),bit(203),bit(206),bit(207),bit(218),bit(219),bit(222),bit(223) });
    cbf_h_p[7] = pack17(std::vector<int>{ static_cast<int>(cbf_left[13] & 1), bit(162),bit(163),bit(166),bit(167),bit(178),bit(179),bit(182),bit(183),bit(226),bit(227),bit(230),bit(231),bit(242),bit(243),bit(246),bit(247) });

    cbf_h_q[0] = pack17(std::vector<int>{ static_cast<int>(cbf_left[0] & 1), bit(0),bit(1),bit(4),bit(5),bit(16),bit(17),bit(20),bit(21),bit(64),bit(65),bit(68),bit(69),bit(80),bit(81),bit(84),bit(85) });
    cbf_h_q[1] = pack17(std::vector<int>{ static_cast<int>(cbf_left[2] & 1), bit(8),bit(9),bit(12),bit(13),bit(24),bit(25),bit(28),bit(29),bit(72),bit(73),bit(76),bit(77),bit(88),bit(89),bit(92),bit(93) });
    cbf_h_q[2] = pack17(std::vector<int>{ static_cast<int>(cbf_left[4] & 1), bit(32),bit(33),bit(36),bit(37),bit(48),bit(49),bit(52),bit(53),bit(96),bit(97),bit(100),bit(101),bit(112),bit(113),bit(116),bit(117) });
    cbf_h_q[3] = pack17(std::vector<int>{ static_cast<int>(cbf_left[6] & 1), bit(40),bit(41),bit(44),bit(45),bit(56),bit(57),bit(60),bit(61),bit(104),bit(105),bit(108),bit(109),bit(120),bit(121),bit(124),bit(125) });
    cbf_h_q[4] = pack17(std::vector<int>{ static_cast<int>(cbf_left[8] & 1), bit(128),bit(129),bit(132),bit(133),bit(144),bit(145),bit(148),bit(149),bit(192),bit(193),bit(196),bit(197),bit(208),bit(209),bit(212),bit(213) });
    cbf_h_q[5] = pack17(std::vector<int>{ static_cast<int>(cbf_left[10] & 1), bit(136),bit(137),bit(140),bit(141),bit(152),bit(153),bit(156),bit(157),bit(200),bit(201),bit(204),bit(205),bit(216),bit(217),bit(220),bit(221) });
    cbf_h_q[6] = pack17(std::vector<int>{ static_cast<int>(cbf_left[12] & 1), bit(160),bit(161),bit(164),bit(165),bit(176),bit(177),bit(180),bit(181),bit(224),bit(225),bit(228),bit(229),bit(240),bit(241),bit(244),bit(245) });
    cbf_h_q[7] = pack17(std::vector<int>{ static_cast<int>(cbf_left[14] & 1), bit(168),bit(169),bit(172),bit(173),bit(184),bit(185),bit(188),bit(189),bit(232),bit(233),bit(236),bit(237),bit(248),bit(249),bit(252),bit(253) });

    const int DBY = 3, DBU = 2, DBV = 6, OUT = 5, LOAD = 1;
    bool y_flag = (state == DBY) && (((cnt >> 8) & 1) == 0);
    bool u_flag = (state == DBU) && (((cnt >> 6) & 1) == 0);
    bool v_flag = (state == DBV) && (((cnt >> 6) & 1) == 0);
    bool top_left_flag = ((state == DBY) || (state == DBU) || (state == DBV)) && (((cnt >> 2) & 0x7F) == 0);
    bool top_flag = (y_flag && (((cnt >> 5) & 0xF) == 0)) || (u_flag && (((cnt >> 4) & 0x3) == 0)) || (v_flag && (((cnt >> 4) & 0x3) == 0));
    bool left_flag = (y_flag && (((cnt >> 2) & 0x7) == 0)) || (u_flag && (((cnt >> 2) & 0x3) == 0)) || (v_flag && (((cnt >> 2) & 0x3) == 0));

    // choose cbf_flag_up/dn
    uint32_t cbf_flag_up = 0, cbf_flag_dn = 0;
    if (y_flag) {
        int sel = (cnt >> 5) & 0x7;
        if (sel >=0 && sel < 8) { cbf_flag_up = cbf_h_p[sel]; cbf_flag_dn = cbf_h_q[sel]; }
    }

    // pick ul/ur/dl/dr depending on cnt[4:2]
    bool cbf_ul = false, cbf_ur = false, cbf_dl = false, cbf_dr = false;
    int sel2 = (cnt >> 2) & 0x7;
    if (y_flag) {
        switch (sel2) {
            case 0: cbf_ul = ((cbf_flag_up >> 16) & 1); cbf_ur = ((cbf_flag_up >> 15) & 1); cbf_dl = ((cbf_flag_dn >> 16) & 1); cbf_dr = ((cbf_flag_dn >> 15) & 1); break;
            case 1: cbf_ul = ((cbf_flag_up >> 14) & 1); cbf_ur = ((cbf_flag_up >> 13) & 1); cbf_dl = ((cbf_flag_dn >> 14) & 1); cbf_dr = ((cbf_flag_dn >> 13) & 1); break;
            case 2: cbf_ul = ((cbf_flag_up >> 12) & 1); cbf_ur = ((cbf_flag_up >> 11) & 1); cbf_dl = ((cbf_flag_dn >> 12) & 1); cbf_dr = ((cbf_flag_dn >> 11) & 1); break;
            case 3: cbf_ul = ((cbf_flag_up >> 10) & 1); cbf_ur = ((cbf_flag_up >> 9) & 1); cbf_dl = ((cbf_flag_dn >> 10) & 1); cbf_dr = ((cbf_flag_dn >> 9) & 1); break;
            case 4: cbf_ul = ((cbf_flag_up >> 8) & 1); cbf_ur = ((cbf_flag_up >> 7) & 1); cbf_dl = ((cbf_flag_dn >> 8) & 1); cbf_dr = ((cbf_flag_dn >> 7) & 1); break;
            case 5: cbf_ul = ((cbf_flag_up >> 6) & 1); cbf_ur = ((cbf_flag_up >> 5) & 1); cbf_dl = ((cbf_flag_dn >> 6) & 1); cbf_dr = ((cbf_flag_dn >> 5) & 1); break;
            case 6: cbf_ul = ((cbf_flag_up >> 4) & 1); cbf_ur = ((cbf_flag_up >> 3) & 1); cbf_dl = ((cbf_flag_dn >> 4) & 1); cbf_dr = ((cbf_flag_dn >> 3) & 1); break;
            case 7: cbf_ul = ((cbf_flag_up >> 2) & 1); cbf_ur = ((cbf_flag_up >> 1) & 1); cbf_dl = ((cbf_flag_dn >> 2) & 1); cbf_dr = ((cbf_flag_dn >> 1) & 1); break;
            default: cbf_ul = cbf_ur = cbf_dl = cbf_dr = false; break;
        }
    }

    // select based on cnt[1:0]
    int sel3 = cnt & 0x3;
    switch (sel3) {
        case 0: cbf_p = cbf_ul; cbf_q = cbf_ur; break;
        case 1: cbf_p = cbf_dl; cbf_q = cbf_dr; break;
        case 2: cbf_p = cbf_ul; cbf_q = cbf_dl; break;
        case 3: cbf_p = cbf_ur; cbf_q = cbf_dr; break;
    }

    // update left/top memories on OUT state similar to RTL semantics
    if (state == OUT) {
        // cbf_tl <= previous top_reg[15]
        cbf_tl = static_cast<uint8_t>((top_reg >> 15) & 1);
        cbf_top_r[sys_ctu_x & 0x3F] = cbf_top_w;
        const int left_map[16] = {85,87,93,95,117,119,125,127,213,215,221,223,245,247,253,255};
        for (int i = 0; i < 16; ++i) cbf_left[i] = static_cast<uint8_t>(bit(left_map[i]));
    }
}

void BorderStrength::select_qp(const CustomPacket &pkt,
                   unsigned sys_ctu_x, unsigned sys_ctu_y,
                   uint16_t cnt, uint8_t state,
                   uint8_t &qp_p, uint8_t &qp_q) {
    const int DBY = 3, DBU = 2, DBV = 6, OUT = 5, LOAD = 1;
    bool y_flag = (state == DBY) && (((cnt >> 8) & 1) == 0);
    bool u_flag = (state == DBU) && (((cnt >> 6) & 1) == 0);
    bool v_flag = (state == DBV) && (((cnt >> 6) & 1) == 0);
    bool top_left_flag = ((state == DBY) || (state == DBU) || (state == DBV)) && (((cnt >> 2) & 0x7F) == 0);
    bool top_flag = (y_flag && (((cnt >> 5) & 0xF) == 0)) || (u_flag && (((cnt >> 4) & 0x3) == 0)) || (v_flag && (((cnt >> 4) & 0x3) == 0));
    bool left_flag = (y_flag && (((cnt >> 2) & 0x7) == 0)) || (u_flag && (((cnt >> 2) & 0x3) == 0)) || (v_flag && (((cnt >> 2) & 0x3) == 0));

    // compute qp_flag chain (db_qp instantiation order => indices 0,4,8,...,252)
    std::array<bool,64> qp_flag{};
    bool cbf_any = pkt.cbf_mask.any();
    bool qp_first = ((sys_ctu_x != 0) || (sys_ctu_y != 0)) ? (pkt.mb_partition.test(0) || !cbf_any) : false;
    bool prev = qp_first;
    for (int i = 0; i < 64; ++i) {
        int cbf_idx = i * 4;
        bool cbf_y = pkt.cbf_mask.test(cbf_idx);
        bool cbf_u = false;
        bool cbf_v = false;
        bool modified_flag = !(cbf_y || cbf_u || cbf_v);
        bool qf = modified_flag ? prev : false;
        qp_flag[i] = qf;
        prev = qf;
    }

    // emulate qp_left_flag update on OUT (msb->lsb order as RTL)
    if (state == OUT) {
        qp_left_flag[7] = qp_flag[21] ? 1 : 0;
        qp_left_flag[6] = qp_flag[23] ? 1 : 0;
        qp_left_flag[5] = qp_flag[29] ? 1 : 0;
        qp_left_flag[4] = qp_flag[31] ? 1 : 0;
        qp_left_flag[3] = qp_flag[53] ? 1 : 0;
        qp_left_flag[2] = qp_flag[55] ? 1 : 0;
        qp_left_flag[1] = qp_flag[61] ? 1 : 0;
        qp_left_flag[0] = qp_flag[63] ? 1 : 0;
    }

    // emulate qp_left and qp_top RAM write on OUT with cnt==0
    if (state == OUT && (cnt == 0)) {
        qp_left_modified = qp_left;
        qp_left = qp_flag[63] ? qp_left : pkt.qp;
        // build qp_top_reg: {qp_flag[63],qp_flag[62],qp_flag[59],qp_flag[58],qp_flag[47],qp_flag[46],qp_flag[43],qp_flag[42],qp_left,qp_i}
        uint32_t top_reg = 0;
        top_reg |= static_cast<uint32_t>(qp_flag[63] ? 1u : 0u) << 19;
        top_reg |= static_cast<uint32_t>(qp_flag[62] ? 1u : 0u) << 18;
        top_reg |= static_cast<uint32_t>(qp_flag[59] ? 1u : 0u) << 17;
        top_reg |= static_cast<uint32_t>(qp_flag[58] ? 1u : 0u) << 16;
        top_reg |= static_cast<uint32_t>(qp_flag[47] ? 1u : 0u) << 15;
        top_reg |= static_cast<uint32_t>(qp_flag[46] ? 1u : 0u) << 14;
        top_reg |= static_cast<uint32_t>(qp_flag[43] ? 1u : 0u) << 13;
        top_reg |= static_cast<uint32_t>(qp_flag[42] ? 1u : 0u) << 12;
        top_reg |= static_cast<uint32_t>(qp_left & 0x3Fu) << 6;
        top_reg |= static_cast<uint32_t>(pkt.qp & 0x3Fu);
        qp_top_r[sys_ctu_x & 0x3F] = top_reg;
    }

    // read qp_top registers for selection (approximate RTL read behavior)
    uint32_t top_reg_read = 0;
    if (sys_ctu_y != 0) top_reg_read = qp_top_r[sys_ctu_x & 0x3F];
    uint8_t qp_top = static_cast<uint8_t>(top_reg_read & 0x3F);
    uint8_t qp_top_modified = static_cast<uint8_t>((top_reg_read >> 6) & 0x3F);
    uint8_t qp_top_flag = static_cast<uint8_t>((top_reg_read >> 12) & 0xFF);

    auto pack9 = [&](const std::initializer_list<int> &lst)->int {
        int out = 0; int shift = 8;
        for (int v : lst) { out |= ((v & 1) << shift); --shift; }
        return out;
    };

    // build qp_flag_h0..h8
    int qp_flag_h0 = pack9({0, (qp_top_flag>>0)&1, (qp_top_flag>>1)&1, (qp_top_flag>>2)&1, (qp_top_flag>>3)&1, (qp_top_flag>>4)&1, (qp_top_flag>>5)&1, (qp_top_flag>>6)&1, (qp_top_flag>>7)&1});
    int qp_flag_h1 = pack9({ qp_left_flag[7], qp_flag[0], qp_flag[1], qp_flag[4], qp_flag[5], qp_flag[16], qp_flag[17], qp_flag[20], qp_flag[21] });
    int qp_flag_h2 = pack9({ qp_left_flag[6], qp_flag[2], qp_flag[3], qp_flag[6], qp_flag[7], qp_flag[18], qp_flag[19], qp_flag[22], qp_flag[23] });
    int qp_flag_h3 = pack9({ qp_left_flag[5], qp_flag[8], qp_flag[9], qp_flag[12], qp_flag[13], qp_flag[24], qp_flag[25], qp_flag[28], qp_flag[29] });
    int qp_flag_h4 = pack9({ qp_left_flag[4], qp_flag[10], qp_flag[11], qp_flag[14], qp_flag[15], qp_flag[26], qp_flag[27], qp_flag[30], qp_flag[31] });
    int qp_flag_h5 = pack9({ qp_left_flag[3], qp_flag[32], qp_flag[33], qp_flag[36], qp_flag[37], qp_flag[48], qp_flag[49], qp_flag[52], qp_flag[53] });
    int qp_flag_h6 = pack9({ qp_left_flag[2], qp_flag[34], qp_flag[35], qp_flag[38], qp_flag[39], qp_flag[50], qp_flag[51], qp_flag[54], qp_flag[55] });
    int qp_flag_h7 = pack9({ qp_left_flag[1], qp_flag[40], qp_flag[41], qp_flag[44], qp_flag[45], qp_flag[56], qp_flag[57], qp_flag[60], qp_flag[61] });
    int qp_flag_h8 = pack9({ qp_left_flag[0], qp_flag[42], qp_flag[43], qp_flag[46], qp_flag[47], qp_flag[58], qp_flag[59], qp_flag[62], qp_flag[63] });

    int qp_flag_up = 0, qp_flag_dn = 0;
    if (y_flag) {
        int sel = (cnt >> 5) & 0x7;
        switch (sel) {
            case 0: qp_flag_up = qp_flag_h0; qp_flag_dn = qp_flag_h1; break;
            case 1: qp_flag_up = qp_flag_h1; qp_flag_dn = qp_flag_h2; break;
            case 2: qp_flag_up = qp_flag_h2; qp_flag_dn = qp_flag_h3; break;
            case 3: qp_flag_up = qp_flag_h3; qp_flag_dn = qp_flag_h4; break;
            case 4: qp_flag_up = qp_flag_h4; qp_flag_dn = qp_flag_h5; break;
            case 5: qp_flag_up = qp_flag_h5; qp_flag_dn = qp_flag_h6; break;
            case 6: qp_flag_up = qp_flag_h6; qp_flag_dn = qp_flag_h7; break;
            case 7: qp_flag_up = qp_flag_h7; qp_flag_dn = qp_flag_h8; break;
            default: qp_flag_up = qp_flag_dn = 0; break;
        }
    } else {
        int sel = (cnt >> 4) & 0x3;
        switch (sel) {
            case 0: qp_flag_up = qp_flag_h0; qp_flag_dn = qp_flag_h1; break;
            case 1: qp_flag_up = qp_flag_h2; qp_flag_dn = qp_flag_h3; break;
            case 2: qp_flag_up = qp_flag_h4; qp_flag_dn = qp_flag_h5; break;
            case 3: qp_flag_up = qp_flag_h6; qp_flag_dn = qp_flag_h7; break;
            default: qp_flag_up = qp_flag_dn = 0; break;
        }
    }

    bool qp_ul=false, qp_ur=false, qp_dl=false, qp_dr=false;
    if (y_flag) {
        int sel = (cnt >> 2) & 0x7;
        switch (sel) {
            case 0: qp_ul = (qp_flag_up >> 8) & 1; qp_ur = (qp_flag_up >> 7) & 1; qp_dl = (qp_flag_dn >> 8) & 1; qp_dr = (qp_flag_dn >> 7) & 1; break;
            case 1: qp_ul = (qp_flag_up >> 7) & 1; qp_ur = (qp_flag_up >> 6) & 1; qp_dl = (qp_flag_dn >> 7) & 1; qp_dr = (qp_flag_dn >> 6) & 1; break;
            case 2: qp_ul = (qp_flag_up >> 6) & 1; qp_ur = (qp_flag_up >> 5) & 1; qp_dl = (qp_flag_dn >> 6) & 1; qp_dr = (qp_flag_dn >> 5) & 1; break;
            case 3: qp_ul = (qp_flag_up >> 5) & 1; qp_ur = (qp_flag_up >> 4) & 1; qp_dl = (qp_flag_dn >> 5) & 1; qp_dr = (qp_flag_dn >> 4) & 1; break;
            case 4: qp_ul = (qp_flag_up >> 4) & 1; qp_ur = (qp_flag_up >> 3) & 1; qp_dl = (qp_flag_dn >> 4) & 1; qp_dr = (qp_flag_dn >> 3) & 1; break;
            case 5: qp_ul = (qp_flag_up >> 3) & 1; qp_ur = (qp_flag_up >> 2) & 1; qp_dl = (qp_flag_dn >> 3) & 1; qp_dr = (qp_flag_dn >> 2) & 1; break;
            case 6: qp_ul = (qp_flag_up >> 2) & 1; qp_ur = (qp_flag_up >> 1) & 1; qp_dl = (qp_flag_dn >> 2) & 1; qp_dr = (qp_flag_dn >> 1) & 1; break;
            case 7: qp_ul = (qp_flag_up >> 1) & 1; qp_ur = (qp_flag_up >> 0) & 1; qp_dl = (qp_flag_dn >> 1) & 1; qp_dr = (qp_flag_dn >> 0) & 1; break;
        }
    } else {
        int sel = (cnt >> 2) & 0x3;
        switch (sel) {
            case 0: qp_ul = (qp_flag_up >> 8) & 1; qp_ur = (qp_flag_up >> 7) & 1; qp_dl = (qp_flag_dn >> 8) & 1; qp_dr = (qp_flag_dn >> 7) & 1; break;
            case 1: qp_ul = (qp_flag_up >> 6) & 1; qp_ur = (qp_flag_up >> 5) & 1; qp_dl = (qp_flag_dn >> 6) & 1; qp_dr = (qp_flag_dn >> 4) & 1; break;
            case 2: qp_ul = (qp_flag_up >> 4) & 1; qp_ur = (qp_flag_up >> 3) & 1; qp_dl = (qp_flag_dn >> 4) & 1; qp_dr = (qp_flag_dn >> 3) & 1; break;
            case 3: qp_ul = (qp_flag_up >> 2) & 1; qp_ur = (qp_flag_up >> 1) & 1; qp_dl = (qp_flag_dn >> 2) & 1; qp_dr = (qp_flag_dn >> 1) & 1; break;
        }
    }

    // selection logic per db_bs.v
    uint8_t qp_i = pkt.qp & 0x3F;
    uint8_t qp_tl = ( (qp_top_flag >> 7) & 1 ) ? qp_top_modified : qp_top;
    uint8_t qp_p_r = 0, qp_q_r = 0;
    if (top_left_flag) {
        switch (cnt & 0x3) {
            case 0: qp_p_r = qp_tl; qp_q_r = qp_ur ? qp_top_modified : qp_top; break;
            case 1: qp_p_r = qp_dl ? qp_left_modified : qp_left; qp_q_r = qp_dr ? qp_left_modified : qp_left; break;
            case 2: qp_p_r = qp_ul ? qp_left_modified : qp_left; qp_q_r = qp_dl ? qp_left_modified : qp_left; break;
            case 3: qp_p_r = qp_ur ? qp_top_modified : qp_top; qp_q_r = qp_dr ? qp_left : qp_i; break;
        }
    } else if (top_flag) {
        switch (cnt & 0x3) {
            case 0: qp_p_r = qp_ul ? qp_top_modified : qp_top; qp_q_r = qp_ur ? qp_top_modified : qp_top; break;
            case 1: qp_p_r = qp_dl ? qp_left : qp_i; qp_q_r = qp_dr ? qp_left : qp_i; break;
            case 2: qp_p_r = qp_ul ? qp_top_modified : qp_top; qp_q_r = qp_dl ? qp_left : qp_i; break;
            case 3: qp_p_r = qp_ur ? qp_top_modified : qp_top; qp_q_r = qp_dr ? qp_left : qp_i; break;
        }
    } else if (left_flag) {
        switch (cnt & 0x3) {
            case 0: qp_p_r = qp_ul ? qp_left_modified : qp_left; qp_q_r = qp_ur ? qp_left : qp_i; break;
            case 1: qp_p_r = qp_dl ? qp_left_modified : qp_left; qp_q_r = qp_dr ? qp_left : qp_i; break;
            case 2: qp_p_r = qp_ul ? qp_left_modified : qp_left; qp_q_r = qp_dl ? qp_left_modified : qp_left; break;
            case 3: qp_p_r = qp_ur ? qp_left : qp_i; qp_q_r = qp_dr ? qp_left : qp_i; break;
        }
    } else {
        switch (cnt & 0x3) {
            case 0: qp_p_r = qp_ul ? qp_left : qp_i; qp_q_r = qp_ur ? qp_left : qp_i; break;
            case 1: qp_p_r = qp_dl ? qp_left : qp_i; qp_q_r = qp_dr ? qp_left : qp_i; break;
            case 2: qp_p_r = qp_ul ? qp_left : qp_i; qp_q_r = qp_dl ? qp_left : qp_i; break;
            case 3: qp_p_r = qp_ur ? qp_left : qp_i; qp_q_r = qp_dr ? qp_left : qp_i; break;
        }
    }

    qp_p = qp_p_r;
    qp_q = qp_q_r;
}