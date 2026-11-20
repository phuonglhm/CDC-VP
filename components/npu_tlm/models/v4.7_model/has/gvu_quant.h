// gvu_quant.h -- QUANTIZE arithmetic shared by every HAS pipeline (HW drawing "QUANTIZE", has/HAS_IFACE.md §3-§5).
// Pure functions, no SystemC: requant / dequant / round-shift / direct LUT, plus the knobs that parameterise the points the
// drawings leave open (H1 rounding, H2 int16 narrowing, H3 dequant zero-point order). Written from the spec, not from
// psm/obp_top.h or tools/fe/fe_ref_int8.py.
// Microarchitecture specification (MAS) V1.0, "Vector Round-Shift Unit": the rounding mode is selected per instruction among
// floor, ceil, LSB truncation, half away from zero, half to even and half up; the unit may saturate its output to a
// configured range (the int16 narrowing of the requant drawing). LSB truncation of a two's complement value is the
// arithmetic shift, so it shares FLOOR here. "Quantization": dequantize is x = S * (q - Z), the SUB_BEFORE order.
// RTL of the vector unit (gvu_vroundshift, gvu_adder32_lane, gvu_lut_lookup_lane, gvu_lut_mem): seven rounding modes --
// floor, ceil, toward zero (TRUNC), away from zero (AWAY), nearest with ties up / away / to even; the adders wrap, they do
// not saturate (knob zp_add_wrap16); the knobs that follow the RTL are grouped by Knobs::spec_rtl().
#ifndef HAS_QUANT_H
#define HAS_QUANT_H

#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include "has/gvu_range_probe.h"
#include "has/gvu_rtl_arith.h"

namespace has
{
    enum RoundMode : int { FLOOR = 0, HALF_UP = 1, HALF_AWAY = 2, HALF_EVEN = 3, CEIL = 4, TRUNC = 5, AWAY = 6 };
    enum ReqNarrow : int { SAT16 = 0, WRAP16 = 1, NARROW_NONE = 2 };
    enum DeqZpOrder : int { SUB_BEFORE = 0, ADD_AFTER = 1 };
    enum ScaleFmt : int { I32 = 0, U32 = 1 };
    enum MpMode : int { MP_DIRECT = 0, MP_SEPARABLE = 1 };

    struct Knobs
    {
        int round_mode{HALF_UP};
        int req_narrow{SAT16};
        int deq_zp_order{SUB_BEFORE};
        int scale_fmt{I32};
        int mp_mode{MP_DIRECT};
        uint32_t scratch_bytes{24576};
        // Banked scratchpad (vector-unit specification section 4, tables 3-5): Scratchpad as banks, every bank double-buffered against the DMA.
        // sp_banked = 0: one 24 KB block, DMA and compute serial. 1: per-half sizes below (H8 open; default
        // = the 24 KB split the ADD already used, x2 halves = 48 KB = HAS 6.12 Scratch), chunks pipelined.
        int sp_banked{0};
        uint32_t sp_v0{4096}, sp_v1{4096}, sp_v2{16384};   // bytes per half: vbank#0 (v32int8), vbank#1/#2 (v32int32)
        // Scalar bank (int32 dual-port: requant scale + shift, LayerNorm gamma + beta) bytes per half. 0 = not sized (the
        // pipelines never fail on it); the MAS preset sets it and has/gvu_sp_map.h checks the pipelines against it.
        uint32_t sp_s{0};
        // Vector-unit pipeline latencies (cycles from first read to write). Defaults = the dashed pipeline cuts of the drawings
        // (dashed cuts of the drawings). gvu_latencies() = DERIVED from GVU table 1 stage counts, upper bounds:
        //   OBP  = read reg 1 + Mul 32x32 5 + RoundShift 1 + Add zp 1 + Clamp 1 + LUT 1 = 10
        //   ADD1 = read reg 1 + Sub 1 + Mul 32x16 3 + RoundShift 1 + Add 1 + write 1  = 8
        //   ADD2 = read reg 1 + Mul 32x32 5 + RoundShift 1 + Add zp 1 + Clamp 1 + write 1 = 10
        //   MAX  = read reg 1 + Comparator 1 + write 1 = 3
        int lat_obp{6}, lat_add1{6}, lat_add2{5}, lat_max{3};
        void gvu_latencies() { lat_obp = 10; lat_add1 = 8; lat_add2 = 10; lat_max = 3; }
        // MAS V1.0 Scratchpad: SCRATCH_SRAM_SIZE 128 KB, every bank double-buffered, so 64 KB per half. The MAS gives the
        // total and the contents of each bank (table "Scratchpad SRAM banks capacity summary", typical case d_model 256,
        // about 60 KB) but no per-bank split; this preset takes the largest entry of each bank in that typical column:
        // vbank#0 24 576 B (32 rows x 768 int8; the table prints 24756), vbank#1 1 024 B, vbank#2 32 768 B, sbank 2 560 B
        // = 60 928 B of the 65 536 B half. Env HAS_SP_PRESET=mas128k. Not the default: the reference runs keep 4/4/16 KB.
        void scratch_mas128k() { sp_banked = 1; sp_v0 = 24576; sp_v1 = 1024; sp_v2 = 32768; sp_s = 2560; }
        static constexpr uint32_t kMasScratchHalfBytes = 65536;
        // Balanced split of the same 64 KB half, sized for a hidden size of 768 and for ELEM_WISE: vbank#0 10 784 B and
        // vbank#2 43 008 B hold 14 LAYERNORM rows of 768 (x int8 + variance, D int32), sbank 3 840 B holds gamma + beta of
        // 768 channels, vbank#1 takes the remaining 7 904 B (ADD chunk of 7 904 elements). Total 65 536 B. A SOFTMAX row
        // longer than 337 elements no longer runs 32 rows at once. Env HAS_SP_PRESET=mas128k_bal.
        void scratch_mas128k_bal() { sp_banked = 1; sp_v0 = 10784; sp_v1 = 7904; sp_v2 = 43008; sp_s = 3840; }
        // The balanced split with a scalar bank of int32 words (8 B per LAYERNORM channel, sp_s_word32): sbank 6 144 B for
        // 768 channels, vbank#0 10 784 B and vbank#2 43 008 B for 14 rows of 768, vbank#1 the remaining 5 600 B.
        // Total 65 536 B. This is the split of the default configuration (spec_default()). Env HAS_SP_PRESET=mas128k_bal8.
        void scratch_mas128k_bal8() { sp_banked = 1; sp_v0 = 10784; sp_v1 = 5600; sp_v2 = 43008; sp_s = 6144; }
        // The bank depths of the RTL (gvu_pkg.sv SBANK_DEPTH 768 words of 32 bits, VBANK0_DEPTH 800 rows of 256 bits,
        // VBANK1_DEPTH 32 and VBANK2_DEPTH 256 rows of 1024 bits), per buffer: vbank#0 25 600 B, vbank#1 4 096 B,
        // vbank#2 32 768 B, sbank 3 072 B = 65 536 B. Env HAS_SP_PRESET=rtl. Not the default.
        void scratch_rtl() { sp_banked = 1; sp_v0 = 25600; sp_v1 = 4096; sp_v2 = 32768; sp_s = 3072; }
        // Cycle estimates of SOFTMAX / LAYERNORM in the instruction path (has/has_npu_top.h):
        //   lut_stages     pipeline depth of LUT_recip in SOFTMAX stage 3. 4 = register cuts of the drawing (reference
        //                  runs); the MAS gives 6 for the PWL mode and 3 for the log-scale mode. Env HAS_LUT_STAGES.
        //   sp_limit_rows  0 = always 32 rows per group (reference runs); 1 = the rows per group are what the banked
        //                  Scratchpad holds (has/gvu_sp_map.h sp_max_rows() with sp_v0 / sp_v1 / sp_v2 / sp_s).
        //                  Env HAS_SP_LIMIT_ROWS.
        //   lut_mas_depth  0 = LUT_recip / LUT_rsqrt with 257 entries per segment (reference runs); 1 = the MAS depths, 256
        //                  and 512 entries, last entry held (has::LutIndirect mas_depth). Changes results on the inputs
        //                  whose mantissa index is 255. Env HAS_LUT_MAS_DEPTH.
        int lut_stages{4};
        int sp_limit_rows{0};
        int lut_mas_depth{0};
        // Behaviour read from the RTL of the vector unit (all 0 = reference runs):
        //   zp_add_wrap16  requant: the zero point is added by a 16-bit adder that wraps (no saturation), so a narrowed value
        //                  near +/-32767 plus the zero point changes sign before the int8 clamp. Counted in QuantCounters::zpwrap.
        //   lut_zero0      LUT_recip / LUT_rsqrt return 0 (with the input-zero flag) for an input of 0 instead of the
        //                  largest value.
        //   sp_s_word32    the int8 port of the scalar bank reads the low byte of an int32 word, so an int8 parameter
        //                  takes a whole word: LayerNorm gamma + beta need 8 B per channel instead of 5 B.
        // Env HAS_ZP_ADD_WRAP16, HAS_LUT_ZERO0, HAS_SP_S_WORD32.
        int zp_add_wrap16{0};
        int lut_zero0{0};
        int sp_s_word32{0};
        // Latencies with the pipeline depths of the RTL parameters as delivered (ESTIMATE, the stage composition is this
        // model's): multiplier 32x32 4 stages, 32x16 2 stages (the lower ends of the ranges of gvu_latencies()). Not part of
        // spec_rtl(): the run profiles of HasNpuTop set the latencies themselves (gvu_latencies()).
        void rtl_latencies() { lat_obp = 9; lat_add1 = 7; lat_add2 = 9; lat_max = 3; }
        // Everything the RTL of the functional units settles: table depths 256 / 512 with the last entry held, LUT_recip
        // 4 stages in the PWL mode, zero input, wrapping zero-point adder, scalar-bank words. The Scratchpad
        // capacities are still parameters in the RTL, so no preset is selected here. Env HAS_SPEC=rtl.
        void spec_rtl() { lut_mas_depth = 1; lut_stages = 4; lut_zero0 = 1; zp_add_wrap16 = 1; sp_s_word32 = 1; }
        // Default configuration of the instruction path (HasNpuTop run profiles other than Legacy): the MAS Scratchpad
        // (128 KB double-buffered, split mas128k_bal8), rows per group limited by it, and what the RTL of the functional units
        // settles -- LUT tables of 256 / 512 entries with the last entry held, LUT_recip 4 stages, LUT input 0 returns 0,
        // scalar bank of int32 words. The wrapping zero-point adder stays a knob (zp_add_wrap16). A default-constructed
        // Knobs keeps the earlier values, which HAS_SPEC=v47ref selects for the instruction path: 4 / 4 / 16 KB banks,
        // 32 rows per group, 257-entry tables -- the configuration of the results released before the MAS.
        void spec_default() { scratch_mas128k_bal8(); sp_limit_rows = 1; lut_mas_depth = 1; lut_stages = 4; lut_zero0 = 1; sp_s_word32 = 1; }
        // Every MAS V1.0 value this model keeps behind a knob, at once. Env HAS_SPEC=mas10.
        void spec_mas10() { scratch_mas128k(); lut_stages = 6; sp_limit_rows = 1; lut_mas_depth = 1; }
        // ELEM_WISE DMA: fixed cycles per transfer before the data (ESTIMATE, default 20). Env HAS_ELEM_DMA_LAT.
        int elem_dma_lat{20};

        // Compat = the v4.5 epilogue arithmetic: floor + uint32 scale.
        static Knobs compat()
        {
            Knobs k;
            k.round_mode = FLOOR;
            k.scale_fmt = U32;
            return k;
        }

        // HAS_MODE=compat|has selects the base; HAS_ROUND_MODE / HAS_REQ_NARROW / HAS_DEQ_ZP_ORDER / HAS_SCALE_FMT /
        // HAS_MP_MODE / HAS_SCRATCH_BYTES override single knobs (numbers as in HAS_IFACE §2).
        // mas_default = true (instruction path, profiles other than Legacy): start from spec_default() unless HAS_SPEC
        // names another group. HAS_SPEC = mas (spec_default) | v47ref (the earlier values) | mas10 | rtl (groups applied
        // on top of the earlier values). Single HAS_* variables override the group.
        static Knobs from_env(bool mas_default = false)
        {
            const char *m = std::getenv("HAS_MODE");
            Knobs k = (m && std::string(m) == "compat") ? compat() : Knobs();
            {
                const char *sp = std::getenv("HAS_SPEC");
                if (sp ? std::string(sp) == "mas" : mas_default) k.spec_default();
            }
            auto get = [](const char *name, int &dst) { if (const char *v = std::getenv(name)) dst = std::atoi(v); };
            get("HAS_ROUND_MODE", k.round_mode);
            get("HAS_REQ_NARROW", k.req_narrow);
            get("HAS_DEQ_ZP_ORDER", k.deq_zp_order);
            get("HAS_SCALE_FMT", k.scale_fmt);
            get("HAS_MP_MODE", k.mp_mode);
            if (const char *v = std::getenv("HAS_SCRATCH_BYTES")) k.scratch_bytes = static_cast<uint32_t>(std::atol(v));
            get("HAS_SP_BANKED", k.sp_banked);
            get("HAS_ELEM_DMA_LAT", k.elem_dma_lat);
            if (const char *v = std::getenv("HAS_SP_V0")) k.sp_v0 = static_cast<uint32_t>(std::atol(v));
            if (const char *v = std::getenv("HAS_SP_V1")) k.sp_v1 = static_cast<uint32_t>(std::atol(v));
            if (const char *v = std::getenv("HAS_SP_V2")) k.sp_v2 = static_cast<uint32_t>(std::atol(v));
            // HAS_SP_PRESET=gvu68k: banked Scratchpad with vbank#0 16 KB, vbank#1 16 KB, vbank#2 32 KB per half (a sizing
            // proposed for the vector unit; the specification leaves the capacity open, question H8)
            // HAS_SP_PRESET=mas128k: the Scratchpad of MAS V1.0 (128 KB double-buffered), see scratch_mas128k()
            if (const char *v = std::getenv("HAS_SP_PRESET"))
            {
                if (std::string(v) == "gvu68k") { k.sp_banked = 1; k.sp_v0 = 16384; k.sp_v1 = 16384; k.sp_v2 = 32768; }
                else if (std::string(v) == "mas128k") k.scratch_mas128k();
                else if (std::string(v) == "mas128k_bal") k.scratch_mas128k_bal();
                else if (std::string(v) == "mas128k_bal8") k.scratch_mas128k_bal8();
                else if (std::string(v) == "rtl") k.scratch_rtl();
            }
            // HAS_SPEC=mas10 sets the MAS group; the single knobs below still override it
            if (const char *v = std::getenv("HAS_SPEC"))
            {
                if (std::string(v) == "mas10") k.spec_mas10();
                else if (std::string(v) == "rtl") k.spec_rtl();
            }
            if (const char *v = std::getenv("HAS_SP_S")) k.sp_s = static_cast<uint32_t>(std::atol(v));
            get("HAS_LUT_STAGES", k.lut_stages);
            get("HAS_SP_LIMIT_ROWS", k.sp_limit_rows);
            get("HAS_LUT_MAS_DEPTH", k.lut_mas_depth);
            get("HAS_ZP_ADD_WRAP16", k.zp_add_wrap16);
            get("HAS_LUT_ZERO0", k.lut_zero0);
            get("HAS_SP_S_WORD32", k.sp_s_word32);
            return k;
        }

        std::string str() const
        {
            return "round=" + std::to_string(round_mode) + " narrow=" + std::to_string(req_narrow) +
                   " deq_zp=" + std::to_string(deq_zp_order) + " scale_fmt=" + std::to_string(scale_fmt) +
                   " mp=" + std::to_string(mp_mode) + " scratch=" + std::to_string(scratch_bytes) +
                   " lat obp/add1/add2/max=" + std::to_string(lat_obp) + "/" + std::to_string(lat_add1) + "/" +
                   std::to_string(lat_add2) + "/" + std::to_string(lat_max) +
                   (elem_dma_lat != 20 ? " elem_dma_lat=" + std::to_string(elem_dma_lat) : std::string()) +
                   (sp_banked ? " banked v0/v1/v2=" + std::to_string(sp_v0) + "/" + std::to_string(sp_v1) + "/" + std::to_string(sp_v2) : "") +
                   (sp_s ? " sbank=" + std::to_string(sp_s) : "") +
                   (lut_stages != 4 ? " lut_stages=" + std::to_string(lut_stages) : std::string()) +
                   (sp_limit_rows ? " sp_limit_rows=1" : "") + (lut_mas_depth ? " lut_mas_depth=1" : "") +
                   (zp_add_wrap16 ? " zp_add_wrap16=1" : "") + (lut_zero0 ? " lut_zero0=1" : "") + (sp_s_word32 ? " sp_s_word32=1" : "");
        }
    };

    // Event counters (HAS_IFACE §4): evidence for H2 (narrowing) and output clamping.
    struct QuantCounters
    {
        uint64_t n{0}, sat16{0}, clamp8{0}, ovf64{0}, sat32{0}, zpwrap{0};
        void add(const QuantCounters &o) { n += o.n; sat16 += o.sat16; clamp8 += o.clamp8; ovf64 += o.ovf64; sat32 += o.sat32; zpwrap += o.zpwrap; }
    };

    typedef __int128 i128;

    inline i128 clamp128(i128 v, i128 lo, i128 hi) { return v < lo ? lo : (v > hi ? hi : v); }

    // Round-shift (HAS_IFACE §3). s in [0, 63]; >> on signed __int128 is arithmetic with g++.
    inline i128 rshift(i128 p, int s, int mode)
    {
        if (s < 0 || s > 63)
            throw std::runtime_error("has::rshift: shift " + std::to_string(s) + " outside [0, 63]");
        if (s == 0)
            return p;
        const i128 half = static_cast<i128>(1) << (s - 1);
        switch (mode)
        {
        case FLOOR:
            return p >> s;
        case HALF_UP:
            return (p + half) >> s;
        case HALF_AWAY:
        {
            const i128 a = p < 0 ? -p : p;
            const i128 q = (a + half) >> s;
            return p < 0 ? -q : q;
        }
        case HALF_EVEN:
        {
            i128 q = p >> s;
            const i128 rem = p - (q << s); // in [0, 2^s)
            if (rem > half || (rem == half && (q & 1)))
                q += 1;
            return q;
        }
        case CEIL:
            return (p + ((static_cast<i128>(1) << s) - 1)) >> s;
        case TRUNC:
        {
            const i128 q = p >> s;
            return (p < 0 && (q << s) != p) ? q + 1 : q;
        }
        case AWAY:
        {
            const i128 q = p >> s;
            return (p > 0 && (q << s) != p) ? q + 1 : q;
        }
        }
        throw std::runtime_error("has::rshift: unknown round mode " + std::to_string(mode));
    }

    // Interpret the 32 raw bits a host wrote as Out_Scale / In_Scale according to SCALE_FMT.
    inline int64_t decode_scale(uint32_t bits, int fmt)
    {
        return fmt == U32 ? static_cast<int64_t>(bits) : static_cast<int64_t>(static_cast<int32_t>(bits));
    }

    // Requant (HAS_IFACE §4, drawing QUANTIZE top row): x * S -> round-shift -> narrow -> + zp -> clamp int8.
    inline int8_t requant(int64_t x, int64_t S, int s, int zp, const Knobs &k, QuantCounters *c = nullptr)
    {
        const i128 p = static_cast<i128>(x) * S;
        const i128 lim63 = static_cast<i128>(1) << 63;
        const i128 r = rshift(p, s, k.round_mode);
        i128 n = r;
        if (k.req_narrow == SAT16)
            n = clamp128(r, -32768, 32767);
        else if (k.req_narrow == WRAP16)
            n = static_cast<int16_t>(static_cast<uint16_t>(static_cast<uint64_t>(r & 0xFFFF)));
        i128 y = n + zp;
        bool wrapped = false;
        if (k.zp_add_wrap16 && k.req_narrow != NARROW_NONE && (y > 32767 || y < -32768))
        {
            y = static_cast<int16_t>(static_cast<uint16_t>(static_cast<uint64_t>(y & 0xFFFF)));
            wrapped = true;
        }
        const i128 out = clamp128(y, -128, 127);
        if (RangeProbe::on())
        {
            RangeProbe::req(RangeProbe::R_X, x); RangeProbe::req(RangeProbe::R_S, S); RangeProbe::req(RangeProbe::R_PROD, p);
            RangeProbe::req(RangeProbe::R_SHIFT, s); RangeProbe::req(RangeProbe::R_ROUNDED, r); RangeProbe::req(RangeProbe::R_SUM_ZP, n + zp);
        }
        if (c)
        {
            c->n++;
            if (wrapped) c->zpwrap++;
            if (p >= lim63 || p < -lim63) c->ovf64++;
            if (n != r) c->sat16++;
            if (out != y) c->clamp8++;
        }
        // HAS_ARITH_RTL: the value of the ported units replaces the result (the counters above stay those of the model)
        if (RtlArith::on())
        {
            int h = 0;
            if (RtlArith::requant(x, S, s, zp, k.round_mode, k.req_narrow == SAT16, static_cast<int>(out), &h)) return static_cast<int8_t>(h);
        }
        return static_cast<int8_t>(out);
    }

    // Dequant (HAS_IFACE §4, drawing QUANTIZE bottom row): int8 -> int32.
    inline int32_t dequant(int x8, int zp, int64_t S, int s, const Knobs &k, QuantCounters *c = nullptr)
    {
        const i128 lo = INT32_MIN, hi = INT32_MAX;
        i128 v;
        if (k.deq_zp_order == SUB_BEFORE)
            v = rshift(static_cast<i128>(x8 - zp) * S, s, k.round_mode);
        else
            v = rshift(static_cast<i128>(x8) * S, s, k.round_mode) + zp;
        const i128 out = clamp128(v, lo, hi);
        if (RangeProbe::on())
        {
            const int a = k.deq_zp_order == SUB_BEFORE ? x8 - zp : x8;
            RangeProbe::pt(RangeProbe::DEQ_A, a); RangeProbe::pt(RangeProbe::DEQ_S, S); RangeProbe::pt(RangeProbe::DEQ_PROD, static_cast<i128>(a) * S);
            RangeProbe::pt(RangeProbe::DEQ_SHIFT, s); RangeProbe::pt(RangeProbe::DEQ_V, v);
        }
        if (c)
        {
            c->n++;
            if (out != v) c->sat32++;
        }
        if (RtlArith::on())
        {
            int32_t h = 0;
            if (RtlArith::dequant(x8, zp, S, s, k.round_mode, k.deq_zp_order == SUB_BEFORE, static_cast<int32_t>(out), &h)) return h;
        }
        return static_cast<int32_t>(out);
    }

    // Saturating int32 add (ELEM_WISE ADD stage 1 "Add v32int32"). With HAS_ARITH_RTL the 32-bit adder of the ported
    // units is used, which wraps.
    inline int32_t add_sat32(int32_t a, int32_t b, QuantCounters *c = nullptr)
    {
        const i128 v = static_cast<i128>(a) + b;
        RangeProbe::pt(RangeProbe::ADD_SUM, v);
        const i128 out = clamp128(v, INT32_MIN, INT32_MAX);
        if (c && out != v) c->sat32++;
        if (RtlArith::on()) return RtlArith::add32(a, b, static_cast<int32_t>(out));
        return static_cast<int32_t>(out);
    }

    // Direct LUT (act / exp): one int8[256] table shared by all lanes, indexed by x + 128.
    inline int8_t lut_direct(const int8_t *lut, int x8) { return lut[x8 + 128]; }

} // namespace has

#endif // HAS_QUANT_H
