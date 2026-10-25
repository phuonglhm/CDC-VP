// rtl_ref_gvu_fu.h -- functional units of the vector unit, ported line by line from the RTL (neo_core/gvu): gvu_pkg.sv,
// pipeline_reg.sv and FUs/{clz, gvu_full_adder, gvu_adder32_lane, gvu_adder64_lane, gvu_vadder32, gvu_vadder64, gvu_vcmp32,
// gvu_vector_clamp, gvu_vector_shift, gvu_vroundshift, gvu_vroundshift64, gvu_vroundshift72, gvu_vmul32x32, gvu_vmul48x40}.
// Pure C++17, no SystemC. Every function names the .sv file and the signals it follows; widths, truncations and wrap-around
// are those of the RTL expressions, not of the mathematical operation. The functional model of the same operations is
// has/gvu_quant.h (namespace has); nothing here is used by the instruction path.
// Latencies: kLat* / lat_*() give the clock cycles from an accepted input beat to its output beat for the RTL parameters
// (FuParams holds them with the RTL defaults). DOUBLE_PIPE_REG = 1 selects spill_register_flushable, whose source is not
// part of the RTL delivered, so only DOUBLE_PIPE_REG = 0 is ported.
// Points where the RTL as written differs from the operation its comments describe are noted at the function; the value
// paths are ported as written.
#ifndef RTL_REF_GVU_FU_H
#define RTL_REF_GVU_FU_H

#include <array>
#include <cstdint>

namespace rtl_ref_gvu
{
    typedef __int128 i128;
    typedef unsigned __int128 u128;

    // ---- gvu_pkg.sv ----------------------------------------------------------------------------------------------------
    constexpr int GVU_LANES = 32, GVU_LANE_W = 32, GVU_PROD_W = 64;
    constexpr int VRF_ROW_BITS = 256, VRF_ROWS = 32, VRF_ROW_BYTES = VRF_ROW_BITS / 8;
    constexpr int GVU_MULB_A_W = 48, GVU_MULB_B_W = 40, GVU_MULB_OUT_W = 72, GVU_MULB_AUX_W = 32;
    constexpr int MULA_MAX_STAGES = 4, MULB_MAX_STAGES = 4;

    enum DataFormat : int { GVU_FMT_I8 = 0, GVU_FMT_I16 = 1, GVU_FMT_I32 = 2, GVU_FMT_I64 = 3 };   // gvu_data_format_t
    enum ShiftOp : int { GVU_SHIFT_LEFT = 0, GVU_SHIFT_RIGHT_LOGICAL = 1, GVU_SHIFT_RIGHT_ARITH = 2 }; // gvu_shift_op_t
    enum ElemW : int { EW_8 = 0, EW_16 = 1, EW_32 = 2 };                                           // elem_w_e
    enum CmpOp : int { CMP_MAX = 0, CMP_MIN = 1 };                                                 // cmp_op_e
    enum CmpMode : int { CMP_ELEMENTWISE = 0, CMP_ACCUMULATE = 1 };                                // cmp_mode_e
    enum RoundMode : int { RND_FLOOR = 0, RND_CEIL = 1, RND_TRUNC = 2, RND_AWAY = 3, RND_NEAR_UP = 4, RND_NEAR_AWAY = 5, RND_NEAR_EVEN = 6 }; // round_mode_e
    enum NarrowMode : int { NARROW_WRAP = 0, NARROW_SATURATE = 1 };                                // narrow_mode_e
    enum NarrowW : int { NW_32 = 0, NW_16 = 1, NW_8 = 2 };                                         // narrow_w_e
    enum AdderMode : int { ADD_64 = 0, ADD_32 = 1, ADD_16 = 2, ADD_8 = 3 };                        // adder_mode_type_e
    enum MulaMode : int { MA_32X32 = 0, MA_32X16 = 1, MA_32X8 = 2, MA_16X16 = 3 };                 // mula_mode_e
    enum MulbMode : int { MB_40X32 = 0, MB_32X32_16X8 = 1, MB_32X8 = 2, MB_3X16X8 = 3 };           // mulb_mode_e

    inline int narrow_bits(NarrowW w) { return w == NW_16 ? 16 : (w == NW_8 ? 8 : 32); }
    inline int mula_latency(MulaMode m) { return m == MA_32X32 ? 4 : ((m == MA_32X16 || m == MA_16X16) ? 2 : 1); }
    inline int mulb_latency(MulbMode m) { return (m == MB_40X32 || m == MB_32X32_16X8) ? 4 : 1; }
    inline int format_bits(DataFormat f) { return f == GVU_FMT_I8 ? 8 : (f == GVU_FMT_I16 ? 16 : 32); } // width_bits of clamp / shift

    // Module parameters that add or remove a pipeline register, with the RTL defaults.
    struct FuParams
    {
        bool ADD_64_PIPE{false};     // gvu_adder64_lane, gvu_vadder64: register between the two 32-bit halves in ADD_64 mode
        bool DOUBLE_PIPE_REG{false}; // pipeline_reg: spill register instead of the single register (not ported)
        bool CMP_OUT_PIPE{false};    // gvu_vcmp32 OUT_PIPE: output register
        bool RS_S2_PIPE{false};      // gvu_vroundshift S2_PIPE: register after the rounding stage
    };
    constexpr int kLatClz = 0, kLatAdder32 = 0, kLatClamp = 0, kLatShift = 0;
    inline int lat_adder64(AdderMode m, const FuParams &p = FuParams()) { return (m == ADD_64 && p.ADD_64_PIPE) ? 1 : 0; }
    inline int lat_vcmp32(const FuParams &p = FuParams()) { return p.CMP_OUT_PIPE ? 1 : 0; }
    // The header comment of gvu_vroundshift.sv says two cycles; with S2_PIPE = 0 (default, and what the 64 / 72 wrappers
    // instantiate) only the stage-1 register exists.
    inline int lat_vroundshift(const FuParams &p = FuParams()) { return 1 + (p.RS_S2_PIPE ? 1 : 0); }

    inline int32_t sext32(uint32_t v, int w) { return w >= 32 ? static_cast<int32_t>(v) : static_cast<int32_t>(v << (32 - w)) >> (32 - w); }
    inline uint32_t mask32(int w) { return w >= 32 ? 0xFFFFFFFFu : ((1u << w) - 1u); }

    // ---- pipeline_reg.sv (DOUBLE_PIPE_REG = 0) -------------------------------------------------------------------------
    // gen_bypass: wires. gen_reg: o_ready = (i_valid & i_ready) | ~i_valid; the register loads valid and data whenever
    // i_ready is high; i_flush and reset clear only o_valid.
    template <typename T> struct PipelineReg
    {
        bool bypass{false};
        bool valid{false};
        T data{};
        bool o_valid(bool i_valid) const { return bypass ? i_valid : valid; }
        const T &o_data(const T &i_data) const { return bypass ? i_data : data; }
        bool o_ready(bool i_valid, bool i_ready) const { return bypass ? i_ready : ((i_valid && i_ready) || !i_valid); }
        void tick(bool i_rstn, bool i_flush, bool i_valid, const T &i_data, bool i_ready)
        {
            if (bypass) return;
            if (!i_rstn || i_flush) valid = false;
            else if (i_ready) { valid = i_valid; data = i_data; }
        }
    };

    // ---- FUs/clz.sv ----------------------------------------------------------------------------------------------------
    // o_idx is the POSITION of the first set bit found (Mode 1: scanning from the MSB, Mode 0: from the LSB), not a count
    // of zeros; 0 when no bit is set. IdxWidth = clog2(DataWidth).
    struct ClzOut { uint32_t idx; bool all_zeros; };
    inline ClzOut clz(uint64_t i_data, int DataWidth, bool Mode)
    {
        ClzOut o{0, true};
        for (int i = 0; i < DataWidth; i++) if ((i_data >> i) & 1) o.all_zeros = false;
        if (Mode) { for (int i = DataWidth - 1; i >= 0; i--) if ((i_data >> i) & 1) { o.idx = static_cast<uint32_t>(i); break; } }
        else      { for (int i = 0; i < DataWidth; i++)      if ((i_data >> i) & 1) { o.idx = static_cast<uint32_t>(i); break; } }
        return o;
    }

    // ---- FUs/gvu_full_adder.sv -----------------------------------------------------------------------------------------
    // r = i_a + i_b + i_c on DW + 1 bits; o_r = r[DW-1:0], o_c = r[DW]. DW <= 32 here.
    struct FaOut { uint32_t r; bool c; };
    inline FaOut full_adder(uint32_t i_a, uint32_t i_b, bool i_c, int DW)
    {
        const uint64_t r = static_cast<uint64_t>(i_a & mask32(DW)) + (i_b & mask32(DW)) + (i_c ? 1u : 0u);
        return {static_cast<uint32_t>(r) & mask32(DW), ((r >> DW) & 1) != 0};
    }

    // ---- FUs/gvu_adder32_lane.sv ---------------------------------------------------------------------------------------
    // Four 8-bit full adders with a configurable carry chain. Subtraction negates b per element (~b + 1 on the element
    // width), so every element wraps on its own width: there is no saturation and no carry / overflow output.
    // i_cfg_mode = ADD_64 takes no branch: a plain 32-bit add, the sub inputs are ignored.
    // The valid / ready ports are wires (o_*_valid = i_*_valid, o_*_ready = i_*_ready): latency 0.
    inline uint32_t adder32_lane(AdderMode i_cfg_mode, unsigned i_sub8_mode, unsigned i_sub16_mode, bool i_sub32_mode, uint32_t i_a, uint32_t i_b)
    {
        const uint32_t a = i_a;
        uint32_t b = i_b;
        bool chain[4] = {false, true, true, true};   // c_i = {c_o[2:0], 1'b0}
        if (i_cfg_mode == ADD_32)
            b = i_sub32_mode ? ~i_b + 1u : i_b;
        else if (i_cfg_mode == ADD_16)
        {
            const uint32_t lo = (i_sub16_mode & 1) ? (~i_b + 1u) & 0xFFFFu : (i_b & 0xFFFFu);
            const uint32_t hi = (i_sub16_mode & 2) ? (~(i_b >> 16) + 1u) & 0xFFFFu : ((i_b >> 16) & 0xFFFFu);
            b = (hi << 16) | lo;
            chain[2] = false;                        // c_i[2] = 1'b0
        }
        else if (i_cfg_mode == ADD_8)
        {
            b = 0;
            for (int i = 0; i < 4; i++)
            {
                const uint32_t e = (i_b >> (8 * i)) & 0xFFu;
                b |= (((i_sub8_mode >> i) & 1) ? (~e + 1u) & 0xFFu : e) << (8 * i);
            }
            chain[1] = chain[2] = chain[3] = false;  // c_i = '0
        }
        uint32_t o_r = 0;
        bool c_o = false;
        for (int i = 0; i < 4; i++)                  // gen_8b_adder
        {
            const FaOut f = full_adder((a >> (8 * i)) & 0xFFu, (b >> (8 * i)) & 0xFFu, chain[i] ? c_o : false, 8);
            o_r |= f.r << (8 * i);
            c_o = f.c;
        }
        return o_r;
    }

    // ---- FUs/gvu_adder64_lane.sv ---------------------------------------------------------------------------------------
    // Two 32-bit full adders; the value below is o_r of the combinational path (ADD_64_PIPE = 0, and every mode other
    // than ADD_64 when ADD_64_PIPE = 1). ADD_32 cuts the carry between the halves; other mode values chain it.
    // The always_comb block assigns "b = b_0" BEFORE the mode branches negate b_0, and the adders take b: on this path the
    // operand is always i_b, so i_sub32_mode and i_sub64_mode have no effect and the unit adds. Only the registered path
    // of ADD_64_PIPE = 1 uses the negated b_0 (Adder64Lane). Confirmed on the RTL simulation: every add beat matches,
    // every beat that differs from a - b is a sub beat.
    inline uint64_t adder64_lane(AdderMode i_cfg_mode, unsigned i_sub32_mode, bool i_sub64_mode, uint64_t i_a, uint64_t i_b)
    {
        (void)i_sub32_mode;
        (void)i_sub64_mode;
        const bool chain1 = i_cfg_mode != ADD_32;    // c_i = {c_o[0], 1'b0}; ADD_32: c_i[1] = 1'b0
        const FaOut f0 = full_adder(static_cast<uint32_t>(i_a), static_cast<uint32_t>(i_b), false, 32);
        const FaOut f1 = full_adder(static_cast<uint32_t>(i_a >> 32), static_cast<uint32_t>(i_b >> 32), chain1 ? f0.c : false, 32);
        return (static_cast<uint64_t>(f1.r) << 32) | f0.r;
    }

    // Cycle model of gvu_adder64_lane for the 64-bit interface. ADD_64_PIPE = 1 and ADD_64 mode: the low adder works on
    // the input beat, pipe_data_in = {c_o[0], r[31:0], b_0[63:32], i_a[63:32]} is registered, the high adder works on the
    // registered operands and carry, o_r = {r[63:32], pipe_data_out[95:64]}. Otherwise the register is bypassed.
    // The 32-bit interface (o_32b_valid = i_32b_valid) is combinational in every configuration.
    class Adder64Lane
    {
    public:
        struct Pipe { bool c; uint32_t r_lo, b0_hi, a_hi; };
        struct Out { bool o_64b_ready, o_64b_valid; uint64_t o_r; };
        explicit Adder64Lane(const FuParams &p = FuParams()) : P_(p) { reg_.bypass = !p.ADD_64_PIPE; }
        Out eval(AdderMode mode, bool sub64, bool i_64b_valid, uint64_t i_a, uint64_t i_b, bool i_64b_ready) const
        {
            Out o;
            const Pipe in = pipe_in(mode, sub64, i_a, i_b);
            o.o_64b_valid = reg_.o_valid(i_64b_valid);
            o.o_64b_ready = reg_.o_ready(i_64b_valid, i_64b_ready);
            if (P_.ADD_64_PIPE && mode == ADD_64)
            {
                const Pipe &q = reg_.o_data(in);
                o.o_r = (static_cast<uint64_t>(full_adder(q.a_hi, q.b0_hi, q.c, 32).r) << 32) | q.r_lo;
            }
            else
                o.o_r = adder64_lane(mode, 0, sub64, i_a, i_b);
            return o;
        }
        void tick(bool i_rstn, bool i_pipe_flush, AdderMode mode, bool sub64, bool i_64b_valid, uint64_t i_a, uint64_t i_b, bool i_64b_ready)
        {
            reg_.tick(i_rstn, i_pipe_flush, i_64b_valid, pipe_in(mode, sub64, i_a, i_b), i_64b_ready);
        }

    private:
        FuParams P_;
        PipelineReg<Pipe> reg_;
        static Pipe pipe_in(AdderMode mode, bool sub64, uint64_t i_a, uint64_t i_b)
        {
            const uint64_t b_0 = (mode == ADD_64 && sub64) ? ~i_b + 1u : i_b;
            const FaOut f0 = full_adder(static_cast<uint32_t>(i_a), static_cast<uint32_t>(b_0), false, 32);
            return {f0.c, f0.r, static_cast<uint32_t>(b_0 >> 32), static_cast<uint32_t>(i_a >> 32)};
        }
    };

    // ---- FUs/gvu_vadder32.sv, gvu_vadder64.sv ------------------------------------------------------------------------
    // NUM_LANE copies of the lane adder sharing every control input.
    typedef std::array<uint32_t, GVU_LANES> V32;
    typedef std::array<uint64_t, GVU_LANES> V64;
    inline V32 vadder32(AdderMode m, unsigned sub8, unsigned sub16, bool sub32, const V32 &a, const V32 &b)
    {
        V32 r{};
        for (int l = 0; l < GVU_LANES; l++) r[l] = adder32_lane(m, sub8, sub16, sub32, a[l], b[l]);
        return r;
    }
    inline V64 vadder64(AdderMode m, unsigned sub32, bool sub64, const V64 &a, const V64 &b)
    {
        V64 r{};
        for (int l = 0; l < GVU_LANES; l++) r[l] = adder64_lane(m, sub32, sub64, a[l], b[l]);
        return r;
    }

    // ---- FUs/gvu_vcmp32.sv ---------------------------------------------------------------------------------------------
    // ext8 / ext16 / ext32: every element becomes a 33-bit signed value whose sign bit is (is_signed & msb).
    // take_a: CMP_MAX takes a when a >= b, CMP_MIN when a <= b (a wins ties). in_elem_w = 3 is treated as EW_32 (default).
    inline uint32_t lane_minmax(uint32_t a, uint32_t b, ElemW ew, bool is_signed, CmpOp op)
    {
        const int w = ew == EW_8 ? 8 : (ew == EW_16 ? 16 : 32);
        uint32_t res = 0;
        for (int e = 0; e < 32 / w; e++)
        {
            const uint32_t av = (a >> (e * w)) & mask32(w), bv = (b >> (e * w)) & mask32(w);
            const int64_t ae = is_signed ? static_cast<int64_t>(sext32(av, w)) : static_cast<int64_t>(av);
            const int64_t be = is_signed ? static_cast<int64_t>(sext32(bv, w)) : static_cast<int64_t>(bv);
            const bool take_a = (op == CMP_MAX) ? (ae >= be) : (ae <= be);
            res |= (take_a ? av : bv) << (e * w);
        }
        return res;
    }

    // Cycle model. CMP_ELEMENTWISE: out = minmax(src0, src1), src1 lane 0 broadcast when in_src1_bcast. CMP_ACCUMULATE:
    // per-lane running result in acc_q, loaded from src0 on in_first, emitted on the beat that carries in_last.
    // OUT_PIPE = 0 (default): in_ready = out_ready, out_valid = in_valid & out_ready & emit, the result is combinational;
    // pipe_en = ~out_valid | out_ready is then always 1, so a beat is accepted only while the consumer is ready and an
    // accumulate beat without in_last is also held back while out_ready = 0. OUT_PIPE = 1: in_ready = pipe_en, output
    // register, one cycle (the "Latency: one cycle" of the file header). Out::unstable is kept for callers and is always
    // false (an earlier RTL had a combinational loop here).
    // Per-lane mode (latest hardware RTL drop, in_lane_mode = 1, used by MAXPOOL): every lane has its own valid / ready / first / last
    // and its own accumulator update; the shared in_valid / in_ready path is then closed (in_ready = 0). in_lane_ready =
    // out_lane_ready (OUT_PIPE = 0) or the lane's own pipe enable (OUT_PIPE = 1); a lane emits on in_lane_last (every
    // accepted beat in CMP_ELEMENTWISE). With in_lane_mode = 0 nothing changes from the shared mode above. OUT_PIPE = 1
    // keeps out_valid_q while in per-lane mode, as the RTL does (B10 of the review 1051; gvu_top builds OUT_PIPE = 0).
    class Vcmp32
    {
    public:
        struct In
        {
            bool in_valid; CmpMode in_cmp_mode; CmpOp in_op; ElemW in_elem_w; bool in_signed, in_src1_bcast, in_first, in_last; V32 in_src0, in_src1; bool out_ready;
            bool in_lane_mode{false};
            uint32_t in_lane_valid{0}, in_lane_first{0}, in_lane_last{0}, out_lane_ready{0};
        };
        struct Out { bool in_ready, out_valid, unstable; V32 out_res; uint32_t in_lane_ready{0}, out_lane_valid{0}; };
        explicit Vcmp32(const FuParams &p = FuParams()) : P_(p) { acc_q_.fill(0); out_res_q_.fill(0); }
        void reset() { acc_q_.fill(0); out_valid_q_ = false; lane_out_valid_q_ = 0; }
        V32 result(const In &i) const
        {
            V32 r{};
            for (int l = 0; l < GVU_LANES; l++)
            {
                const uint32_t src1_eff = i.in_src1_bcast ? i.in_src1[0] : i.in_src1[l];
                const bool first = i.in_lane_mode ? ((i.in_lane_first >> l) & 1u) != 0 : i.in_first;
                const uint32_t cmp_b = (i.in_cmp_mode == CMP_ELEMENTWISE) ? src1_eff : (first ? i.in_src0[l] : acc_q_[l]);
                r[l] = lane_minmax(i.in_src0[l], cmp_b, i.in_elem_w, i.in_signed, i.in_op);
            }
            return r;
        }
        Out eval(const In &i) const
        {
            Out o{};
            const bool emit = (i.in_cmp_mode == CMP_ELEMENTWISE) || i.in_last;
            const uint32_t lane_on = i.in_lane_mode ? 0xFFFFFFFFu : 0u;
            const uint32_t lane_emit = (i.in_cmp_mode == CMP_ELEMENTWISE) ? 0xFFFFFFFFu : i.in_lane_last;
            if (P_.CMP_OUT_PIPE)
            {
                o.in_ready = (!out_valid_q_ || i.out_ready) && !i.in_lane_mode;
                o.out_valid = out_valid_q_;
                o.out_res = out_res_q_;
                o.in_lane_ready = (~lane_out_valid_q_ | i.out_lane_ready) & lane_on;
                o.out_lane_valid = lane_out_valid_q_;
            }
            else
            {
                o.unstable = false;
                o.in_ready = i.out_ready && !i.in_lane_mode;
                o.out_valid = i.in_valid && o.in_ready && emit;
                o.out_res = result(i);
                o.in_lane_ready = i.out_lane_ready & lane_on;
                o.out_lane_valid = i.in_lane_valid & o.in_lane_ready & lane_emit;
            }
            return o;
        }
        void tick(const In &i)
        {
            const Out o = eval(i);
            const bool pipe_en = P_.CMP_OUT_PIPE ? (!out_valid_q_ || i.out_ready) : true, accept = i.in_valid && o.in_ready;
            const bool emit = (i.in_cmp_mode == CMP_ELEMENTWISE) || i.in_last;
            const V32 r = result(i);
            if (!i.in_lane_mode)
            {
                if (P_.CMP_OUT_PIPE && pipe_en) { out_valid_q_ = accept && emit; if (accept) out_res_q_ = r; }
                if (pipe_en && accept && i.in_cmp_mode == CMP_ACCUMULATE) acc_q_ = r;
                return;
            }
            const uint32_t lane_emit = (i.in_cmp_mode == CMP_ELEMENTWISE) ? 0xFFFFFFFFu : i.in_lane_last;
            const uint32_t lane_accept = i.in_lane_valid & o.in_lane_ready;
            // OUT_PIPE = 0: lane_out_valid_q is lane_accept & lane_emit, so the lane pipe enable reduces to lane_accept
            const uint32_t lane_pipe_en = P_.CMP_OUT_PIPE ? (~lane_out_valid_q_ | i.out_lane_ready) : 0xFFFFFFFFu;
            for (int l = 0; l < GVU_LANES; l++)
            {
                const uint32_t b = 1u << l;
                if (!(lane_pipe_en & b)) continue;
                if (P_.CMP_OUT_PIPE)
                {
                    lane_out_valid_q_ = (lane_accept & lane_emit & b) ? (lane_out_valid_q_ | b) : (lane_out_valid_q_ & ~b);
                    if (lane_accept & b) out_res_q_[l] = r[l];
                }
                if ((lane_accept & b) && i.in_cmp_mode == CMP_ACCUMULATE) acc_q_[l] = r[l];
            }
        }
        const V32 &acc() const { return acc_q_; }

    private:
        FuParams P_;
        V32 acc_q_, out_res_q_;
        bool out_valid_q_{false};
        uint32_t lane_out_valid_q_{0};
    };

    // ---- FUs/gvu_vector_clamp.sv, module gvu_vector_clamp32 ------------------------------------------------------------
    // One lane, signed 32 bits, bounds shared by the lanes: in > hi -> hi, else in < lo -> lo, else in. The hi test has
    // priority, so lo > hi returns hi for in > hi and lo otherwise-below. Combinational: valid and ready pass through.
    inline int32_t clamp32_lane(int32_t i_clamp_in, int32_t i_clamp_lo, int32_t i_clamp_hi)
    {
        return i_clamp_in > i_clamp_hi ? i_clamp_hi : (i_clamp_in < i_clamp_lo ? i_clamp_lo : i_clamp_in);
    }

    // ---- clamp with a lane format (the module of earlier RTL revisions; the RTL now has gvu_vector_clamp32 only) -------
    // Used by the pipelines of this model on signed lanes with lo <= hi, where it equals clamp32_lane on the sign-extended
    // lane (tools/has/tb_rtl_ref_gvu_fu.cpp checks this).
    // One logical lane of width_bits (8 / 16 / 32): a < lo -> lo, else a > hi -> hi, else a. The lo test has priority, so
    // lo > hi returns lo for a < lo and hi otherwise-above. signed_mode compares the sign-extended lanes, else unsigned.
    inline uint32_t clamp_lane(uint32_t lane_a, uint32_t lane_lo, uint32_t lane_hi, int width_bits, bool signed_mode)
    {
        const uint32_t m = mask32(width_bits), a = lane_a & m, lo = lane_lo & m, hi = lane_hi & m;
        const bool lt = signed_mode ? (sext32(a, width_bits) < sext32(lo, width_bits)) : (a < lo);
        const bool gt = signed_mode ? (sext32(a, width_bits) > sext32(hi, width_bits)) : (a > hi);
        return lt ? lo : (gt ? hi : a);
    }
    // The module: lanes are packed at i * width_bits on a 1024-bit bus (here: one array element per logical lane holding
    // width_bits). GVU_FMT_I64 raises rsp_error and drives y = 0. req_ready = rsp_ready, rsp_valid = req_valid: latency 0.
    inline bool vector_clamp(DataFormat format, bool signed_mode, const V32 &a, const V32 &lo, const V32 &hi, V32 &y)
    {
        y.fill(0);
        if (format == GVU_FMT_I64) return true;      // rsp_error
        for (int i = 0; i < GVU_LANES; i++) y[i] = clamp_lane(a[i], lo[i], hi[i], format_bits(format), signed_mode);
        return false;
    }

    // ---- FUs/gvu_vector_shift.sv ---------------------------------------------------------------------------------------
    // amount is 6 bits and shared by the lanes. Left and logical right shift work on the lane width, so an amount at or
    // above the width gives 0; the arithmetic right shift works on the lane sign-extended to 32 bits and keeps the low
    // width_bits, so a large amount gives the sign. op = 3 takes the default branch (arithmetic right shift).
    inline uint32_t shift_lane(uint32_t lane_a, int width_bits, ShiftOp op, unsigned amount)
    {
        const uint32_t m = mask32(width_bits), a = lane_a & m;
        amount &= 63u;
        if (op == GVU_SHIFT_LEFT) return amount >= 32 ? 0u : ((a << amount) & m);
        if (op == GVU_SHIFT_RIGHT_LOGICAL) return amount >= 32 ? 0u : (a >> amount);
        return static_cast<uint32_t>(sext32(a, width_bits) >> (amount > 31 ? 31 : amount)) & m;
    }
    inline bool vector_shift(DataFormat format, ShiftOp op, unsigned amount, const V32 &a, V32 &y)
    {
        y.fill(0);
        if (format == GVU_FMT_I64) return true;      // rsp_error
        for (int i = 0; i < GVU_LANES; i++) y[i] = shift_lane(a[i], format_bits(format), op, amount);
        return false;
    }

    // ---- FUs/gvu_vroundshift.sv (IN_W = 64: gvu_vroundshift64, IN_W = 72: gvu_vroundshift72) ---------------------------
    // One lane. in_data holds the IN_W-bit two's complement input in its low bits. in_shift has clog2(IN_W) bits: 6 for
    // IN_W = 64 (0..63), 7 for IN_W = 72 (0..127; at 72 and above every input bit is remainder and the floor is the sign).
    //   stage 1  mask = 2^S - 1, half = 2^(S-1) (both 0 when S = 0); trunc = x >>> S; rem = x & mask;
    //            rem_nz, rem_gt_h = rem > half, rem_eq_h = (rem == half) && S != 0, neg = x[IN_W-1]
    //   stage 2  inc by rounding mode; y = trunc + inc on IN_W bits; hi = y > lim_hi, lo = y < lim_lo for the target width;
    //            wrapped = y truncated to the target and sign-extended to 32 bits; out_ovf = hi | lo in both narrow modes;
    //            NARROW_SATURATE replaces the result by the limit, NARROW_WRAP keeps wrapped.
    // in_round_mode = 7 gives inc = 0 (floor).
    struct RsOut { uint32_t out_data; bool out_ovf; };
    inline RsOut vroundshift_lane(u128 in_data, int IN_W, unsigned in_shift, RoundMode in_round_mode, NarrowMode in_narrow_mode, NarrowW in_narrow_w)
    {
        const int SH_W = IN_W > 64 ? 7 : 6;                                    // $clog2(IN_W) for 64 and 72
        const unsigned S = in_shift & ((1u << SH_W) - 1u);
        const u128 M = (static_cast<u128>(1) << IN_W) - 1;
        const u128 d = in_data & M;
        auto one_shl = [&](unsigned n) -> u128 { return n >= static_cast<unsigned>(IN_W) ? 0 : (static_cast<u128>(1) << n); }; // IN_W'(1) << n
        const u128 mask = S == 0 ? 0 : ((one_shl(S) - 1) & M);
        const u128 half = S == 0 ? 0 : one_shl(S - 1);
        const bool neg = ((d >> (IN_W - 1)) & 1) != 0;
        const i128 x = neg ? static_cast<i128>(d | ~M) : static_cast<i128>(d);  // $signed(in_data[l])
        const u128 rem = d & mask;
        const i128 s1_trunc = S >= static_cast<unsigned>(IN_W) ? (neg ? static_cast<i128>(-1) : 0) : (x >> S);
        const bool rem_nz = rem != 0, rem_gt_h = rem > half, rem_eq_h = (rem == half) && S != 0;
        bool inc = false;
        switch (in_round_mode)
        {
        case RND_FLOOR:     inc = false; break;
        case RND_CEIL:      inc = rem_nz; break;
        case RND_TRUNC:     inc = neg && rem_nz; break;
        case RND_AWAY:      inc = !neg && rem_nz; break;
        case RND_NEAR_UP:   inc = rem_gt_h || rem_eq_h; break;
        case RND_NEAR_AWAY: inc = rem_gt_h || (rem_eq_h && !neg); break;
        case RND_NEAR_EVEN: inc = rem_gt_h || (rem_eq_h && (s1_trunc & 1)); break;
        default:            inc = false; break;
        }
        const u128 yu = (static_cast<u128>(s1_trunc) + (inc ? 1 : 0)) & M;      // IN_W-bit add
        const i128 y = ((yu >> (IN_W - 1)) & 1) ? static_cast<i128>(yu | ~M) : static_cast<i128>(yu);
        const int nwb = narrow_bits(in_narrow_w);
        const i128 lim_hi = (static_cast<i128>(1) << (nwb - 1)) - 1, lim_lo = -(static_cast<i128>(1) << (nwb - 1));
        const bool hi = y > lim_hi, lo = y < lim_lo;
        const uint32_t y32 = static_cast<uint32_t>(yu);
        const uint32_t wrapped = in_narrow_w == NW_16 ? static_cast<uint32_t>(sext32(y32 & 0xFFFFu, 16))
                               : in_narrow_w == NW_8 ? static_cast<uint32_t>(sext32(y32 & 0xFFu, 8)) : y32;
        RsOut o{wrapped, hi || lo};
        if (in_narrow_mode == NARROW_SATURATE)
        {
            if (in_narrow_w == NW_16)     o.out_data = hi ? 0x00007FFFu : (lo ? 0xFFFF8000u : wrapped);
            else if (in_narrow_w == NW_8) o.out_data = hi ? 0x0000007Fu : (lo ? 0xFFFFFF80u : wrapped);
            else                          o.out_data = hi ? 0x7FFFFFFFu : (lo ? 0x80000000u : wrapped);
        }
        return o;
    }
    inline RsOut vroundshift64_lane(uint64_t in_data, unsigned in_shift, RoundMode rm, NarrowMode nm, NarrowW nw) { return vroundshift_lane(in_data, 64, in_shift, rm, nm, nw); }
    inline RsOut vroundshift72_lane(u128 in_data, unsigned in_shift, RoundMode rm, NarrowMode nm, NarrowW nw) { return vroundshift_lane(in_data, 72, in_shift, rm, nm, nw); }

    // ---- FUs/gvu_vmul32x32.sv ------------------------------------------------------------------------------------------
    // mul17: both 16-bit operands widened to 17 bits (sign bit = signed flag & bit 15), 34-bit signed product.
    inline int64_t mul17(uint32_t a, bool a_signed, uint32_t b, bool b_signed)
    {
        const int32_t ae = a_signed ? static_cast<int32_t>(static_cast<int16_t>(a & 0xFFFFu)) : static_cast<int32_t>(a & 0xFFFFu);
        const int32_t be = b_signed ? static_cast<int32_t>(static_cast<int16_t>(b & 0xFFFFu)) : static_cast<int32_t>(b & 0xFFFFu);
        return static_cast<int64_t>(ae) * be;
    }
    inline uint32_t sext8to16(uint32_t v) { return static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(v & 0xFFu))) & 0xFFFFu; }

    // One lane, prod_comb[l]: four 17 x 17 blocks.
    //   MA_32X32  a[31:0] x b[31:0] signed, 64 bits
    //   MA_32X16  a[31:0] x b[15:0] signed; MA_32X8  a[31:0] x b[7:0] signed -- sign-extended to 64 bits
    //   MA_16X16  {a[31:16] x b[31:16], a[15:0] x b[15:0]}, two signed 32-bit products
    // Latency mula_latency(mode): 4 / 2 / 1 / 2. The unit accepts a beat of another latency only when its pipeline is
    // empty (lat_stall = pipe_busy & (new_lat != cur_lat)); a single mode streams one beat per cycle.
    inline uint64_t vmul32x32_lane(MulaMode in_mode, uint32_t a, uint32_t b)
    {
        uint32_t opa[4] = {0, 0, 0, 0}, opb[4] = {0, 0, 0, 0};
        bool opa_s[4] = {false, false, false, false}, opb_s[4] = {false, false, false, false};
        const uint32_t aL = a & 0xFFFFu, aH = a >> 16, bL = b & 0xFFFFu, bH = b >> 16;
        switch (in_mode)
        {
        case MA_32X32:
            opa[0] = aL; opb[0] = bL;
            opa[1] = aL; opb[1] = bH; opb_s[1] = true;
            opa[2] = aH; opa_s[2] = true; opb[2] = bL;
            opa[3] = aH; opa_s[3] = true; opb[3] = bH; opb_s[3] = true;
            break;
        case MA_32X16:
        case MA_32X8:
        {
            const uint32_t b16 = (in_mode == MA_32X8) ? sext8to16(b) : bL;
            opa[1] = aL; opb[1] = b16; opb_s[1] = true;
            opa[3] = aH; opa_s[3] = true; opb[3] = b16; opb_s[3] = true;
            break;
        }
        default: // MA_16X16
            opa[0] = aL; opa_s[0] = true; opb[0] = bL; opb_s[0] = true;
            opa[3] = aH; opa_s[3] = true; opb[3] = bH; opb_s[3] = true;
            break;
        }
        uint64_t blk[4];
        for (int k = 0; k < 4; k++) blk[k] = static_cast<uint64_t>(mul17(opa[k], opa_s[k], opb[k], opb_s[k])); // 64'(blk[k])
        if (in_mode == MA_32X32) return (blk[3] << 32) + ((blk[2] + blk[1]) << 16) + blk[0];
        if (in_mode == MA_32X16 || in_mode == MA_32X8) return (blk[3] << 16) + blk[1];
        return ((blk[3] & 0xFFFFFFFFull) << 32) | (blk[0] & 0xFFFFFFFFull);
    }

    // ---- FUs/gvu_vmul48x40.sv ------------------------------------------------------------------------------------------
    // One lane, prod_comb[l] (72 bits, returned in the low bits) and aux_comb[l] (32 bits): six 17 x 17 blocks.
    //   MB_40X32       a[39:0] x b[31:0] signed, 72 bits
    //   MB_32X32_16X8  out_prod = a[31:0] x b[31:0] sign-extended to 72 bits; out_aux = a[47:32] x b[39:32] (24-bit
    //                  product sign-extended to 32 bits)
    //   MB_32X8        a[31:0] x b[7:0] sign-extended to 72 bits
    //   MB_3X16X8      {a[47:32] x b[23:16], a[31:16] x b[15:8], a[15:0] x b[7:0]}, three signed 24-bit products
    // Latency mulb_latency(mode): 4 / 4 / 1 / 1. Three valid / ready interfaces: 0 (MB_40X32, MB_32X8), 1 (two bits: the
    // 32x32 product and the 16x8 product of MB_32X32_16X8), 2 (three bits: the three products of MB_3X16X8). The pipeline
    // of an interface advances only while in_mode selects that interface. Only the per-beat values are ported here; the
    // latencies are measured on the RTL simulation (tools/has/gvu_oracle).
    struct MulbOut { u128 out_prod; uint32_t out_aux; };
    inline MulbOut vmul48x40_lane(MulbMode in_mode, uint64_t a, uint64_t b)
    {
        uint32_t opa[6] = {0, 0, 0, 0, 0, 0}, opb[6] = {0, 0, 0, 0, 0, 0};
        bool opa_s[6] = {false, false, false, false, false, false}, opb_s[6] = {false, false, false, false, false, false};
        const uint32_t a0 = static_cast<uint32_t>(a & 0xFFFFu), a1 = static_cast<uint32_t>((a >> 16) & 0xFFFFu), a2 = static_cast<uint32_t>((a >> 32) & 0xFFFFu);
        const uint32_t b0 = static_cast<uint32_t>(b & 0xFFFFu), b1 = static_cast<uint32_t>((b >> 16) & 0xFFFFu);
        switch (in_mode)
        {
        case MB_40X32: // a0, a1 unsigned limbs; a2 = sext8(a[39:32]) and b1 carry the sign
            opa[0] = a0; opb[0] = b0;
            opa[1] = a0; opb[1] = b1; opb_s[1] = true;
            opa[2] = a1; opb[2] = b0;
            opa[3] = a1; opb[3] = b1; opb_s[3] = true;
            opa[4] = sext8to16(a2); opa_s[4] = true; opb[4] = b0;
            opa[5] = sext8to16(a2); opa_s[5] = true; opb[5] = b1; opb_s[5] = true;
            break;
        case MB_32X32_16X8:
            opa[0] = a0; opb[0] = b0;
            opa[1] = a0; opb[1] = b1; opb_s[1] = true;
            opa[2] = a1; opa_s[2] = true; opb[2] = b0;
            opa[3] = a1; opa_s[3] = true; opb[3] = b1; opb_s[3] = true;
            opa[4] = a2; opa_s[4] = true; opb[4] = sext8to16(static_cast<uint32_t>(b >> 32)); opb_s[4] = true;
            break;
        case MB_32X8:
        {
            const uint32_t bs = sext8to16(static_cast<uint32_t>(b));
            opa[1] = a0; opb[1] = bs; opb_s[1] = true;
            opa[3] = a1; opa_s[3] = true; opb[3] = bs; opb_s[3] = true;
            break;
        }
        default: // MB_3X16X8
            opa[0] = a0; opa_s[0] = true; opb[0] = sext8to16(static_cast<uint32_t>(b)); opb_s[0] = true;
            opa[1] = a1; opa_s[1] = true; opb[1] = sext8to16(static_cast<uint32_t>(b >> 8)); opb_s[1] = true;
            opa[2] = a2; opa_s[2] = true; opb[2] = sext8to16(static_cast<uint32_t>(b >> 16)); opb_s[2] = true;
            break;
        }
        i128 blk[6];
        for (int k = 0; k < 6; k++) blk[k] = mul17(opa[k], opa_s[k], opb[k], opb_s[k]);   // OW'(blk[k]), sign-extended
        const u128 M72 = (static_cast<u128>(1) << GVU_MULB_OUT_W) - 1;
        auto shl = [](i128 v, int n) -> u128 { return static_cast<u128>(v) << n; };
        MulbOut o{0, 0};
        switch (in_mode)
        {
        case MB_40X32:
            o.out_prod = (shl(blk[5], 48) + shl(blk[4] + blk[3], 32) + shl(blk[2] + blk[1], 16) + static_cast<u128>(blk[0])) & M72;
            break;
        case MB_32X32_16X8:
            o.out_prod = (shl(blk[3], 32) + shl(blk[2] + blk[1], 16) + static_cast<u128>(blk[0])) & M72;
            o.out_aux = static_cast<uint32_t>(static_cast<int64_t>(blk[4]));              // AW'(blk[4])
            break;
        case MB_32X8:
            o.out_prod = (shl(blk[3], 16) + static_cast<u128>(blk[1])) & M72;
            break;
        default:
            o.out_prod = (static_cast<u128>(static_cast<uint64_t>(blk[2]) & 0xFFFFFFull) << 48) |
                         (static_cast<u128>(static_cast<uint64_t>(blk[1]) & 0xFFFFFFull) << 24) |
                         static_cast<u128>(static_cast<uint64_t>(blk[0]) & 0xFFFFFFull);
            break;
        }
        return o;
    }
} // namespace rtl_ref_gvu

#endif // RTL_REF_GVU_FU_H
