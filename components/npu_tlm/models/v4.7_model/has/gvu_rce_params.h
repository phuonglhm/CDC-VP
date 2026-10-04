// gvu_rce_params.h -- DRAM parameter blocks of the two RCE instructions the DFC runs through the GVU
// (docs/SW_INTEGRATION_GUIDE.md, section 6.2). The compiler writes one block per instruction and gives its address in the extension
// register PARAM_ADDR (0x400004E0); the DFC fetches it over CH3 (like the LUT) and parses it here. Little endian, 4-byte words.
//
// FUSED_ATTN (opcode 0x13), block "ATN1", 14 words + 256-entry exp table = 1 080 bytes:
//   w0  magic 0x314E5441       w1..w5 Zq Zk Zv Zqk Zav (i32)   w6..w7 Mqk (i64)   w8 TSqk   w9..w10 Mav (i64)   w11 TSav
//   w12 flags: bit0 asym_zp, bit1 mask_e_zero (H22), bit2 av_zv_corr (H23)       w13 reserved (0)
//   w14..w269 LUT_exp[256] (i32, u8 values, R6). LUT_recip = has::LutIndirect(14), built in (as the unit gates).
// LAYERNORM (opcode 0x14), block "LNP1": w0 magic 0x31504E4C, w1 H, then 19 i64 scalars (w2..w39) in the order of
//   tools/has/export_gvu_ln.py: H Pre_Shift M0_var TS_var Z_var M0_7 TS_7 E_bias Z_7 M0_div TS_div Z_div Z_mul Z_out
//   r8_floor_z_out r8_z_out r9_exact r9_lsb_bits out_int16 ; then gamma i32[H], beta i64[H], M0_mul i64[H], TS_mul i32[H].
// Pure C++ (no SystemC).
#ifndef HAS_RCE_PARAMS_H
#define HAS_RCE_PARAMS_H

#include <cstdint>
#include <cstring>
#include <vector>
#include "has/gvu_fused_attn.h"
#include "has/gvu_layernorm.h"

namespace has
{
    constexpr uint32_t kAttnMagic = 0x314E5441u, kLnMagic = 0x31504E4Cu;
    constexpr uint32_t kAttnBlockBytes = 4 * (14 + 256);
    inline uint32_t ln_block_bytes(int H) { return 4 * 2 + 8 * 19 + uint32_t(H) * (4 + 8 + 8 + 4); }

    struct BlockReader
    {
        const std::vector<uint8_t> &d;
        uint32_t p;
        bool ok{true};
        uint32_t u32() { if (p + 4 > d.size()) { ok = false; return 0; } uint32_t v; std::memcpy(&v, &d[p], 4); p += 4; return v; }
        int32_t i32() { return static_cast<int32_t>(u32()); }
        int64_t i64() { if (p + 8 > d.size()) { ok = false; return 0; } int64_t v; std::memcpy(&v, &d[p], 8); p += 8; return v; }
    };

    inline bool parse_attn_block(const std::vector<uint8_t> &dram, uint32_t addr, AttnParams &p, std::vector<int32_t> &exp)
    {
        BlockReader r{dram, addr};
        if (r.u32() != kAttnMagic) return false;
        p.Zq = r.i32(); p.Zk = r.i32(); p.Zv = r.i32(); p.Zqk = r.i32(); p.Zav = r.i32();
        p.Mqk = r.i64(); p.TSqk = r.i32(); p.Mav = r.i64(); p.TSav = r.i32();
        const uint32_t f = r.u32();
        r.u32();
        p.asym_zp = f & 1u; p.mask_e_zero = (f >> 1) & 1u; p.av_zv_corr = (f >> 2) & 1u;
        exp.resize(256);
        for (auto &e : exp) e = r.i32();
        return r.ok;
    }

    inline bool parse_ln_block(const std::vector<uint8_t> &dram, uint32_t addr, LnParams &p)
    {
        BlockReader r{dram, addr};
        if (r.u32() != kLnMagic) return false;
        const int H = r.i32();
        int64_t s[19];
        for (auto &v : s) v = r.i64();
        if (H <= 0 || s[0] != H) return false;
        p.H = H; p.Pre_Shift = int(s[1]); p.M0_var = s[2]; p.TS_var = int(s[3]); p.Z_var = int(s[4]);
        p.M0_7 = s[5]; p.TS_7 = int(s[6]); p.E_bias = s[7]; p.Z_7 = int(s[8]); p.M0_div = s[9]; p.TS_div = int(s[10]);
        p.Z_div = int(s[11]); p.Z_mul = int(s[12]); p.Z_out = int(s[13]); p.r8_floor_z_out = s[14] != 0; p.r8_z_out = int(s[15]);
        p.r9_exact = s[16] != 0; p.r9_lsb_bits = int(s[17]); p.out_int16 = s[18] != 0;
        p.gamma.resize(H); p.beta.resize(H); p.M0_mul.resize(H); p.TS_mul.resize(H);
        for (auto &v : p.gamma) v = r.i32();
        for (auto &v : p.beta) v = r.i64();
        for (auto &v : p.M0_mul) v = r.i64();
        for (auto &v : p.TS_mul) v = r.i32();
        return r.ok;
    }
} // namespace has

#endif // HAS_RCE_PARAMS_H
