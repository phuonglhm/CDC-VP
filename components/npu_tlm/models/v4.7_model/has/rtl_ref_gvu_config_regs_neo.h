// rtl_ref_gvu_config_regs_neo.h -- configuration register file of the vector unit as built in neo_lite (gvu_config_regs.sv, gvu_pkg.sv gvu_cfg_reg_t).
// Pure C++17, no SystemC. One tick() = one rising clock edge. Byte addresses, 4 bytes per register: waddr = byte_addr[5:2].
//   0 CONTROL [0] enable [1] done flag [2] clear done (write 1)          1 OPCODE [3:0]            2 LUT reset [0..3] act / exp / recip / rsqrt (one-edge pulse)
//   3 GEMM_FUSED [0] skip act [15:8] zp   4 SOFTMAX_0 [15:0] token len [31:16] tiles   5 SOFTMAX_1 scale   6 SOFTMAX_2 [7:0] shift [15:8] zp
//   7 RESIDUAL_0 [15:0] len [23:16] out zp [29:24] out shift [30] sub   8 / 9 scale A / B   10 / 11 bias low / high
//  12 POOLING_0 [1:0] mode [4:2] stride [15:8] W tiles, then rev 0 (revision 0) [23:16] C [31:24] OH, rev 1 (revision 1) [23:16] OH [31:24] C   13 POOLING_1 [9:0] H move [25:16] C move
// rev (constructor; has::neo_rev() in has/neo_rev.h): 0 = neo_lite RTL revision 0, 1 = neo_lite RTL revision 1, which also swaps the four buffer-select flops on a write of
// CONTROL bit 3 with mask bit 3 ("force buffer swap", sent by the DFC with its last CONTROL write of a command), unless the enable rises in the same edge.
// RTL has GVU_NUM_REGS = 14 (indices 14, 15 do not exist: a write there is dropped, a read gives 0 here, x in a 4-state simulator).
// num_regs = 32 (HAS_GVU_REGS_NEO, proposal for the hardware: GVU_NUM_REGS 32, IF_AW 7): indices 14..31 are plain storage that the model decodes
// (14 / 15 average pool, 16..24 AGU_ND, 25..29 the parts of the residual add that RESIDUAL_0..4 do not hold); the default 14 is the RTL verbatim.
// Behaviour kept edge by edge: pipe_done clears the enable and sets the done flag, then a host write of CONTROL overrides (bit 0 written ==> enable follows the
// host even in the edge of pipe_done; bit 2 written ==> done = done_q & ~wdata[2], the set of pipe_done in the same edge is lost); the LUT reset flops clear
// themselves every edge unless written; the four buffer-select flops toggle in the edge in which the enable rises (pipe_en_d & ~pipe_en_q);
// write mask is one bit per bit (all four bytes writable); cfg_reg is NOT reset in the RTL (the port starts at 0; a 4-state simulator starts at x).
#ifndef RTL_REF_GVU_CONFIG_REGS_NEO_H
#define RTL_REF_GVU_CONFIG_REGS_NEO_H

#include <array>
#include <cstdint>

namespace rtl_ref_gvu
{
    enum GvuCfgRegNeo : unsigned
    {
        NEO_CONTROL = 0, NEO_OPCODE = 1, NEO_LUT = 2, NEO_GEMM_FUSED = 3, NEO_SOFTMAX_0 = 4, NEO_SOFTMAX_1 = 5, NEO_SOFTMAX_2 = 6,
        NEO_RESIDUAL_0 = 7, NEO_RESIDUAL_1 = 8, NEO_RESIDUAL_2 = 9, NEO_RESIDUAL_3 = 10, NEO_RESIDUAL_4 = 11, NEO_POOLING_0 = 12, NEO_POOLING_1 = 13
    };

    struct GvuConfigNeoIn
    {
        bool rstn{true};
        unsigned byte_addr{0};          // IF_AW = 6 bits
        bool wren{false}, rden{false};
        uint32_t wmask{0xFFFFFFFFu};    // one mask bit per data bit
        uint32_t wdata{0};
        bool gvu_pipe_done{false};
    };

    struct GvuConfigNeoOut
    {
        bool pipe_en, done_intr;
        unsigned op;
        bool lut_act_reset, lut_exp_reset, lut_recip_reset, lut_rsqrt_reset;
        bool sbank_buf_sel, vbank0_buf_sel, vbank1_buf_sel, vbank2_buf_sel;
        bool gemm_fused_skip_act; int8_t gemm_fused_z;
        unsigned num_tile, token_len;                 // 16 bits each
        int8_t attn_z; int32_t attn_scale; unsigned attn_shift;
        unsigned residual_sequence_len; bool residual_sub_mode; int32_t residual_scale_a, residual_scale_b; int64_t residual_bias;
        int8_t residual_out_z; unsigned residual_out_shift;
        unsigned pooling_mode, pooling_stride, pooling_num_w_tile, pooling_c, pooling_oh, pooling_c_move, pooling_h_move;
        uint32_t rdata;
    };

    class GvuConfigRegsNeo
    {
    public:
        static constexpr unsigned kNumRegs = 14;       // the RTL
        static constexpr unsigned kMaxRegs = 32;       // the proposal
        explicit GvuConfigRegsNeo(unsigned num_regs = kNumRegs, unsigned rev = 0) : nr_(num_regs == kMaxRegs ? kMaxRegs : kNumRegs), rev_(rev) { reset_all(); }
        void reset_all() { cfg_.fill(0u); rst_flops(); rdata_ = 0; }

        void tick(const GvuConfigNeoIn &in)
        {
            const unsigned a = (in.byte_addr >> 2) & (nr_ == kMaxRegs ? 31u : 15u);
            if (!in.rstn) { rst_flops(); if (in.rden) rdata_ = a < nr_ ? cfg_[a] : 0u; return; }   // cfg_reg is not reset; o_rdata still loads
            // read samples the value from before the edge
            const uint32_t rd = a < nr_ ? cfg_[a] : 0u;
            std::array<uint32_t, kMaxRegs> n = cfg_;
            bool pe_d = in.gvu_pipe_done ? false : pe_q_;
            bool pd_d = in.gvu_pipe_done ? true : pd_q_;
            bool lr_d[4]; for (int i = 0; i < 4; i++) lr_d[i] = false;  // lut_x_reset_d = lut_x_reset_q ? 0 : lut_x_reset_q  ==> always 0 unless written
            if (in.wren)
            {
                if (a == NEO_CONTROL)
                {
                    if (in.wmask & 1u) pe_d = in.wdata & 1u;
                    if ((in.wmask >> 2) & 1u) pd_d = pd_q_ && !((in.wdata >> 2) & 1u);
                }
                else if (a == NEO_LUT)
                {
                    for (int i = 0; i < 4; i++) if ((in.wmask >> i) & 1u) lr_d[i] = (in.wdata >> i) & 1u;
                }
                else if (a < nr_)
                    n[a] = (n[a] & ~in.wmask) | (in.wdata & in.wmask);
            }
            n[NEO_CONTROL] = (n[NEO_CONTROL] & ~3u) | (pe_d ? 1u : 0u) | (pd_d ? 2u : 0u);
            const bool edge = pe_d && !pe_q_;
            if (edge) { for (int i = 0; i < 4; i++) buf_[i] = !buf_[i]; edges_++; }
            else if (rev_ >= 1 && in.wren && a == NEO_CONTROL && ((in.wmask >> 3) & 1u) && ((in.wdata >> 3) & 1u)) { for (int i = 0; i < 4; i++) buf_[i] = !buf_[i]; forced_++; }
            pe_q_ = pe_d; pd_q_ = pd_d; for (int i = 0; i < 4; i++) lr_q_[i] = lr_d[i];
            cfg_ = n;
            if (in.rden) rdata_ = rd;
        }

        uint32_t reg(unsigned i) const { return i < nr_ ? cfg_[i] : 0u; }
        uint64_t enable_edges() const { return edges_; }
        uint64_t forced_swaps() const { return forced_; }
        unsigned rev() const { return rev_; }
        unsigned num_regs() const { return nr_; }
        bool lut_pulse() const { return lr_q_[0] || lr_q_[1] || lr_q_[2] || lr_q_[3]; }   // a LUT reset flop is high: the next edge clears it
        bool buf(unsigned bank) const { return buf_[bank & 3u]; }                          // 0 sbank, 1 vbank0, 2 vbank1, 3 vbank2

        GvuConfigNeoOut out() const
        {
            GvuConfigNeoOut o{};
            o.pipe_en = pe_q_; o.done_intr = pd_q_; o.op = cfg_[NEO_OPCODE] & 15u;
            o.lut_act_reset = lr_q_[0]; o.lut_exp_reset = lr_q_[1]; o.lut_recip_reset = lr_q_[2]; o.lut_rsqrt_reset = lr_q_[3];
            o.sbank_buf_sel = buf_[0]; o.vbank0_buf_sel = buf_[1]; o.vbank1_buf_sel = buf_[2]; o.vbank2_buf_sel = buf_[3];
            const uint32_t g = cfg_[NEO_GEMM_FUSED];
            o.gemm_fused_skip_act = g & 1u; o.gemm_fused_z = static_cast<int8_t>((g >> 8) & 0xFFu);
            o.token_len = cfg_[NEO_SOFTMAX_0] & 0xFFFFu; o.num_tile = cfg_[NEO_SOFTMAX_0] >> 16;
            o.attn_scale = static_cast<int32_t>(cfg_[NEO_SOFTMAX_1]);
            o.attn_shift = cfg_[NEO_SOFTMAX_2] & 0xFFu; o.attn_z = static_cast<int8_t>((cfg_[NEO_SOFTMAX_2] >> 8) & 0xFFu);
            const uint32_t r0 = cfg_[NEO_RESIDUAL_0];
            o.residual_sequence_len = r0 & 0xFFFFu; o.residual_out_z = static_cast<int8_t>((r0 >> 16) & 0xFFu);
            o.residual_out_shift = (r0 >> 24) & 0x3Fu; o.residual_sub_mode = (r0 >> 30) & 1u;
            o.residual_scale_a = static_cast<int32_t>(cfg_[NEO_RESIDUAL_1]); o.residual_scale_b = static_cast<int32_t>(cfg_[NEO_RESIDUAL_2]);
            o.residual_bias = static_cast<int64_t>((static_cast<uint64_t>(cfg_[NEO_RESIDUAL_4]) << 32) | cfg_[NEO_RESIDUAL_3]);
            const uint32_t p0 = cfg_[NEO_POOLING_0], p1 = cfg_[NEO_POOLING_1];
            o.pooling_mode = p0 & 3u; o.pooling_stride = (p0 >> 2) & 7u; o.pooling_num_w_tile = (p0 >> 8) & 0xFFu;
            o.pooling_c = (p0 >> (rev_ >= 1 ? 24 : 16)) & 0xFFu; o.pooling_oh = (p0 >> (rev_ >= 1 ? 16 : 24)) & 0xFFu;
            o.pooling_h_move = p1 & 0x3FFu; o.pooling_c_move = (p1 >> 16) & 0x3FFu;
            o.rdata = rdata_;
            return o;
        }

    private:
        unsigned nr_{kNumRegs};
        unsigned rev_{0};
        std::array<uint32_t, kMaxRegs> cfg_{};
        bool pe_q_{false}, pd_q_{false}, lr_q_[4]{}, buf_[4]{};
        uint32_t rdata_{0};
        uint64_t edges_{0}, forced_{0};
        void rst_flops() { pe_q_ = false; pd_q_ = false; for (int i = 0; i < 4; i++) { lr_q_[i] = false; buf_[i] = false; } }
    };
}

#endif
