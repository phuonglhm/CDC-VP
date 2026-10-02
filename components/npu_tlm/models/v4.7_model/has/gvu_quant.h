// gvu_quant.h -- QUANTIZE arithmetic shared by every HAS pipeline (HW drawing "QUANTIZE", has/HAS_IFACE.md §3-§5).
// Pure functions, no SystemC: requant / dequant / round-shift / direct LUT, plus the knobs that parameterise the points the
// drawings leave open (H1 rounding, H2 int16 narrowing, H3 dequant zero-point order). Written from the spec, not from
// psm/obp_top.h or tools/fe/fe_ref_int8.py.
#ifndef HAS_QUANT_H
#define HAS_QUANT_H

#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace has
{
    enum RoundMode : int { FLOOR = 0, HALF_UP = 1, HALF_AWAY = 2, HALF_EVEN = 3 };
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
        // Vector-unit pipeline latencies (cycles from first read to write). Defaults = the dashed pipeline cuts of the drawings
        // (dashed cuts of the drawings). gvu_latencies() = DERIVED from GVU table 1 stage counts, upper bounds:
        //   OBP  = read reg 1 + Mul 32x32 5 + RoundShift 1 + Add zp 1 + Clamp 1 + LUT 1 = 10
        //   ADD1 = read reg 1 + Sub 1 + Mul 32x16 3 + RoundShift 1 + Add 1 + write 1  = 8
        //   ADD2 = read reg 1 + Mul 32x32 5 + RoundShift 1 + Add zp 1 + Clamp 1 + write 1 = 10
        //   MAX  = read reg 1 + Comparator 1 + write 1 = 3
        int lat_obp{6}, lat_add1{6}, lat_add2{5}, lat_max{3};
        void gvu_latencies() { lat_obp = 10; lat_add1 = 8; lat_add2 = 10; lat_max = 3; }
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
        static Knobs from_env()
        {
            const char *m = std::getenv("HAS_MODE");
            Knobs k = (m && std::string(m) == "compat") ? compat() : Knobs();
            auto get = [](const char *name, int &dst) { if (const char *v = std::getenv(name)) dst = std::atoi(v); };
            get("HAS_ROUND_MODE", k.round_mode);
            get("HAS_REQ_NARROW", k.req_narrow);
            get("HAS_DEQ_ZP_ORDER", k.deq_zp_order);
            get("HAS_SCALE_FMT", k.scale_fmt);
            get("HAS_MP_MODE", k.mp_mode);
            if (const char *v = std::getenv("HAS_SCRATCH_BYTES")) k.scratch_bytes = static_cast<uint32_t>(std::atol(v));
            get("HAS_SP_BANKED", k.sp_banked);
            get("HAS_ELEM_DMA_LAT", k.elem_dma_lat);
            if (const char *v = std::getenv("HAS_GVU_LAT")) if (std::atoi(v)) k.gvu_latencies();
            if (const char *v = std::getenv("HAS_SP_V0")) k.sp_v0 = static_cast<uint32_t>(std::atol(v));
            if (const char *v = std::getenv("HAS_SP_V1")) k.sp_v1 = static_cast<uint32_t>(std::atol(v));
            if (const char *v = std::getenv("HAS_SP_V2")) k.sp_v2 = static_cast<uint32_t>(std::atol(v));
            // HAS_SP_PRESET=gvu68k: banked Scratchpad with vbank#0 16 KB, vbank#1 16 KB, vbank#2 32 KB per half (a sizing
            // proposed for the vector unit; the specification leaves the capacity open, question H8)
            if (const char *v = std::getenv("HAS_SP_PRESET"))
                if (std::string(v) == "gvu68k") { k.sp_banked = 1; k.sp_v0 = 16384; k.sp_v1 = 16384; k.sp_v2 = 32768; }
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
                   (sp_banked ? " banked v0/v1/v2=" + std::to_string(sp_v0) + "/" + std::to_string(sp_v1) + "/" + std::to_string(sp_v2) : "");
        }
    };

    // Event counters (HAS_IFACE §4): evidence for H2 (narrowing) and output clamping.
    struct QuantCounters
    {
        uint64_t n{0}, sat16{0}, clamp8{0}, ovf64{0}, sat32{0};
        void add(const QuantCounters &o) { n += o.n; sat16 += o.sat16; clamp8 += o.clamp8; ovf64 += o.ovf64; sat32 += o.sat32; }
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
        const i128 y = n + zp;
        const i128 out = clamp128(y, -128, 127);
        if (c)
        {
            c->n++;
            if (p >= lim63 || p < -lim63) c->ovf64++;
            if (n != r) c->sat16++;
            if (out != y) c->clamp8++;
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
        if (c)
        {
            c->n++;
            if (out != v) c->sat32++;
        }
        return static_cast<int32_t>(out);
    }

    // Saturating int32 add (ELEM_WISE ADD stage 1 "Add v32int32").
    inline int32_t add_sat32(int32_t a, int32_t b, QuantCounters *c = nullptr)
    {
        const i128 v = static_cast<i128>(a) + b;
        const i128 out = clamp128(v, INT32_MIN, INT32_MAX);
        if (c && out != v) c->sat32++;
        return static_cast<int32_t>(out);
    }

    // Direct LUT (act / exp): one int8[256] table shared by all lanes, indexed by x + 128.
    inline int8_t lut_direct(const int8_t *lut, int x8) { return lut[x8 + 128]; }

} // namespace has

#endif // HAS_QUANT_H
