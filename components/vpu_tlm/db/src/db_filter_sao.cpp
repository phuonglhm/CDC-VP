#include "db_filter_sao.h"
#include <algorithm>

Filter_SAO::Filter_SAO(sc_core::sc_module_name name)  : sc_module(name), out_socket("sao_socket"), bs_socket("bs_socket"), mv_socket("mv_socket") {
    bs_socket.register_b_transport(this, &Filter_SAO::b_transport);
    mv_socket.register_b_transport(this, &Filter_SAO::b_transport); //placeholder
}

void Filter_SAO::unpack_blocks(const DbCustomPacket &pkt, std::array<uint8_t,16> &p_blk, std::array<uint8_t,16> &q_blk) const {
    p_blk.fill(0);
    q_blk.fill(0);
    const size_t n = pkt.data.size();
    if (n == 0) return;
    const size_t copy_p = std::min<size_t>(16, n);
    std::copy(pkt.data.begin(), pkt.data.begin() + copy_p, p_blk.begin());
    if (n > 16) {
        const size_t copy_q = std::min<size_t>(16, n - 16);
        std::copy(pkt.data.begin() + 16, pkt.data.begin() + 16 + copy_q, q_blk.begin());
    }
}

void Filter_SAO::pack_blocks(DbCustomPacket &pkt, const std::array<uint8_t,16> &p_blk, const std::array<uint8_t,16> &q_blk) const {
    pkt.data.clear();
    pkt.data.reserve(32);
    pkt.data.insert(pkt.data.end(), p_blk.begin(), p_blk.end());
    pkt.data.insert(pkt.data.end(), q_blk.begin(), q_blk.end());
}

void Filter_SAO::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
    DbCustomPacket pkt = unpackDbCustomPacket(trans);

    // Extract 4x4 blocks
    std::array<uint8_t,16> p_in{}, q_in{}, p_out{}, q_out{};
    unpack_blocks(pkt, p_in, q_in);

    // Build filter params from canonical metadata populated by BorderStrength/MotionVector
    FilterParams params;
    params.is_luma = (pkt.sel == 0);
    params.tu_edge = pkt.tu_edge;
    params.pu_edge = pkt.pu_edge;
    params.cbf_p = pkt.cbf_p;
    params.cbf_q = pkt.cbf_q;
    // qp_p/qp_q may be annotated by BorderStrength; fall back to pkt.qp if zero
    params.qp_p = (pkt.qp_p != 0) ? pkt.qp_p : pkt.qp;
    params.qp_q = (pkt.qp_q != 0) ? pkt.qp_q : pkt.qp;
    params.bs = pkt.bs;
    params.is_ver = pkt.is_ver;

    // Choose kernel based on annotated bs and chroma/luma
    if (params.is_luma) {
        if (params.bs > 0) {
            // use RTL-like dsam/norm_str test to decide between normal vs strong
            if (want_strong_filter(p_in, q_in, params)) apply_strong_filter(p_in, q_in, p_out, q_out, params);
            else apply_normal_filter(p_in, q_in, p_out, q_out, params);
        } else {
            p_out = p_in; q_out = q_in;
        }
    } else {
        if (params.bs > 0) apply_chroma_filter(p_in, q_in, p_out, q_out, params);
        else { p_out = p_in; q_out = q_in; }
    }

    pack_blocks(pkt, p_out, q_out);
    std::vector<uint8_t> outbuf = packDbCustomPacket(pkt);
    trans.set_data_ptr(outbuf.data());
    trans.set_data_length(outbuf.size());
    out_socket->b_transport(trans, delay);
}

uint8_t Filter_SAO::lookup_beta(uint8_t qp) const {
    switch (qp) {
        case 16: return 6;
        case 17: return 7;
        case 18: return 8;
        case 19: return 9;
        case 20: return 10;
        case 21: return 11;
        case 22: return 12;
        case 23: return 13;
        case 24: return 14;
        case 25: return 15;
        case 26: return 16;
        case 27: return 17;
        case 28: return 18;
        case 29: return 20;
        case 30: return 22;
        case 31: return 24;
        case 32: return 26;
        case 33: return 28;
        case 34: return 30;
        case 35: return 32;
        case 36: return 34;
        case 37: return 36;
        case 38: return 38;
        case 39: return 40;
        case 40: return 42;
        case 41: return 44;
        case 42: return 46;
        case 43: return 48;
        case 44: return 50;
        case 45: return 52;
        case 46: return 54;
        case 47: return 56;
        case 48: return 58;
        case 49: return 60;
        case 50: return 62;
        case 51: return 64;
        default: return 0;
    }
}

uint8_t Filter_SAO::lookup_tc(uint8_t qp, bool intra) const {
    int qp_w = static_cast<int>(qp) + (intra ? 1 : 0);
    switch (qp_w) {
        case 18: case 19: case 20: case 21: case 22: case 23: case 24: case 25: case 26: return 1;
        case 27: case 28: case 29: case 30: return 2;
        case 31: case 32: case 33: case 34: return 3;
        case 35: case 36: case 37: return 4;
        case 38: case 39: return 5;
        case 40: case 41: return 6;
        case 42: return 7;
        case 43: return 8;
        case 44: return 9;
        case 45: return 10;
        case 46: return 11;
        case 47: return 13;
        case 48: return 14;
        case 49: return 16;
        case 50: return 18;
        case 51: return 20;
        case 52: return 22;
        case 53: return 24;
        default: return 0;
    }
}

void Filter_SAO::apply_normal_filter(const std::array<uint8_t,16> &p_in,
                                     const std::array<uint8_t,16> &q_in,
                                     std::array<uint8_t,16> &p_out,
                                     std::array<uint8_t,16> &q_out,
                                     const FilterParams &params) const {
    // Start with passthrough
    p_out = p_in;
    q_out = q_in;

    // compute qpw and lookup beta/tc
    int qpw = ((int)params.qp_p + (int)params.qp_q + 1) >> 1;
    uint8_t beta_w = lookup_beta(static_cast<uint8_t>(qpw));

    // compute qp_lc per db_filter.v logic
    int qpwcc = qpw - 30;
    int qpw1 = 0;
    switch (qpwcc) {
        case 0: qpw1 = 29; break;
        case 1: qpw1 = 30; break;
        case 2: qpw1 = 31; break;
        case 3: qpw1 = 32; break;
        case 4: qpw1 = 33; break;
        case 5: qpw1 = 33; break;
        case 6: qpw1 = 34; break;
        case 7: qpw1 = 34; break;
        case 8: qpw1 = 35; break;
        case 9: qpw1 = 35; break;
        case 10: qpw1 = 36; break;
        case 11: qpw1 = 36; break;
        case 12: qpw1 = 37; break;
        case 13: qpw1 = 37; break;
        default: qpw1 = 0; break;
    }
    int qpc;
    if (qpw > 29 && qpw < 44) qpc = qpw1;
    else if (qpw < 30) qpc = qpw;
    else qpc = qpw - 6;
    int qp_lc = params.is_luma ? qpw : qpc;

    // For tc lookup we don't currently have MB-type info; assume INTER (intra=false)
    uint8_t tc_w = lookup_tc(static_cast<uint8_t>(qp_lc), false);

    // Extract edge pixels assuming vertical edge: p rightmost column, q leftmost
    int p0_0[4], p0_1[4], p0_2[4], p0_3[4];
    int q0_0[4], q0_1[4], q0_2[4], q0_3[4];
    for (int r = 0; r < 4; ++r) {
        p0_0[r] = p_in[r*4 + 3];
        p0_1[r] = p_in[r*4 + 2];
        p0_2[r] = p_in[r*4 + 1];
        p0_3[r] = p_in[r*4 + 0];

        q0_0[r] = q_in[r*4 + 0];
        q0_1[r] = q_in[r*4 + 1];
        q0_2[r] = q_in[r*4 + 2];
        q0_3[r] = q_in[r*4 + 3];
    }

    // stage1_a
    int dp0 = p0_2[0] + p0_0[0] - 2 * p0_1[0];
    int dp3 = p0_2[3] + p0_0[3] - 2 * p0_1[3];
    int dq0 = q0_2[0] + q0_0[0] - 2 * q0_1[0];
    int dq3 = q0_2[3] + q0_0[3] - 2 * q0_1[3];

    int dp0_abs = std::abs(dp0);
    int dp3_abs = std::abs(dp3);
    int dq0_abs = std::abs(dq0);
    int dq3_abs = std::abs(dq3);

    int dpw = dp0_abs + dp3_abs;
    int dqw = dq0_abs + dq3_abs;
    int d_w = dpw + dqw;

    int dp0_3_0 = std::abs(p0_0[0] - p0_3[0]);
    int dq0_3_0 = std::abs(q0_0[0] - q0_3[0]);
    int dp3_3_0 = std::abs(p0_0[3] - p0_3[3]);
    int dq3_3_0 = std::abs(q0_0[3] - q0_3[3]);
    int dpq0_0_0 = std::abs(p0_0[0] - q0_0[0]);
    int dpq3_0_0 = std::abs(p0_0[3] - q0_0[3]);

    int dqp0_w = dp0_abs + dq0_abs;
    int dqp3_w = dp3_abs + dq3_abs;
    int dqp0_m_2_w = dqp0_w << 1;
    int dqp3_m_2_w = dqp3_w << 1;

    int beta_m_w = (static_cast<int>(beta_w) + (static_cast<int>(beta_w) >> 1)) >> 3;
    int tc_mux_3_2_w = ((tc_w << 1) + tc_w + 1) >> 1;

    bool dsam0 = (dqp0_m_2_w < (static_cast<int>(beta_w) >> 2)) && ((dp0_3_0 + dq0_3_0) < (static_cast<int>(beta_w) >> 3)) && (dpq0_0_0 < tc_mux_3_2_w);
    bool dsam3 = (dqp3_m_2_w < (static_cast<int>(beta_w) >> 2)) && ((dp3_3_0 + dq3_3_0) < (static_cast<int>(beta_w) >> 3)) && (dpq3_0_0 < tc_mux_3_2_w);

    bool norm_str_w = dsam0 && dsam3;

    // stage1_b: compute deltas per row
    int tc_x = -static_cast<int>(tc_w);
    int tc_y = static_cast<int>(tc_w);

    int tc_mux_10 = (tc_w << 3) + (tc_w << 1); // tc*10

    int delta_o[4];
    int delta_abs[4];
    int not_nature_edge[4];
    for (int r = 0; r < 4; ++r) {
        int qm_p0 = q0_0[r] - p0_0[r];
        int qm_p1 = q0_1[r] - p0_1[r];
        int qm_p0_m8 = qm_p0 << 3;
        int qm_p1_m1 = qm_p1 << 1;
        int qm_p_w = qm_p0_m8 - qm_p1_m1;
        int qm_q_w = qm_p0 - qm_p1;
        int delta_w = qm_p_w + qm_q_w + 8;
        int delta = delta_w >> 4;
        delta_abs[r] = std::abs(delta);
        not_nature_edge[r] = (delta_abs[r] < tc_mux_10) ? 1 : 0;
        // clamp to [-tc, tc]
        if (delta < tc_x) delta = tc_x;
        if (delta > tc_y) delta = tc_y;
        delta_o[r] = delta;
    }

    // normal filter: compute p0_0_o and q0_0_o
    int p0_0_o[4], q0_0_o[4];
    for (int r = 0; r < 4; ++r) {
        int pplus = p0_0[r] + delta_o[r];
        int qplus = q0_0[r] - delta_o[r];
        p0_0_o[r] = static_cast<int>(clamp8(pplus));
        q0_0_o[r] = static_cast<int>(clamp8(qplus));
    }

    // normal filter: second stage for p*_1 and q*_1
    int tc_div = static_cast<int>(tc_w) >> 1; // approximate tc_div_y
    for (int r = 0; r < 4; ++r) {
        int deltap2 = (((p0_2[r] + p0_0[r] + 1) >> 1) - p0_1[r] + delta_o[r]) >> 1;
        int deltaq2 = (((q0_2[r] + q0_0[r] + 1) >> 1) - q0_1[r] - delta_o[r]) >> 1;
        if (deltap2 < -tc_div) deltap2 = -tc_div;
        if (deltap2 > tc_div) deltap2 = tc_div;
        if (deltaq2 < -tc_div) deltaq2 = -tc_div;
        if (deltaq2 > tc_div) deltaq2 = tc_div;
        int pplus1 = p0_1[r] + deltap2;
        int qplus1 = q0_1[r] + deltaq2;
        int p0_1_o = static_cast<int>(clamp8(pplus1));
        int q0_1_o = static_cast<int>(clamp8(qplus1));

        // write back into output blocks (vertical edge mapping)
        p_out[r*4 + 3] = static_cast<uint8_t>(p0_0_o[r]);
        p_out[r*4 + 2] = static_cast<uint8_t>(p0_1_o);
        // p_out[r*4 +1] and [r*4 +0] remain unchanged

        q_out[r*4 + 0] = static_cast<uint8_t>(q0_0_o[r]);
        q_out[r*4 + 1] = static_cast<uint8_t>(q0_1_o);
        // q_out[r*4 +2] and [r*4 +3] remain unchanged
    }
}

static inline uint8_t clip3_int(int iVal, int x, int y) {
    if (iVal < x) return static_cast<uint8_t>(x);
    if (iVal > y) return static_cast<uint8_t>(y);
    return static_cast<uint8_t>(iVal & 0xFF);
}

void Filter_SAO::apply_strong_filter(const std::array<uint8_t,16> &p_in,
                                     const std::array<uint8_t,16> &q_in,
                                     std::array<uint8_t,16> &p_out,
                                     std::array<uint8_t,16> &q_out,
                                     const FilterParams &params) const {
    p_out = p_in;
    q_out = q_in;

    // compute qp/tc similar to normal filter
    int qpw = ((int)params.qp_p + (int)params.qp_q + 1) >> 1;
    int qpwcc = qpw - 30;
    int qpw1 = 0;
    switch (qpwcc) {
        case 0: qpw1 = 29; break; case 1: qpw1 = 30; break; case 2: qpw1 = 31; break; case 3: qpw1 = 32; break;
        case 4: qpw1 = 33; break; case 5: qpw1 = 33; break; case 6: qpw1 = 34; break; case 7: qpw1 = 34; break;
        case 8: qpw1 = 35; break; case 9: qpw1 = 35; break; case 10: qpw1 = 36; break; case 11: qpw1 = 36; break;
        case 12: qpw1 = 37; break; case 13: qpw1 = 37; break; default: qpw1 = 0; break;
    }
    int qpc = (qpw > 29 && qpw < 44) ? qpw1 : ((qpw < 30) ? qpw : (qpw - 6));
    int qp_lc = params.is_luma ? qpw : qpc;
    uint8_t tc_w = lookup_tc(static_cast<uint8_t>(qp_lc), false);

    int tc2 = static_cast<int>(tc_w) << 1; // 2*tc

    // process each row
    for (int r = 0; r < 4; ++r) {
        int p0_0 = p_in[r*4 + 3];
        int p0_1 = p_in[r*4 + 2];
        int p0_2 = p_in[r*4 + 1];
        int p0_3 = p_in[r*4 + 0];

        int q0_0 = q_in[r*4 + 0];
        int q0_1 = q_in[r*4 + 1];
        int q0_2 = q_in[r*4 + 2];
        int q0_3 = q_in[r*4 + 3];

        int pcommon = p0_2 + p0_1 + p0_0 + q0_0 + 2;
        int p0_0_w = (pcommon + p0_1 + p0_0 + q0_0 + q0_1 + 2) >> 3;
        int p0_1_w = (pcommon) >> 2;
        int p0_2_w = (pcommon + (p0_3 << 1) + (p0_2 << 1) + 2) >> 3;

        int qcommon = q0_2 + q0_1 + q0_0 + p0_0 + 2;
        int q0_0_w = (qcommon + q0_1 + q0_0 + p0_0 + p0_1 + 2) >> 3;
        int q0_1_w = (qcommon) >> 2;
        int q0_2_w = (qcommon + (q0_3 << 1) + (q0_2 << 1) + 2) >> 3;

        // compute clipping bounds (clamped to 0..255)
        int p0_0_min = p0_0 - tc2; if (p0_0_min < 0) p0_0_min = 0;
        int p0_0_max = p0_0 + tc2; if (p0_0_max > 255) p0_0_max = 255;
        int p0_1_min = p0_1 - tc2; if (p0_1_min < 0) p0_1_min = 0;
        int p0_1_max = p0_1 + tc2; if (p0_1_max > 255) p0_1_max = 255;
        int p0_2_min = p0_2 - tc2; if (p0_2_min < 0) p0_2_min = 0;
        int p0_2_max = p0_2 + tc2; if (p0_2_max > 255) p0_2_max = 255;

        int q0_0_min = q0_0 - tc2; if (q0_0_min < 0) q0_0_min = 0;
        int q0_0_max = q0_0 + tc2; if (q0_0_max > 255) q0_0_max = 255;
        int q0_1_min = q0_1 - tc2; if (q0_1_min < 0) q0_1_min = 0;
        int q0_1_max = q0_1 + tc2; if (q0_1_max > 255) q0_1_max = 255;
        int q0_2_min = q0_2 - tc2; if (q0_2_min < 0) q0_2_min = 0;
        int q0_2_max = q0_2 + tc2; if (q0_2_max > 255) q0_2_max = 255;

        uint8_t p0_0_o = clip3_int(p0_0_w, p0_0_min, p0_0_max);
        uint8_t p0_1_o = clip3_int(p0_1_w, p0_1_min, p0_1_max);
        uint8_t p0_2_o = clip3_int(p0_2_w, p0_2_min, p0_2_max);

        uint8_t q0_0_o = clip3_int(q0_0_w, q0_0_min, q0_0_max);
        uint8_t q0_1_o = clip3_int(q0_1_w, q0_1_min, q0_1_max);
        uint8_t q0_2_o = clip3_int(q0_2_w, q0_2_min, q0_2_max);

        // write back (vertical-edge mapping)
        p_out[r*4 + 3] = p0_0_o;
        p_out[r*4 + 2] = p0_1_o;
        p_out[r*4 + 1] = p0_2_o;

        q_out[r*4 + 0] = q0_0_o;
        q_out[r*4 + 1] = q0_1_o;
        q_out[r*4 + 2] = q0_2_o;
    }
}

void Filter_SAO::apply_chroma_filter(const std::array<uint8_t,16> &p_in,
                                     const std::array<uint8_t,16> &q_in,
                                     std::array<uint8_t,16> &p_out,
                                     std::array<uint8_t,16> &q_out,
                                     const FilterParams &params) const {
    p_out = p_in;
    q_out = q_in;

    int qpw = ((int)params.qp_p + (int)params.qp_q + 1) >> 1;
    int qpwcc = qpw - 30;
    int qpw1 = 0;
    switch (qpwcc) {
        case 0: qpw1 = 29; break; case 1: qpw1 = 30; break; case 2: qpw1 = 31; break; case 3: qpw1 = 32; break;
        case 4: qpw1 = 33; break; case 5: qpw1 = 33; break; case 6: qpw1 = 34; break; case 7: qpw1 = 34; break;
        case 8: qpw1 = 35; break; case 9: qpw1 = 35; break; case 10: qpw1 = 36; break; case 11: qpw1 = 36; break;
        case 12: qpw1 = 37; break; case 13: qpw1 = 37; break; default: qpw1 = 0; break;
    }
    int qpc = (qpw > 29 && qpw < 44) ? qpw1 : ((qpw < 30) ? qpw : (qpw - 6));
    int qp_lc = params.is_luma ? qpw : qpc;
    uint8_t tc_w = lookup_tc(static_cast<uint8_t>(qp_lc), false);

    int tc_x = -static_cast<int>(tc_w);
    int tc_y = static_cast<int>(tc_w);

    for (int r = 0; r < 4; ++r) {
        int p0_0 = p_in[r*4 + 3];
        int p0_1 = p_in[r*4 + 2];
        int q0_0 = q_in[r*4 + 0];
        int q0_1 = q_in[r*4 + 1];

        int delta_w = ((((q0_0 - p0_0) << 2) + p0_1 - q0_1 + 4) >> 3);
        if (delta_w < tc_x) delta_w = tc_x;
        if (delta_w > tc_y) delta_w = tc_y;

        int p0_0_m = p0_0 + delta_w;
        int q0_0_m = q0_0 - delta_w;

        p_out[r*4 + 3] = clamp8(p0_0_m);
        q_out[r*4 + 0] = clamp8(q0_0_m);
    }
}

bool Filter_SAO::want_strong_filter(const std::array<uint8_t,16> &p_in,
                                    const std::array<uint8_t,16> &q_in,
                                    const FilterParams &params) const {
    // replicate the dsam/norm_str calculation from the RTL/db_filter.v
    int qpw = ((int)params.qp_p + (int)params.qp_q + 1) >> 1;
    uint8_t beta_w = lookup_beta(static_cast<uint8_t>(qpw));

    int qpwcc = qpw - 30;
    int qpw1 = 0;
    switch (qpwcc) {
        case 0: qpw1 = 29; break;
        case 1: qpw1 = 30; break;
        case 2: qpw1 = 31; break;
        case 3: qpw1 = 32; break;
        case 4: qpw1 = 33; break;
        case 5: qpw1 = 33; break;
        case 6: qpw1 = 34; break;
        case 7: qpw1 = 34; break;
        case 8: qpw1 = 35; break;
        case 9: qpw1 = 35; break;
        case 10: qpw1 = 36; break;
        case 11: qpw1 = 36; break;
        case 12: qpw1 = 37; break;
        case 13: qpw1 = 37; break;
        default: qpw1 = 0; break;
    }
    int qpc;
    if (qpw > 29 && qpw < 44) qpc = qpw1;
    else if (qpw < 30) qpc = qpw;
    else qpc = qpw - 6;
    int qp_lc = params.is_luma ? qpw : qpc;

    uint8_t tc_w = lookup_tc(static_cast<uint8_t>(qp_lc), false);

    // extract edge pixels assuming vertical edge mapping (same mapping used in apply_normal_filter)
    int p0_0[4], p0_1[4], p0_2[4], p0_3[4];
    int q0_0[4], q0_1[4], q0_2[4], q0_3[4];
    for (int r = 0; r < 4; ++r) {
        p0_0[r] = p_in[r*4 + 3];
        p0_1[r] = p_in[r*4 + 2];
        p0_2[r] = p_in[r*4 + 1];
        p0_3[r] = p_in[r*4 + 0];

        q0_0[r] = q_in[r*4 + 0];
        q0_1[r] = q_in[r*4 + 1];
        q0_2[r] = q_in[r*4 + 2];
        q0_3[r] = q_in[r*4 + 3];
    }

    int dp0 = p0_2[0] + p0_0[0] - 2 * p0_1[0];
    int dp3 = p0_2[3] + p0_0[3] - 2 * p0_1[3];
    int dq0 = q0_2[0] + q0_0[0] - 2 * q0_1[0];
    int dq3 = q0_2[3] + q0_0[3] - 2 * q0_1[3];

    int dp0_abs = std::abs(dp0);
    int dp3_abs = std::abs(dp3);
    int dq0_abs = std::abs(dq0);
    int dq3_abs = std::abs(dq3);

    int dqp0_w = dp0_abs + dq0_abs;
    int dqp3_w = dp3_abs + dq3_abs;
    int dqp0_m_2_w = dqp0_w << 1;
    int dqp3_m_2_w = dqp3_w << 1;

    int dp0_3_0 = std::abs(p0_0[0] - p0_3[0]);
    int dq0_3_0 = std::abs(q0_0[0] - q0_3[0]);
    int dp3_3_0 = std::abs(p0_0[3] - p0_3[3]);
    int dq3_3_0 = std::abs(q0_0[3] - q0_3[3]);
    int dpq0_0_0 = std::abs(p0_0[0] - q0_0[0]);
    int dpq3_0_0 = std::abs(p0_0[3] - q0_0[3]);

    int tc_mux_3_2_w = ((tc_w << 1) + tc_w + 1) >> 1;

    bool dsam0 = (dqp0_m_2_w < (static_cast<int>(beta_w) >> 2)) && ((dp0_3_0 + dq0_3_0) < (static_cast<int>(beta_w) >> 3)) && (dpq0_0_0 < tc_mux_3_2_w);
    bool dsam3 = (dqp3_m_2_w < (static_cast<int>(beta_w) >> 2)) && ((dp3_3_0 + dq3_3_0) < (static_cast<int>(beta_w) >> 3)) && (dpq3_0_0 < tc_mux_3_2_w);

    bool norm_str_w = dsam0 && dsam3;
    return norm_str_w;
}
