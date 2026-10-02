// libsauria_mem — C tensor<->DRAM pack/unpack for the SAURIA core (driver side).
// Ports data_helper.{pack_as_bytes, optimize_weight_tensor_shape, assign_dram_values}
// so a driver lays out activations/weights/preloads in DRAM and reads outputs back,
// bit-exact with SAURIA. Verified against Python initial_dram (tools/test_mem.cpp).
//
// DRAM layout: [A region][byte-align][B region][byte-align][C region]. A is the
// activation tensor [C_in,A_h,A_w] flattened C-order; B is the weight tensor
// reshaped to [K_ext,C_ext,c_til,B_h,B_w,k_til] then flattened; C is [C_out,C_h,C_w].
// Element widths: A=IA_W, B=IB_W, C=OC_W bits (from the target).

#ifndef LIBSAURIA_MEM_H
#define LIBSAURIA_MEM_H

#include <cstdint>
#include <vector>
#include <cmath>
#include <algorithm>
#include "fp16.h"          // sauria::fp16_t (SystemC-free)
#include "sauria_targets.h"

namespace sauria
{
    // Port of SAURIA data_helper.encode_FP — the CUSTOM FP encoding used for DRAM
    // (NOT IEEE): round-half-UP, denormal underflow flushes to zero, overflow
    // saturates to max magnitude (no inf/NaN), +0 canonical. For normal-range halves
    // this equals the IEEE bits, but denormals / exact ties differ, so the DRAM
    // packer MUST use this (the model's internal decode via fp16_t stays IEEE and
    // remains consistent because the two agree on the normal range).
    inline uint64_t sauria_fp_encode_bsc(double val, int MANT_BITS, int E_BITS)
    {
        int e_bias = (1 << (E_BITS - 1)) - 1;
        if (val == 0.0)
            return 0;
        int s = (val < 0.0) ? 1 : 0;
        double av = val < 0.0 ? -val : val;
        // SAURIA computes these in float16 (val is np.float16): np.log2 and the
        // division round to half precision, which shifts the exponent at denormal
        // boundaries. Replicate by rounding through fp16_t.
        double lg = (double)fp16_t::half_to_float(fp16_t::float_to_half((float)std::log2(av)));
        int e = (int)std::floor(lg);
        double div = (double)fp16_t::half_to_float(
            fp16_t::float_to_half((float)(av / std::pow(2.0, e))));
        e = e + e_bias;
        uint64_t m;
        if (e < 0) { return 0; } // underflow -> zero (s=0)
        else if (e > (1 << E_BITS) - 2)
        {
            e = (1 << E_BITS) - 2;
            m = (1ULL << MANT_BITS) - 1;
        }
        else
        {
            double approx = div - 1.0;
            m = 0;
            for (int j = 0; j < MANT_BITS; j++)
            {
                approx *= 2.0;
                int bit = (approx < 1.0) ? 0 : 1;
                if (bit) approx -= 1.0;
                m = (m << 1) | (uint64_t)bit;
            }
            if (approx * 2.0 >= 1.0) m += 1; // round to nearest (half up)
            if (m >= (1ULL << MANT_BITS)) { e += 1; m -= (1ULL << MANT_BITS); }
        }
        return ((uint64_t)s << (MANT_BITS + E_BITS)) | ((uint64_t)e << MANT_BITS) | m;
    }

    // Encode one element to its packed bit pattern. FP -> SAURIA custom half bits;
    // INT -> two's-complement truncated to bit_width.
    inline uint64_t sauria_enc_elem(double v, bool is_fp, int bit_width)
    {
        if (is_fp) // half: TOTAL=16, MANT=10, E=5
            return sauria_fp_encode_bsc(v, bit_width - 1 - 5, 5);
        int64_t iv = (int64_t)std::llround(v);
        uint64_t mask = (bit_width >= 64) ? ~0ULL : ((1ULL << bit_width) - 1ULL);
        return (uint64_t)iv & mask;
    }

    inline double sauria_dec_elem(uint64_t bits, bool is_fp, int bit_width)
    {
        if (is_fp)
            return (double)fp16_t::half_to_float((uint16_t)bits);
        // sign-extend from bit_width
        if (bit_width < 64)
        {
            uint64_t sign = 1ULL << (bit_width - 1);
            if (bits & sign)
                bits |= ~((1ULL << bit_width) - 1ULL);
        }
        return (double)(int64_t)bits;
    }

    // Port of pack_as_bytes: pack n elements LSB-first at start_bit, bit_width each.
    // Returns the ending bit index. mem is grown as needed.
    inline size_t sauria_pack_bits(std::vector<uint8_t> &mem, const uint64_t *vals,
                                   int n, size_t start_bit, int bit_width)
    {
        size_t bit_idx = start_bit;
        for (int e = 0; e < n; e++)
        {
            uint64_t el = vals[e];
            size_t end_bit = bit_idx + bit_width - 1;
            size_t start_byte = bit_idx / 8, end_byte = end_bit / 8;
            int written = 0, remaining = bit_width;
            for (size_t b = start_byte; b <= end_byte; b++)
            {
                if (b >= mem.size())
                    mem.resize(b + 1, 0u);
                int mem_offs = (int)(bit_idx % 8);
                int curr = std::min(std::min(bit_width, 8 - mem_offs), remaining);
                mem[b] |= (uint8_t)(((el >> written) << mem_offs) & 0xFF);
                bit_idx += curr;
                written += curr;
                remaining -= curr;
            }
        }
        return bit_idx;
    }

    inline void sauria_unpack_bits(const std::vector<uint8_t> &mem, uint64_t *vals,
                                   int n, size_t start_bit, int bit_width)
    {
        size_t bit_idx = start_bit;
        for (int e = 0; e < n; e++)
        {
            uint64_t v = 0;
            for (int b = 0; b < bit_width; b++)
            {
                size_t abs_bit = bit_idx + b;
                size_t byte = abs_bit / 8;
                int off = (int)(abs_bit % 8);
                uint64_t bit = (byte < mem.size()) ? ((mem[byte] >> off) & 1ULL) : 0ULL;
                v |= (bit << b);
            }
            vals[e] = v;
            bit_idx += bit_width;
        }
    }

    // Serialize weights [C_out,C_in,B_h,B_w] -> SAURIA order
    // [K_ext,C_ext,c_til,B_h,B_w,k_til] flattened. B(k,c,kh,kw) index into the flat
    // input B_kchw of size C_out*C_in*B_h*B_w (C-order).
    inline std::vector<uint64_t> sauria_weight_order(const double *B_kchw,
                                                     int C_out, int C_in, int B_h, int B_w,
                                                     int c_til, int k_til, bool is_fp, int bit_width)
    {
        int K_ext = C_out / k_til, C_ext = C_in / c_til;
        std::vector<uint64_t> out;
        out.reserve((size_t)C_out * C_in * B_h * B_w);
        for (int kext = 0; kext < K_ext; kext++)
            for (int cext = 0; cext < C_ext; cext++)
                for (int ct = 0; ct < c_til; ct++)
                    for (int kh = 0; kh < B_h; kh++)
                        for (int kw = 0; kw < B_w; kw++)
                            for (int kt = 0; kt < k_til; kt++)
                            {
                                int k = kext * k_til + kt, c = cext * c_til + ct;
                                size_t idx = (((size_t)k * C_in + c) * B_h + kh) * B_w + kw;
                                out.push_back(sauria_enc_elem(B_kchw[idx], is_fp, bit_width));
                            }
        return out;
    }

    inline size_t byte_align(size_t bit_idx) { return 8 * ((bit_idx + 7) / 8); }

    // Assemble the initial DRAM image (A + weights + C-preloads) exactly like
    // assign_dram_values. Inputs are raw element values (row-major). Returns the
    // byte image; out_offsets = {A_off, B_off, C_off} byte addresses.
    struct SauriaDramLayout
    {
        std::vector<uint8_t> dram;
        uint32_t A_off, B_off, C_off;
    };

    inline SauriaDramLayout sauria_assemble_dram(
        const double *A_chw, int A_c, int A_h, int A_w,
        const double *B_kchw, int C_out, int C_in, int B_h, int B_w,
        const double *C_khw, int C_c, int C_h, int C_w,
        int c_til, int k_til, const SauriaTarget &t)
    {
        const bool fp = (t.op_type == 1);
        SauriaDramLayout L;
        size_t bit = 0;

        // A: C-order flatten, IA_W bits
        int nA = A_c * A_h * A_w;
        std::vector<uint64_t> a(nA);
        for (int i = 0; i < nA; i++) a[i] = sauria_enc_elem(A_chw[i], fp, t.ia_w);
        L.A_off = (uint32_t)(bit / 8);
        bit = sauria_pack_bits(L.dram, a.data(), nA, bit, t.ia_w);
        bit = byte_align(bit);

        // B: SAURIA weight order, IB_W bits
        std::vector<uint64_t> b = sauria_weight_order(B_kchw, C_out, C_in, B_h, B_w,
                                                      c_til, k_til, fp, t.ib_w);
        L.B_off = (uint32_t)(bit / 8);
        bit = sauria_pack_bits(L.dram, b.data(), (int)b.size(), bit, t.ib_w);
        bit = byte_align(bit);

        // C: preloads, C-order flatten, OC_W bits
        int nC = C_c * C_h * C_w;
        std::vector<uint64_t> c(nC);
        for (int i = 0; i < nC; i++) c[i] = sauria_enc_elem(C_khw[i], fp, t.oc_w);
        L.C_off = (uint32_t)(bit / 8);
        bit = sauria_pack_bits(L.dram, c.data(), nC, bit, t.oc_w);
        return L;
    }

    // Read back the output C region (n elements) as decoded values.
    inline std::vector<double> sauria_unpack_output(const std::vector<uint8_t> &dram,
                                                    uint32_t C_off, int n,
                                                    const SauriaTarget &t)
    {
        std::vector<uint64_t> raw(n);
        sauria_unpack_bits(dram, raw.data(), n, (size_t)C_off * 8, t.oc_w);
        std::vector<double> out(n);
        for (int i = 0; i < n; i++)
            out[i] = sauria_dec_elem(raw[i], t.op_type == 1, t.oc_w);
        return out;
    }

} // namespace sauria

#endif // LIBSAURIA_MEM_H
