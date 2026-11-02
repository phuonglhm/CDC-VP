// rtl_ref_gvu_ln_lanes.h -- LAYERNORM of the vector unit with the statistics per lane: the data path the model proposes for the RTL
// (MAS figure 8, plan decisions Q13 / Q14). Not a port: the delivered RTL keeps S, V and w as one value for all 32 lanes and writes
// the result to vbank1 (128 int8 rows), so it neither normalises 32 tokens at once nor holds H = 768 (P0.3 of the plan).
//
// Layout (one group = up to 32 tokens, one token per lane; beat k = channel k, k = 0 .. H-1):
//   vbank0 buffer 0, word k     x_k of every lane (int8); stage 4 writes n_k in its place, stage 5 writes y_k in its place
//   sbank  buffer 0, word k     beta_k (int32)
//   sbank  buffer 1, word k     gamma_k (int8, sign-extended in the 32-bit word)
//   per lane registers          S (int32 sum), V (int64 sum of squares), v, w, r, E (stage 3 / 4 operands)
// Stage 4 computes D_k = H*x_k - S again from x_k (Q14 (a)) instead of keeping D_k in vbank2 (256 words, H <= 256): the price is one
// multiply-add per beat, the gain is that H = 768 fits (vbank0 800 words, sbank 768 words). M0_mul / TS_mul are one value for the row
// (the MAS form; ViT-B has one value for its 768 channels, P0.4); per-channel values are not expressible here.
//
// GvuLnLanes runs a group clock by clock: tick() is one edge. Stage s issues one bank read per edge (beat k at edge k); the word of
// beat k is processed at edge k + read (the read latency of the bank) and, in stages 4 and 5, written back in place in that edge; the
// tail of a stage (the requant of v after the last beat, stage 3) takes its depth in edges after the last beat. Values: the integer
// equations of has::layernorm_row, lane by lane, operands read from the banks. The depths are an ESTIMATE built from the latencies of
// the ported units (bank read 2, Round-Shift 1, LUT PWL lat_lut, the adders and clamps combinational) and one register per multiplier;
// no RTL exists to measure. ln_lanes_run() runs a whole group in one call (same values and cycles).
#ifndef RTL_REF_GVU_LN_LANES_H
#define RTL_REF_GVU_LN_LANES_H

#include <array>
#include <cstdint>
#include <vector>
#include "has/gvu_layernorm.h"
#include "has/rtl_ref_gvu_lut.h"
#include "has/rtl_ref_gvu_scratchpad.h"

namespace rtl_ref_gvu
{
    struct LnLaneDepths
    {
        int read{2};                       // bank read + read register (vbank0_rd_delay_)
        int st1{1};                        // Adder#1: S += x (register)
        int st2{1 + 1 + 1 + 1 + 1};        // Mul#0 H*x, Adder#2 -S (comb.), Mul D*D, Adder 64 V += (register), Round-Shift of v after the last beat
        int st3{1 + 1 + 1};                // (v - Z_var)*M0_7 (Mul), + E_bias, Round-Shift; clamp and w combinational
        int st4{1 + 1 + 1 + 1};            // D again (Mul#0), r*D (Mul), *M0_div (Mul), Round-Shift; + LUT PWL below
        int st5{1 + 1 + 1};                // (n - Z_div)*gamma (Mul), *M0_mul (Mul), Round-Shift; + beta, clamp combinational
        static LnLaneDepths with_lut()
        {
            LnLaneDepths d;
            d.st4 += lat_lut(LUT_RSQRT, LUT_PWL, LutParams{false, true, true, 32 * 32});
            return d;
        }
    };

    struct LnLaneRun
    {
        bool ok{false};
        uint64_t st[5]{0, 0, 0, 0, 0};
        uint64_t cycles() const { return st[0] + st[1] + st[2] + st[3] + st[4]; }
    };

    // Can the group run here? H fits vbank0 / sbank, gamma int8, beta int32, one M0_mul / TS_mul, int8 output.
    inline bool ln_lanes_fit(const has::LnParams &p, const ScratchpadParams &sp = ScratchpadParams())
    {
        if (p.H < 1 || p.H > sp.VBANK0_DEPTH || p.H > sp.SBANK_DEPTH || p.out_int16) return false;
        for (int j = 0; j < p.H; j++)
        {
            if (p.gamma[j] < -128 || p.gamma[j] > 127) return false;
            if (p.beta[j] < INT32_MIN || p.beta[j] > INT32_MAX) return false;
            if (p.M0_mul[j] != p.M0_mul[0] || p.TS_mul[j] != p.TS_mul[0]) return false;
        }
        return true;
    }

    // Writes the parameters of a row into the sbank (beta, gamma per beat).
    inline void ln_lanes_load_params(GvuScratchpad &sp, const has::LnParams &p)
    {
        for (int k = 0; k < p.H; k++)
        {
            const uint32_t b = static_cast<uint32_t>(static_cast<int32_t>(p.beta[static_cast<size_t>(k)]));
            const uint32_t g = static_cast<uint32_t>(static_cast<int32_t>(p.gamma[static_cast<size_t>(k)]));
            sp.sram(3, 0).poke(static_cast<unsigned>(k), SpBytes{uint8_t(b), uint8_t(b >> 8), uint8_t(b >> 16), uint8_t(b >> 24)});
            sp.sram(3, 1).poke(static_cast<unsigned>(k), SpBytes{uint8_t(g), uint8_t(g >> 8), uint8_t(g >> 16), uint8_t(g >> 24)});
        }
    }

    // One group of `rows` tokens already in vbank0 (and the parameters in the sbank), clock by clock.
    class GvuLnLanes
    {
    public:
        void begin(GvuScratchpad &sp, int rows, const has::LnParams &p, const has::LutIndirect &lut, int round_mode,
                   const LnLaneDepths &d = LnLaneDepths::with_lut())
        {
            sp_ = &sp; p_ = &p; lut_ = &lut; rm_ = round_mode; d_ = d; rows_ = rows;
            S_.fill(0); V_.fill(0); v_.fill(0); w_.fill(0); r_.fill(0); E_.fill(0);
            run_ = LnLaneRun();
            const uint64_t H = static_cast<uint64_t>(p.H);
            len_[0] = H + d.read + d.st1; len_[1] = H + d.read + d.st2; len_[2] = static_cast<uint64_t>(d.st3);
            len_[3] = H + d.read + d.st4; len_[4] = H + d.read + d.st5;
            stage_ = 0; t_ = 0;
            active_ = rows >= 1 && rows <= 32;
        }
        bool active() const { return active_; }
        // One edge; true when the group is done (result in vbank0).
        bool tick()
        {
            if (!active_) return true;
            const int H = p_->H;
            const int64_t k = static_cast<int64_t>(t_) - d_.read;        // beat whose word arrives in this edge (stages 1, 2, 4, 5)
            const bool beat = stage_ != 2 && k >= 0 && k < H;
            switch (stage_)
            {
            case 0: if (beat) beat_sum(static_cast<int>(k)); break;
            case 1:
                if (beat) beat_var(static_cast<int>(k));
                if (t_ + 1 == len_[1]) stage2_tail();
                break;
            case 2: if (t_ + 1 == len_[2]) stage3(); break;
            case 3: if (beat) beat_norm(static_cast<int>(k)); break;
            case 4: if (beat) beat_out(static_cast<int>(k)); break;
            }
            if (++t_ < len_[stage_]) return false;
            run_.st[stage_] = len_[stage_];
            t_ = 0;
            if (++stage_ < 5) return false;
            run_.ok = true;
            active_ = false;
            return true;
        }
        const LnLaneRun &result() const { return run_; }
        // Read-only view of the lane statistics for the stage checks of tools/has/tb_gvu_ln_lanes.cpp: the stage being run (0..4,
        // 5 when done) and, per lane, S and V (stages 1-2), v (end of stage 2), w, r, E (stage 3).
        int stage() const { return active_ ? stage_ : 5; }
        int64_t lane_S(int l) const { return S_[static_cast<size_t>(l)]; }
        int64_t lane_V(int l) const { return V_[static_cast<size_t>(l)]; }
        int lane_v(int l) const { return v_[static_cast<size_t>(l)]; }
        int lane_w(int l) const { return w_[static_cast<size_t>(l)]; }
        int lane_r(int l) const { return r_[static_cast<size_t>(l)]; }
        int lane_E(int l) const { return E_[static_cast<size_t>(l)]; }

    private:
        GvuScratchpad *sp_{nullptr};
        const has::LnParams *p_{nullptr};
        const has::LutIndirect *lut_{nullptr};
        int rm_{0}, rows_{0};
        LnLaneDepths d_;
        std::array<int64_t, 32> S_{}, V_{};
        std::array<int, 32> v_{}, w_{}, r_{}, E_{};
        uint64_t len_[5]{0, 0, 0, 0, 0}, t_{0};
        int stage_{0};
        bool active_{false};
        LnLaneRun run_;

        int x_of(const SpBytes &w, int l) const { return static_cast<int>(static_cast<int8_t>(w[static_cast<size_t>(l)])); }
        void beat_sum(int k)
        {
            const SpBytes w = sp_->sram(0, 0).word(static_cast<unsigned>(k));
            for (int l = 0; l < rows_; l++) S_[static_cast<size_t>(l)] += x_of(w, l);
        }
        void beat_var(int k)
        {
            const SpBytes w = sp_->sram(0, 0).word(static_cast<unsigned>(k));
            for (int l = 0; l < rows_; l++)
            {
                const int64_t D = int64_t(p_->H) * x_of(w, l) - S_[static_cast<size_t>(l)];
                V_[static_cast<size_t>(l)] += D * D;
            }
        }
        void stage2_tail()
        {
            for (int l = 0; l < rows_; l++)
                v_[static_cast<size_t>(l)] = has::requant_clamp(static_cast<has::i128>(V_[static_cast<size_t>(l)] >> p_->Pre_Shift), p_->M0_var, p_->TS_var,
                                                                 p_->Z_var, -128, 127, rm_);
        }
        void stage3()
        {
            for (int l = 0; l < rows_; l++)
            {
                const size_t i = static_cast<size_t>(l);
                const has::i128 u = has::rshift_wide(static_cast<has::i128>(v_[i] - p_->Z_var) * p_->M0_7 + p_->E_bias, p_->TS_7, rm_);
                const int f = static_cast<int>(has::clamp128(u + p_->Z_7, -128, 127));
                w_[i] = f > p_->Z_7 ? f - p_->Z_7 : (p_->r8_floor_z_out ? p_->r8_z_out : 1);
                const has::LutResult lr = lut_->rsqrt(static_cast<uint32_t>(w_[i]));
                r_[i] = lr.r;
                E_[i] = lr.e_shift;
            }
        }
        // stage 4: n_k = clamp8(RoundShift(r*D_k*M0_div, TS_div + E) + Z_div), D_k computed again from x_k; n_k replaces x_k
        void beat_norm(int k)
        {
            SpBytes word = sp_->sram(0, 0).word(static_cast<unsigned>(k));
            for (int l = 0; l < rows_; l++)
            {
                const size_t i = static_cast<size_t>(l);
                const int64_t D = int64_t(p_->H) * x_of(word, l) - S_[i];
                const has::i128 prod = static_cast<has::i128>(r_[i]) * D;
                const has::i128 acc = p_->r9_exact ? prod * p_->M0_div : ((prod >> p_->r9_lsb_bits) * p_->M0_div) << p_->r9_lsb_bits;
                const int n = static_cast<int>(has::clamp128(has::rshift_wide(acc, p_->TS_div + E_[i], rm_) + p_->Z_div, -128, 127));
                word[i] = static_cast<uint8_t>(static_cast<int8_t>(n));
            }
            sp_->sram(0, 0).poke(static_cast<unsigned>(k), word);
        }
        // stage 5: y_k = clamp8(RoundShift((n_k - Z_div)*gamma_k*M0_mul, TS_mul) + Z_mul - Z_mul + beta_k + Z_out), gamma / beta from the sbank
        void beat_out(int k)
        {
            SpBytes word = sp_->sram(0, 0).word(static_cast<unsigned>(k));
            const SpBytes bw = sp_->sram(3, 0).word(static_cast<unsigned>(k)), gw = sp_->sram(3, 1).word(static_cast<unsigned>(k));
            const int32_t beta = static_cast<int32_t>(uint32_t(bw[0]) | uint32_t(bw[1]) << 8 | uint32_t(bw[2]) << 16 | uint32_t(bw[3]) << 24);
            const int32_t gamma = static_cast<int32_t>(uint32_t(gw[0]) | uint32_t(gw[1]) << 8 | uint32_t(gw[2]) << 16 | uint32_t(gw[3]) << 24);
            for (int l = 0; l < rows_; l++)
            {
                const int n = static_cast<int8_t>(word[static_cast<size_t>(l)]);
                const int t = has::requant_clamp(static_cast<has::i128>(n - p_->Z_div) * gamma, p_->M0_mul[0], p_->TS_mul[0], p_->Z_mul, -128, 127, rm_);
                const int y = static_cast<int>(has::clamp128(static_cast<has::i128>(t) - p_->Z_mul + beta + p_->Z_out, -128, 127));
                word[static_cast<size_t>(l)] = static_cast<uint8_t>(static_cast<int8_t>(y));
            }
            sp_->sram(0, 0).poke(static_cast<unsigned>(k), word);
        }
    };

    // A whole group in one call (the clocked unit run to the end): same values and cycles as GvuLnLanes edge by edge.
    inline LnLaneRun ln_lanes_run(GvuScratchpad &sp, int rows, const has::LnParams &p, const has::LutIndirect &lut, int round_mode,
                                  const LnLaneDepths &d = LnLaneDepths::with_lut())
    {
        GvuLnLanes u;
        u.begin(sp, rows, p, lut, round_mode, d);
        if (!u.active()) return LnLaneRun();
        while (!u.tick()) {}
        return u.result();
    }
} // namespace rtl_ref_gvu

#endif // RTL_REF_GVU_LN_LANES_H
