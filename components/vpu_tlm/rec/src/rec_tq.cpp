#include "rec_tq.h"

RecTQ::RecTQ(sc_core::sc_module_name name) : 
    sc_module(name), buffer_socket("buffer_socket"), cabac_socket("cabac_socket"), inv_tq_socket("inv_tq_socket") {
    buffer_socket.register_b_transport(this, &RecTQ::b_transport);  
}

void RecTQ::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
    // Unpack the incoming generic payload into a typed RecPacket, then
    // re-pack and forward a local transaction to the InvTQ path. This keeps
    // RecTQ simple: it doesn't need reference pixels, only packet routing.
    RecPacket pkt = unpackRecPacket(trans);

    // If this is a RESIDUAL packet, perform DCT -> Quantize and send COEFF
    if (pkt.cmd == RecCmd::RESIDUAL) {
        // Parse residual samples: pkt.data holds biased-by-128 signed residuals
        uint8_t blk_size = pkt.size; // 0:4x4,1:8x8,...
        int side = 4 << blk_size;
        int expected_n = side * side;
        int n = (pkt.data.size() > 0) ? static_cast<int>(pkt.data.size()) : expected_n;
        std::vector<int16_t> residuals;
        residuals.reserve(n);
        for (int i = 0; i < n; ++i) {
            uint8_t b = (i < (int)pkt.data.size()) ? pkt.data[i] : 128;
            residuals.push_back(static_cast<int16_t>(static_cast<int>(b) - 128));
        }

        std::vector<int32_t> coeffs(n);
        dct(blk_size, residuals, coeffs);

        std::vector<int16_t> qcoeffs(n);
        bool type_i = (pkt.pred_type == static_cast<uint8_t>(PredType::INTRA));
        quantize(blk_size, coeffs, pkt.qp, type_i, qcoeffs);

        //build COEFF packet for cabac
        RecPacket c_pkt = pkt;
        c_pkt.cmd = RecCmd::COEFF;
        c_pkt.data.clear();
        // pack qcoeffs as little-endian int16
        for (int i = 0; i < n; ++i) {
            int16_t v = qcoeffs[i];
            uint8_t lo = v & 0xFF;
            uint8_t hi = (v >> 8) & 0xFF;
            c_pkt.data.push_back(lo);
            c_pkt.data.push_back(hi);
        }

        std::cout << "----------------TQ Packet (internal)---------------" << std::endl;
        std::cout << pkt;
        //cabac forward
        std::vector<uint8_t> cabac_buf = packRecPacket(c_pkt);
        tlm::tlm_generic_payload cabac_trans;
        cabac_trans.set_command(tlm::TLM_WRITE_COMMAND);
        cabac_trans.set_address(0);
        cabac_trans.set_data_ptr(cabac_buf.data());
        cabac_trans.set_data_length(cabac_buf.size());
        sc_core::sc_time d = sc_core::SC_ZERO_TIME;
        cabac_socket->b_transport(cabac_trans, d);
        // inv_tq forward: send COEFF packet (c_pkt) as TQ output
        std::vector<uint8_t> inv_buf = cabac_buf;
        tlm::tlm_generic_payload inv_trans;
        inv_trans.set_command(tlm::TLM_WRITE_COMMAND);
        inv_trans.set_address(0);
        inv_trans.set_data_ptr(inv_buf.data());
        inv_trans.set_data_length(inv_buf.size());
        inv_tq_socket->b_transport(inv_trans, d);

        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return;
    }

    // default behavior: forward to inv_tq
    std::vector<uint8_t> buf = packRecPacket(pkt);
    tlm::tlm_generic_payload new_trans;
    new_trans.set_command(tlm::TLM_WRITE_COMMAND);
    new_trans.set_address(0);
    new_trans.set_data_ptr(buf.data());
    new_trans.set_data_length(buf.size());
    sc_core::sc_time d = sc_core::SC_ZERO_TIME;

    inv_tq_socket->b_transport(new_trans, d);
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}
    
// Simple integer 4x4 DCT approximating the RTL transform_mtr behavior.
// For now implement separable 4x4 transform. Larger sizes can reuse similar logic.
void RecTQ::dct(uint8_t size4x4, const std::vector<int16_t>& in, std::vector<int32_t>& out) {
    int side = 4 << size4x4; // 4,8,16,32
    int N = side * side;
    out.assign(N, 0);
    if (size4x4 == 0) {
        // 4x4 block
        int32_t tmp[4][4];
        // load input into 2D
        int idx = 0;
        int16_t src[4][4];
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c)
                src[r][c] = (idx < (int)in.size()) ? in[idx++] : 0;

        // horizontal
        for (int r = 0; r < 4; ++r) {
            int a0 = src[r][0] + src[r][3];
            int a1 = src[r][1] + src[r][2];
            int a2 = src[r][1] - src[r][2];
            int a3 = src[r][0] - src[r][3];

            tmp[r][0] = a0 + a1; // DC
            tmp[r][1] = a3 + (a2>>1);
            tmp[r][2] = a0 - a1;
            tmp[r][3] = a3 - (a2>>1);
        }

        // vertical
        for (int c = 0; c < 4; ++c) {
            int b0 = tmp[0][c] + tmp[3][c];
            int b1 = tmp[1][c] + tmp[2][c];
            int b2 = tmp[1][c] - tmp[2][c];
            int b3 = tmp[0][c] - tmp[3][c];

            int32_t o0 = b0 + b1;
            int32_t o1 = b3 + (b2>>1);
            int32_t o2 = b0 - b1;
            int32_t o3 = b3 - (b2>>1);

            out[c + 0*4] = o0;
            out[c + 1*4] = o1;
            out[c + 2*4] = o2;
            out[c + 3*4] = o3;
        }
    } else {
        // fallback: copy inputs as coefficients
        for (int i = 0; i < N && i < (int)in.size(); ++i) out[i] = in[i];
    }
}

// Quantize implemented following the RTL: compute Q from qp/type, then
// q = sign * (((abs(coeff)*Q)+offset) >> shift)
void RecTQ::quantize(uint8_t size4x4, const std::vector<int32_t>& in, uint8_t qp, bool type_i, std::vector<int16_t>& out) {
    int side = 4 << size4x4;
    int N = side * side;
    out.assign(N, 0);

    // RTL tables from mod.v / quan.v
    static const int q_data_table[6] = { 26214, 23302, 20560, 18396, 16384, 14564 };

    int nQpMod6 = qp % 6;
    int nQpDiv6 = qp / 6;

    // base shifts per transform size (matches mod.v): for DCT_4/8/16/32
    int shift_mux_n = 19;      // DCT_4
    int shift_size_mux_0 = 10; // DCT_4
    switch (size4x4) {
        case 0: shift_mux_n = 19; shift_size_mux_0 = 10; break; // 4x4
        case 1: shift_mux_n = 18; shift_size_mux_0 = 9;  break; // 8x8
        case 2: shift_mux_n = 17; shift_size_mux_0 = 8;  break; // 16x16
        case 3: shift_mux_n = 16; shift_size_mux_0 = 7;  break; // 32x32
        default: shift_mux_n = 19; shift_size_mux_0 = 10; break;
    }

    int shift = shift_mux_n + nQpDiv6;
    int shift_size_0 = shift_size_mux_0 + nQpDiv6;
    int data_mux_type = type_i ? 85 : 171; // 9-bit constant in RTL
    uint64_t offset = (uint64_t)data_mux_type << shift_size_0; // offset = data_mux_type << shift_size_0

    int Q = q_data_table[nQpMod6];

    for (int i = 0; i < N && i < (int)in.size(); ++i) {
        int32_t coeff = in[i];
        int sign = (coeff < 0) ? -1 : 1;
        uint32_t a = static_cast<uint32_t>(coeff * sign);
        // multiply abs(coeff) by Q (fixed-point), add offset, then arithmetic right shift by `shift`.
        uint64_t m = (uint64_t)a * (uint64_t)Q; // wide multiply
        uint64_t w_a = m + offset;
        uint64_t w_s = (shift >= 64) ? 0ULL : (w_a >> shift);
        int64_t qv = static_cast<int64_t>(w_s) * sign;
        // clamp to int16 range
        if (qv > INT16_MAX) qv = INT16_MAX;
        if (qv < INT16_MIN) qv = INT16_MIN;
        out[i] = static_cast<int16_t>(qv);
    }
}
