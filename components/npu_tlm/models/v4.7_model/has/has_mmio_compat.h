// has_mmio_compat.h -- host MMIO front end of HasNpuTop that keeps the v4.5 programming protocol
// (docs/SW_INTEGRATION_GUIDE.md).
//
// Software written against the previous hand-over (control/instruction_decoder.h: gather registers 0x40000400..0x468,
// push an opcode to 0x40000310) keeps talking the same way. What it writes is decoded into one LayerInstr per pushed
// instruction (one instruction = one layer; the DFC's tile iterator, has/has_tile_iter.h, walks the tiles).
// Additions, all in address ranges no other code of the model uses:
//   0x40000318 STATUS  (R; any write clears the sticky error)   0x4000031C RETIRED (R)
//   0x4000046C..0x400004E4 extension registers -- ONE-SHOT: cleared after every push, so a later old-style push can
//   never pick up stale conv geometry. The old registers stay sticky exactly as in instruction_decoder.h.
// Nothing is executed here: the DFC pops instructions and calls retire(). Pure C++ (no SystemC) so it can be unit
// tested alone (tools/has/tb_has_mmio_compat.cpp).
#ifndef HAS_MMIO_COMPAT_H
#define HAS_MMIO_COMPAT_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <deque>
#include "has/has_tile_iter.h"

namespace has
{
    namespace mmio
    {
        // --- v4.5 addresses (instruction_decoder.h host_write_process) ---
        constexpr uint32_t LEGACY_LO = 0x40000300, LEGACY_HI = 0x40000304;
        constexpr uint32_t PUSH_A = 0x40000310, PUSH_B = 0x40000314;
        constexpr uint32_t IN_ADDR = 0x40000400, W_ADDR = 0x40000404, OUT_ADDR = 0x40000408, BIAS_ADDR = 0x4000040C;
        constexpr uint32_t M = 0x40000410, K = 0x40000414, N = 0x40000418, KH = 0x4000041C, KW = 0x40000420;
        constexpr uint32_t STRIDE = 0x40000424, PAD = 0x40000428, ACT_TYPE = 0x4000042C;
        constexpr uint32_t HAS_SKIP = 0x40000430, SKIP_ADDR = 0x40000434;
        constexpr uint32_t IN_SCALE = 0x40000438, W_SCALE = 0x4000043C, OUT_SCALE = 0x40000440;
        constexpr uint32_t A_ADDR = 0x40000444, B_ADDR = 0x40000448, V_ADDR = 0x4000044C, LEN = 0x40000450;
        constexpr uint32_t MODE_PACK = 0x40000454, R458 = 0x40000458, R45C = 0x4000045C, SCALE_OUT_E = 0x40000460;
        constexpr uint32_t A_LEN = 0x40000464, B_LEN = 0x40000468;
        // --- additions ---
        constexpr uint32_t STATUS = 0x40000318, RETIRED = 0x4000031C;
        constexpr uint32_t EXT_BASE = 0x4000046C;
        enum Ext : int  // index = (addr - EXT_BASE) / 4
        {
            X_IN_C, X_IN_H, X_IN_W, X_OUT_C, X_OUT_H, X_OUT_W, X_TILE_COUT, X_TILE_H, X_TILE_W,
            X_SCALE_ADDR, X_SHIFT_ADDR, X_LUT_ADDR, X_ZP_OUT, X_FLAGS,
            X_ZP_A, X_ZP_B, X_ZP_O, X_SA, X_SHA, X_SB, X_SHB, X_SO, X_SHO, X_POOL_K, X_POOL_P, X_POOL_MODE,
            X_TILE_CIN,   // 0x400004D4: input channels per core pass; unset / 0 = whole Cin
            X_Y_USED,     // 0x400004D8: output positions per context when it divides the tile width;
                          // unset / 0 = gcd(tile width, 32) as before
            X_ROWS,       // 0x400004DC: FUSED_ATTN query rows NQ (unset = LEN) / LAYERNORM rows
            X_PARAM_ADDR, // 0x400004E0: DRAM address of the instruction's parameter block (has/gvu_rce_params.h)
            X_MASK_ADDR,  // 0x400004E4: FUSED_ATTN int8 mask [NQ][L] (-128 = masked); unset = no mask
            X_COUNT
        };
        constexpr uint32_t EXT_END = EXT_BASE + 4 * X_COUNT;   // exclusive (0x400004E8)
    }

    enum class Err : uint16_t
    {
        NONE = 0, Q_OVF, LANE_B, LEGACY64, UNSUPPORTED_OP, UNSUPPORTED_MODE, BROADCAST, NEED_LUT, BAD_GEOM, SCALE_RANGE, NO_TILING
    };

    struct AddQ   // integer ELEM_WISE ADD parameters, same meaning as has::AddParams (gvu_elemwise.h)
    {
        int zpA{0}, zpB{0}, zpO{0};
        uint32_t SA{1}, SB{1}, SO{1};
        int sA{0}, sB{0}, sO{0};
    };

    struct LayerInstr
    {
        uint8_t opcode{0};             // 0x12 GEMM_FUSED, 0x13 FUSED_ATTN, 0x14 LAYERNORM, 0x15 ELEM_WISE
        uint32_t host_seq{0};          // number of the host push this came from
        bool last_of_host{true};       // false for the GEMM half of a split has_skip instruction
        // GEMM_FUSED
        uint32_t in_addr{0}, w_addr{0}, out_addr{0}, bias_addr{0};
        int in_c{0}, in_h{0}, in_w{0};
        int cin_t{0};                  // input channels per core pass (S1: Cin split, psum kept in SRAM-C); 0 = in_c
        LayerGeom geom;                // output shape, kernel/stride/pad, tile size
        int act{0};                    // 0 none, 1 ReLU (built-in table), 2 SiLU, 3 GELU (need lut_addr)
        uint32_t lut_addr{0};
        bool per_channel{false};       // true: scale/shift arrays in DRAM; false: one broadcast (S, s)
        uint32_t scale_addr{0}, shift_addr{0};
        uint32_t bcast_S{1};
        int bcast_s{0};
        int zp_out{0};
        uint32_t flags{0};
        bool from_float_scale{false};  // approximate path, not bit-exact with the old emulator
        bool default_tiling{false};    // tiling chosen by default_tiling(), not by the offline planner
        // ELEM_WISE
        int mode{0};                   // 0 ADD, 1 MAX_POOL, 5 AVG_POOL
        uint32_t a_addr{0}, b_addr{0};
        uint32_t n{0};
        int c{0}, h{0}, w{0}, pool_k{0}, pool_s{0}, pool_p{0}, pool_mode{0};
        uint32_t avg_scale{0};         // AVG_POOL Avg_Scale (raw, SCALE_FMT) and Avg_Shift
        int avg_shift{0};
        AddQ add;
        // FUSED_ATTN (Q = a_addr, K = b_addr, V = v_addr, O = out_addr) / LAYERNORM (X = in_addr, Y = out_addr)
        uint32_t v_addr{0}, param_addr{0}, mask_addr{0};
        int rows{0}, seq_len{0}, head_dim{0};
    };

    // HAS SRAM capacities (bytes / PSUM elements) used for the fallback tiling of old-style m/k/n instructions.
    struct SramCaps { int a_bytes{80896}, b_bytes{82944}, c_elems{24576}; };

    // float scale -> (S, s) with S in [2^30, 2^31) and value = S * 2^-s. Returns false when s would leave [0, 63].
    inline bool float_to_fixed(double v, uint32_t &S, int &s)
    {
        if (!(v > 0.0) || !std::isfinite(v)) return false;
        int e = static_cast<int>(std::floor(std::log2(v)));
        double m = std::ldexp(v, 30 - e);                    // in [2^30, 2^31)
        uint64_t Si = static_cast<uint64_t>(std::llround(m));
        if (Si >= (1ull << 31)) { Si >>= 1; e++; }
        s = 30 - e;
        if (s < 0 || s > 63) return false;
        S = static_cast<uint32_t>(Si);
        return true;
    }

    // Fallback tiling for a 1x1 layer (Linear/MatMul given as m, k, n): largest tile that fits the HAS SRAMs.
    inline bool default_tiling(int K, int N, int Mpos, const SramCaps &cap, int &cout_t, int &w_t)
    {
        cout_t = std::min({32, N, cap.b_bytes / std::max(K, 1)});
        if (cout_t <= 0) return false;                       // K too deep for one weight buffer (no K split)
        w_t = std::min({Mpos, cap.a_bytes / std::max(K, 1), cap.c_elems / cout_t});
        if (w_t >= 32) w_t -= w_t % 32;
        return w_t > 0;
    }

    class MmioCompat
    {
    public:
        explicit MmioCompat(size_t q_depth = 16, SramCaps caps = SramCaps()) : q_depth_(q_depth), caps_(caps) {}

        // Host write. Returns true if the address belongs to this block.
        bool write(uint32_t addr, uint32_t data)
        {
            using namespace mmio;
            float f;
            std::memcpy(&f, &data, 4);
            if (addr >= EXT_BASE && addr < EXT_END && (addr - EXT_BASE) % 4 == 0)
            {
                const int i = int((addr - EXT_BASE) / 4);
                ext_[i] = data;
                ext_set_[i] = true;
                return true;
            }
            switch (addr)
            {
            case IN_ADDR: r_in_ = data; return true;
            case W_ADDR: r_w_ = data; return true;
            case OUT_ADDR: r_out_ = data; return true;
            case BIAS_ADDR: r_bias_ = data; return true;
            case M: r_m_ = data; return true;
            case K: r_k_ = data; return true;
            case N: r_n_ = data; return true;
            case KH: r_kh_ = data; return true;
            case KW: r_kw_ = data; return true;
            case STRIDE: r_stride_ = data; return true;
            case PAD: r_pad_ = data; return true;
            case ACT_TYPE: r_act_ = data; return true;
            case HAS_SKIP: r_has_skip_ = data; return true;
            case SKIP_ADDR: r_skip_ = data; return true;
            case IN_SCALE: r_in_scale_ = f; return true;
            case W_SCALE: r_w_scale_ = f; return true;
            case OUT_SCALE: r_out_scale_ = f; return true;
            case A_ADDR: r_a_ = data; return true;
            case B_ADDR: r_b_ = data; return true;
            case V_ADDR: r_v_ = data; return true;                       // FUSED_ATTN V
            case LEN: r_len_ = data; return true;
            case MODE_PACK:                                              // same packing rule as instruction_decoder.h
                r_mode_ = (data & 0xFFFF0000u) ? (data >> 24) & 0xFF : data;
                return true;
            case R458: r_scale_a_ = f; r_458_ = data; return true;       // head_dim / eps_shift / scale_a
            case R45C: r_scale_b_ = f; return true;                      // attn_scale / scale_b
            case SCALE_OUT_E: r_scale_o_ = f; return true;
            case A_LEN: r_a_len_ = data; return true;
            case B_LEN: r_b_len_ = data; return true;
            case PUSH_A: push(data & 0xFF); return true;
            case PUSH_B: host_seq_++; error(Err::LANE_B); return true;
            case LEGACY_LO: return true;
            case LEGACY_HI: host_seq_++; error(Err::LEGACY64); return true;
            case STATUS: sticky_err_ = false; last_err_ = Err::NONE; return true;
            case RETIRED: return true;
            default: return false;
            }
        }

        bool owns(uint32_t addr) const
        {
            using namespace mmio;
            return (addr >= LEGACY_LO && addr <= RETIRED) || (addr >= IN_ADDR && addr < EXT_END);
        }

        uint32_t read(uint32_t addr) const
        {
            if (addr == mmio::STATUS) return status();
            if (addr == mmio::RETIRED) return retired_;
            return 0;
        }

        // bit0 busy, bit1 queue full, bit2 sticky error, [15:8] queued instructions, [31:16] last error code.
        uint32_t status() const
        {
            const uint32_t q = uint32_t(std::min<size_t>(q_.size(), 255));
            return uint32_t(busy() ? 1 : 0) | uint32_t(q_.size() >= q_depth_ ? 2 : 0) | uint32_t(sticky_err_ ? 4 : 0) |
                   (q << 8) | (uint32_t(last_err_) << 16);
        }
        bool busy() const { return !q_.empty() || executing_; }
        bool executing() const { return executing_; }   // an instruction has been popped and is not retired yet
        bool irq() const { return !busy() && retired_ > 0; }

        // --- DFC side ---
        bool pop(LayerInstr &li)
        {
            if (q_.empty()) return false;
            li = q_.front();
            q_.pop_front();
            executing_ = true;
            return true;
        }
        void retire(const LayerInstr &li)
        {
            executing_ = false;
            if (li.last_of_host) retired_++;
        }
        void soft_reset()
        {
            q_.clear();
            retired_ = 0;
            executing_ = false;
            sticky_err_ = false;
            last_err_ = Err::NONE;
            clear_ext();
        }

        // counters (evidence for the hand-over report)
        uint64_t n_nsplit_ignored{0}, n_float_scale{0}, n_default_tiling{0}, n_skip_split{0}, n_errors{0};
        Err last_error() const { return last_err_; }
        size_t queued() const { return q_.size(); }
        // Testbench windows (--first): an instruction that is replayed but not pushed must still drop its one-shot extension
        // registers, as its push would (otherwise e.g. TILE_CIN of a skipped layer leaks into the first pushed one).
        void discard_ext() { clear_ext(); }

    private:
        void error(Err e)
        {
            sticky_err_ = true;
            last_err_ = e;
            n_errors++;
        }
        void clear_ext()
        {
            for (int i = 0; i < mmio::X_COUNT; i++) { ext_[i] = 0; ext_set_[i] = false; }
        }
        bool enqueue(const LayerInstr &li)
        {
            if (q_.size() >= q_depth_) { error(Err::Q_OVF); return false; }
            q_.push_back(li);
            return true;
        }
        int32_t ext_i(int i) const { return static_cast<int32_t>(ext_[i]); }

        void push(uint32_t opcode)
        {
            host_seq_++;
            if (opcode == 0x05) n_nsplit_ignored++;                      // SET_NSPLIT: no dual lane in the HAS
            else if (opcode == 0x12) push_gemm();
            else if (opcode == 0x15) push_elem();
            else if (opcode == 0x13 || opcode == 0x14) push_rce(uint8_t(opcode));
            else error(Err::UNSUPPORTED_OP);
            clear_ext();
        }

        void push_gemm()
        {
            using namespace mmio;
            LayerInstr li;
            li.opcode = 0x12;
            li.host_seq = host_seq_;
            li.in_addr = r_in_; li.w_addr = r_w_; li.out_addr = r_out_; li.bias_addr = r_bias_;
            LayerGeom &g = li.geom;
            if (ext_set_[X_IN_C])
            {
                li.in_c = ext_i(X_IN_C); li.in_h = ext_i(X_IN_H); li.in_w = ext_i(X_IN_W);
                g.cout = ext_i(X_OUT_C); g.oh = ext_i(X_OUT_H); g.ow = ext_i(X_OUT_W);
                g.kh = int(r_kh_); g.kw = int(r_kw_); g.sy = g.sx = int(r_stride_); g.pad_t = g.pad_l = int(r_pad_);
                g.cout_t = ext_i(X_TILE_COUT); g.h_t = ext_i(X_TILE_H); g.w_t = ext_i(X_TILE_W);
                if (g.cout_t <= 0 || g.h_t <= 0 || g.w_t <= 0) { error(Err::NO_TILING); return; }
            }
            else
            {
                // Old-style Linear/MatMul: M positions x K inputs -> N outputs = 1x1 conv on a 1 x M image.
                li.in_c = int(r_k_); li.in_h = 1; li.in_w = int(r_m_);
                g.cout = int(r_n_); g.oh = 1; g.ow = int(r_m_);
                g.kh = g.kw = g.sy = g.sx = 1; g.pad_t = g.pad_l = 0;
                g.h_t = 1;
                if (!default_tiling(li.in_c, g.cout, g.ow, caps_, g.cout_t, g.w_t)) { error(Err::NO_TILING); return; }
                li.default_tiling = true;
                n_default_tiling++;
            }
            if (li.in_c <= 0 || li.in_h <= 0 || li.in_w <= 0 || g.cout <= 0 || g.oh <= 0 || g.ow <= 0 || g.kh <= 0 ||
                g.kw <= 0 || g.sy <= 0 || g.pad_t < 0 ||
                (g.oh - 1) * g.sy - g.pad_t + g.kh > li.in_h + g.pad_t || (g.ow - 1) * g.sx - g.pad_l + g.kw > li.in_w + g.pad_l)
            { error(Err::BAD_GEOM); return; }

            li.cin_t = (ext_set_[X_TILE_CIN] && ext_i(X_TILE_CIN) > 0) ? ext_i(X_TILE_CIN) : li.in_c;
            g.y_used = ext_set_[X_Y_USED] ? ext_i(X_Y_USED) : 0;
            if (g.y_used < 0 || g.y_used > g.sa_y) { error(Err::BAD_GEOM); return; }
            if (li.cin_t > li.in_c || li.in_c % li.cin_t != 0) { error(Err::BAD_GEOM); return; }
            li.act = int(r_act_);
            li.lut_addr = ext_[X_LUT_ADDR];
            if (li.act > 3) { error(Err::UNSUPPORTED_MODE); return; }
            if ((li.act == 2 || li.act == 3) && !ext_set_[X_LUT_ADDR]) { error(Err::NEED_LUT); return; }
            li.zp_out = ext_i(X_ZP_OUT);
            li.flags = ext_[X_FLAGS];
            g.pad_tail = (li.flags & 1u) != 0;   // FLAGS bit 0 = PAD_TAIL, bit 1 = CHANNEL_MAJOR (RCE only)
            if (ext_set_[X_SCALE_ADDR])
            {
                li.per_channel = true;
                li.scale_addr = ext_[X_SCALE_ADDR];
                li.shift_addr = ext_[X_SHIFT_ADDR];
            }
            else
            {
                const double m = double(r_in_scale_) * double(r_w_scale_) / double(r_out_scale_);
                if (!float_to_fixed(m, li.bcast_S, li.bcast_s)) { error(Err::SCALE_RANGE); return; }
                li.from_float_scale = true;
                n_float_scale++;
            }
            if (r_has_skip_)
            {
                // HAS: no residual in GEMM_FUSED -> GEMM then an in-place unit-scale ELEM_WISE ADD (the compat path
                // verified byte-identical to the testbench-driven network run).
                LayerInstr add;
                add.opcode = 0x15;
                add.host_seq = host_seq_;
                add.mode = 0;
                add.a_addr = li.out_addr; add.b_addr = r_skip_; add.out_addr = li.out_addr;
                add.n = uint32_t(g.cout) * uint32_t(g.oh) * uint32_t(g.ow);
                add.add = unit_add();
                li.last_of_host = false;
                if (q_.size() + 2 > q_depth_) { error(Err::Q_OVF); return; }
                enqueue(li);
                enqueue(add);
                n_skip_split++;
                return;
            }
            enqueue(li);
        }

        static AddQ unit_add()
        {
            // a' = A * 2^16, b' = B * 2^16, out = round(Sum * 2^-16): exact sat8(A + B) for int8 A, B.
            AddQ q;
            q.SA = q.SB = 1u << 30; q.sA = q.sB = 14;
            q.SO = 1u << 30; q.sO = 46;
            return q;
        }

        void push_elem()
        {
            using namespace mmio;
            LayerInstr li;
            li.opcode = 0x15;
            li.host_seq = host_seq_;
            li.mode = int(r_mode_);
            li.a_addr = r_a_; li.b_addr = r_b_; li.out_addr = r_out_;
            if (li.mode == 0)
            {
                li.n = r_len_;
                const uint32_t al = r_a_len_ ? r_a_len_ : r_len_, bl = r_b_len_ ? r_b_len_ : r_len_;
                if (al != r_len_ || bl != r_len_) { error(Err::BROADCAST); return; }   // HAS ADD has no broadcast
                if (li.n == 0) { error(Err::BAD_GEOM); return; }
                if (ext_set_[X_SA])
                {
                    li.add.zpA = ext_i(X_ZP_A); li.add.zpB = ext_i(X_ZP_B); li.add.zpO = ext_i(X_ZP_O);
                    li.add.SA = ext_[X_SA]; li.add.sA = ext_i(X_SHA);
                    li.add.SB = ext_[X_SB]; li.add.sB = ext_i(X_SHB);
                    li.add.SO = ext_[X_SO]; li.add.sO = ext_i(X_SHO);
                }
                else
                {
                    // float path: a' = A*(sa/so)*2^16, b' = B*(sb/so)*2^16, out = Sum*2^-16
                    const double sa = valid(r_scale_a_), sb = valid(r_scale_b_), so = valid(r_scale_o_);
                    AddQ q = unit_add();
                    if (!float_to_fixed(sa / so * 65536.0, q.SA, q.sA) || !float_to_fixed(sb / so * 65536.0, q.SB, q.sB))
                    { error(Err::SCALE_RANGE); return; }
                    li.add = q;
                    li.from_float_scale = true;
                    n_float_scale++;
                }
            }
            else if (li.mode == 1)
            {
                if (!ext_set_[X_IN_C] || !ext_set_[X_POOL_K]) { error(Err::BAD_GEOM); return; }   // old 1-D pooling is not a HAS mode
                li.c = ext_i(X_IN_C); li.h = ext_i(X_IN_H); li.w = ext_i(X_IN_W);
                li.pool_k = ext_i(X_POOL_K); li.pool_p = ext_i(X_POOL_P); li.pool_s = int(r_stride_);
                li.pool_mode = ext_i(X_POOL_MODE);
                if (li.c <= 0 || li.h <= 0 || li.w <= 0 || li.pool_k <= 0 || li.pool_s <= 0 || li.pool_p < 0 ||
                    li.h + 2 * li.pool_p < li.pool_k || li.w + 2 * li.pool_p < li.pool_k)
                { error(Err::BAD_GEOM); return; }
            }
            else if (li.mode == 5)
            {
                // AVG_POOL (vector-unit drawing "Avg_Pool"): int16 window sum, x Avg_Scale, round-shift by Avg_Shift.
                // No padding in the drawing; Avg_Scale / Avg_Shift come from the extension registers SO / SHO.
                if (!ext_set_[X_IN_C] || !ext_set_[X_POOL_K] || !ext_set_[X_SO]) { error(Err::BAD_GEOM); return; }
                li.c = ext_i(X_IN_C); li.h = ext_i(X_IN_H); li.w = ext_i(X_IN_W);
                li.pool_k = ext_i(X_POOL_K); li.pool_p = ext_i(X_POOL_P); li.pool_s = int(r_stride_);
                li.avg_scale = ext_[X_SO]; li.avg_shift = ext_i(X_SHO);
                if (li.c <= 0 || li.h <= 0 || li.w <= 0 || li.pool_k <= 0 || li.pool_s <= 0 || li.pool_p != 0 ||
                    li.h < li.pool_k || li.w < li.pool_k || li.avg_shift < 0 || li.avg_shift > 63)
                { error(Err::BAD_GEOM); return; }
            }
            else { error(Err::UNSUPPORTED_MODE); return; }                   // MUL / SUB / DIV: not in the HAS ISA
            enqueue(li);
        }

        // FUSED_ATTN = one head, LAYERNORM = X_ROWS rows; the numbers live in the PARAM_ADDR block.
        void push_rce(uint8_t op)
        {
            using namespace mmio;
            LayerInstr li;
            li.opcode = op;
            li.host_seq = host_seq_;
            li.out_addr = r_out_;
            if (!ext_set_[X_PARAM_ADDR]) { error(Err::BAD_GEOM); return; }
            li.param_addr = ext_[X_PARAM_ADDR];
            if (op == 0x13)
            {
                li.a_addr = r_a_; li.b_addr = r_b_; li.v_addr = r_v_;
                li.seq_len = int(r_len_);
                li.head_dim = int(r_458_);
                li.rows = ext_set_[X_ROWS] ? ext_i(X_ROWS) : li.seq_len;
                li.mask_addr = ext_set_[X_MASK_ADDR] ? ext_[X_MASK_ADDR] : 0;
                if (li.seq_len <= 0 || li.head_dim <= 0 || li.rows <= 0) { error(Err::BAD_GEOM); return; }
            }
            else
            {
                li.in_addr = r_in_;
                li.seq_len = int(r_len_);                                // H (row length); must match the block's H
                li.rows = ext_set_[X_ROWS] ? ext_i(X_ROWS) : 0;
                if (li.rows <= 0 || li.seq_len <= 0) { error(Err::BAD_GEOM); return; }
            }
            li.flags = ext_[X_FLAGS];   // bit 1 = CHANNEL_MAJOR operands; other bits reserved
            enqueue(li);
        }

        // same validity rule as emulate_elem_wise()
        static double valid(float v) { return (v > 0.00001f && v < 10000.0f) ? double(v) : 1.0; }

        size_t q_depth_;
        SramCaps caps_;
        std::deque<LayerInstr> q_;
        uint32_t retired_{0}, host_seq_{0};
        bool executing_{false}, sticky_err_{false};
        Err last_err_{Err::NONE};
        // v4.5 sticky registers (defaults as instruction_decoder.h)
        uint32_t r_in_{0}, r_w_{0}, r_out_{0}, r_bias_{0}, r_m_{32}, r_k_{32}, r_n_{32}, r_kh_{1}, r_kw_{1}, r_stride_{1},
            r_pad_{0}, r_act_{0}, r_has_skip_{0}, r_skip_{0}, r_a_{0}, r_b_{0}, r_len_{0}, r_mode_{0}, r_a_len_{0}, r_b_len_{0},
            r_v_{0}, r_458_{0};
        float r_in_scale_{1.0f}, r_w_scale_{1.0f}, r_out_scale_{1.0f}, r_scale_a_{1.0f}, r_scale_b_{1.0f}, r_scale_o_{1.0f};
        uint32_t ext_[mmio::X_COUNT]{};
        bool ext_set_[mmio::X_COUNT]{};
    };
} // namespace has

#endif
