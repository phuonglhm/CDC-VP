#include "rec_inv_tq.h"
#include <algorithm>
#include <cstdint>
#include <cmath>

#include "debug_config.h"

InvTQ::InvTQ(sc_core::sc_module_name name) : 
    sc_module(name), tq_socket("tq_socket"), db_socket("db_socket") {
    tq_socket.register_b_transport(this, &InvTQ::b_transport);  
}

//inverse quantization table
const int InvTQ::inverse_data_mux[6] = { 40, 45, 51, 57, 64, 72 };

void InvTQ::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
    // Unpack the incoming packet
    RecPacket pkt = unpackRecPacket(trans);

    // If this is a COEFF packet, perform inverse quantize + inverse transform
    if (pkt.cmd == RecCmd::COEFF) {
        int side = 4 << pkt.size;
        int N = side * side;

        // Parse little-endian int16 quantized coefficients from payload
        std::vector<int32_t> qcoeffs;
        qcoeffs.reserve(N);
        for (int i = 0; i < N; ++i) {
            int idx = i * 2;
            int16_t v = 0;
            if ((idx + 1) < (int)pkt.data.size()) {
                uint16_t lo = pkt.data[idx];
                uint16_t hi = pkt.data[idx + 1];
                v = static_cast<int16_t>((hi << 8) | lo);
            } else if (idx < (int)pkt.data.size()) {
                v = static_cast<int16_t>(pkt.data[idx]);
            }
            qcoeffs.push_back(static_cast<int32_t>(v));
        }

        // Inverse quantize
        std::vector<int16_t> deq;
        bool type_i = (pkt.pred_type == static_cast<uint8_t>(PredType::INTRA));
        inv_quantize(pkt.size, qcoeffs, pkt.qp, type_i, deq);

        // Inverse transform
        std::vector<int32_t> recon;
        inv_DCT(pkt.size, deq, recon);

        // Build RESIDUAL packet with reconstructed spatial samples (biased by +128)
        RecPacket out_pkt = pkt;
        out_pkt.cmd = RecCmd::RESIDUAL;
        out_pkt.data.clear();
        out_pkt.data.reserve(N);
        for (int i = 0; i < N; ++i) {
            int32_t val = (i < (int)recon.size()) ? recon[i] : 0;
            int32_t biased = val + 128;
            if (biased < 0) biased = 0;
            if (biased > 255) biased = 255;
            out_pkt.data.push_back(static_cast<uint8_t>(biased));
        }

        std::vector<uint8_t> buf = packRecPacket(out_pkt);

        if (cdc::components::verbose_enabled()) {
            std::cout << "------------Inverse TQ Packet (internal)----------------" << std::endl;
            std::cout << out_pkt;
        }
        tlm::tlm_generic_payload db_trans;
        db_trans.set_command(tlm::TLM_WRITE_COMMAND);
        db_trans.set_address(0);
        db_trans.set_data_ptr(buf.data());
        db_trans.set_data_length(buf.size());
        sc_core::sc_time d = sc_core::SC_ZERO_TIME;
        db_socket->b_transport(db_trans, d);
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return;
    }

    // Default: forward unchanged to DB
    db_socket->b_transport(trans, delay);
}

void InvTQ::inv_quantize(uint8_t size4x4, const std::vector<int32_t>& in, uint8_t qp, bool /*type_i*/, std::vector<int16_t>& out) {
    int side = 4 << size4x4;
    int N = side * side;
    out.assign(N, 0);

    int q = qp % 6;
    int p = qp / 6;
    int data_mux = inverse_data_mux[q];
    // data_mux_shift = data_mux << p
    int32_t data_mux_shift = (p >= 0) ? (data_mux << p) : data_mux;

    // inverse-specific small shifts and offsets
    int shift = 1;
    int32_t offset = 1;
    switch (size4x4) {
        case 0: shift = 1; offset = 1; break; // 4x4
        case 1: shift = 2; offset = 2; break; // 8x8
        case 2: shift = 3; offset = 4; break; // 16x16
        case 3: shift = 4; offset = 8; break; // 32x32
        default: shift = 1; offset = 1; break;
    }

    for (int i = 0; i < N && i < (int)in.size(); ++i) {
        int32_t qc = in[i];
        int sign = (qc < 0) ? -1 : 1;
        uint32_t abs_q = static_cast<uint32_t>(qc < 0 ? -qc : qc);
        int64_t m = static_cast<int64_t>(abs_q) * static_cast<int64_t>(data_mux_shift);
        int64_t w_a = m + static_cast<int64_t>(offset);
        int64_t w_s = (shift >= 0) ? (w_a >> shift) : w_a;
        // clamp to int16 range
        if (w_s > INT16_MAX) w_s = INT16_MAX;
        int64_t qv = w_s * sign;
        if (qv < INT16_MIN) qv = INT16_MIN;
        out[i] = static_cast<int16_t>(qv);
    }
}


// Inverse transform: optimized integer 4x4 inverse, generic floating-point IDCT for larger sizes
void InvTQ::inv_DCT(uint8_t size4x4, const std::vector<int16_t>& in, std::vector<int32_t>& out) {
    int side = 4 << size4x4;
    int N = side;
    out.assign(N * N, 0);

    if (size4x4 == 0) {
        // 4x4 inverse: integer path to mirror RecTQ::dct()
        int32_t tmp[4][4] = {};
        // invert vertical stage (operate per-column)
        for (int c = 0; c < 4; ++c) {
            int32_t o0 = (int32_t)in[0 * 4 + c];
            int32_t o1 = (int32_t)in[1 * 4 + c];
            int32_t o2 = (int32_t)in[2 * 4 + c];
            int32_t o3 = (int32_t)in[3 * 4 + c];

            int32_t b0 = (o0 + o2) >> 1;
            int32_t b1 = (o0 - o2) >> 1;
            int32_t b3 = (o1 + o3) >> 1;
            int32_t b2 = o1 - o3;

            tmp[0][c] = (b0 + b3) >> 1;
            tmp[3][c] = (b0 - b3) >> 1;
            tmp[1][c] = (b1 + b2) >> 1;
            tmp[2][c] = (b1 - b2) >> 1;
        }

        // invert horizontal stage (operate per-row)
        for (int r = 0; r < 4; ++r) {
            int32_t t0 = tmp[r][0];
            int32_t t1 = tmp[r][1];
            int32_t t2 = tmp[r][2];
            int32_t t3 = tmp[r][3];

            int32_t a0 = (t0 + t2) >> 1;
            int32_t a1 = (t0 - t2) >> 1;
            int32_t a3 = (t1 + t3) >> 1;
            int32_t a2 = t1 - t3;

            int32_t s0 = (a0 + a3) >> 1;
            int32_t s3 = (a0 - a3) >> 1;
            int32_t s1 = (a1 + a2) >> 1;
            int32_t s2 = (a1 - a2) >> 1;

            out[r * 4 + 0] = s0;
            out[r * 4 + 1] = s1;
            out[r * 4 + 2] = s2;
            out[r * 4 + 3] = s3;
        }
        return;
    }

    // Generic floating-point separable IDCT for NxN (N = 8,16,32)
    // Map input coefficients X[u][v] from `in` (row-major: u* N + v)
    std::vector<double> alpha(N);
    double invN = 1.0 / (double)N;
    alpha[0] = std::sqrt(invN);
    for (int k = 1; k < N; ++k) alpha[k] = std::sqrt(2.0 * invN);

    // Precompute cosine terms: cos(pi*(2*n+1)*k/(2N)) for n,k
    const double PI = std::acos(-1.0);
    std::vector<std::vector<double>> cos_term(N, std::vector<double>(N));
    for (int n = 0; n < N; ++n) {
        for (int k = 0; k < N; ++k) {
            cos_term[n][k] = std::cos(PI * (2.0 * n + 1.0) * k / (2.0 * N));
        }
    }

    // tmp[x][v] = sum_u alpha[u] * X[u][v] * cos((2x+1)u*pi/(2N))
    std::vector<double> tmp(N * N, 0.0);
    for (int v = 0; v < N; ++v) {
        for (int x = 0; x < N; ++x) {
            double sum = 0.0;
            for (int u = 0; u < N; ++u) {
                double Xuv = (u * N + v) < (int)in.size() ? static_cast<double>(in[u * N + v]) : 0.0;
                sum += alpha[u] * Xuv * cos_term[x][u];
            }
            tmp[x * N + v] = sum;
        }
    }

    // f[x][y] = sum_v alpha[v] * tmp[x][v] * cos((2y+1)v*pi/(2N))
    for (int x = 0; x < N; ++x) {
        for (int y = 0; y < N; ++y) {
            double sum = 0.0;
            for (int v = 0; v < N; ++v) {
                sum += alpha[v] * tmp[x * N + v] * cos_term[y][v];
            }
            long long val = llround(sum);
            // clamp to 32-bit signed range (shouldn't overflow in practice)
            if (val > INT32_MAX) val = INT32_MAX;
            if (val < INT32_MIN) val = INT32_MIN;
            out[x * N + y] = static_cast<int32_t>(val);
        }
    }
}
