// rtl_ref_gvu_config_regs.h -- configuration register file of the vector unit, ported line by line from the latest RTL drop:
// neo_core/gvu/gvu_config_regs.sv (registers, decode) with the word-index choice of gvu_core.sv:252. Pure C++17, no SystemC.
// Nothing here is used by the instruction path yet: it is the model of what a DFC / host drives through the AXI-Lite port of gvu_core.
//
// Register map (gvu_pkg.sv gvu_cfg_reg_t, index = 4-bit register number; 16 registers of 32 bits exist, 9 are used):
//   0 CONTROL      [0] pipeline enable   [1] pipeline done flag   [2] clear done flag (write 1)   [6:3] opcode (gvu_opcode_t)
//   1 LUT          [0] act reset  [1] exp reset  [2] recip reset  [3] rsqrt reset
//   2 SCRATCHPAD   [0] sbank buf  [1] vbank0 buf  [2] vbank1 buf (toggles on lut_init_done)  [3] vbank2 buf
//   3 GEMM_FUSED   [0] skip activation  [15:8] output zero point (int8)
//   4 SOFTMAX_0    [8:0] token length   [17:9] number of tiles
//   5 SOFTMAX_1    [31:0] attention scale (int32)
//   6 SOFTMAX_2    [7:0] attention shift   [15:8] attention zero point (int8)
//   7 POOLING_0    [1:0] window mode  [4:2] stride  [15:8] number of W tiles  [23:16] C  [31:24] OH   (code; the RTL comment swaps C / OH, B11)
//   8 POOLING_1    [9:0] H move   [19:10] C move  (VBANK0_AW = 10 bits each)
// Registers 9..15 are plain storage in the RTL drop (no decode). The register proposal for the residual add (not RTL) decodes them:
//   9 RES_SCALE_A [31:0] scale_a   10 RES_SCALE_B [31:0] scale_b   11 RES_SCALE_OUT [31:0] scale_out   (int32)
//  12 RES_SHIFT  [7:0] shift_a  [15:8] shift_b  [23:16] shift_out  [26:24] rounding mode (round_mode_e)  [27] out_int16
//  13 RES_ZP     [7:0] zp_a  [15:8] zp_b  [23:16] zp_out  (int8)
//  14 RES_ROW    [9:0] first row of A (vbank0)  [19:10] first row of B (vbank1)  [29:20] first row of the int32 sum (vbank2)
//  15 RES_LASTMASK [31:0] lane mask of the last row of the last run
// The res_* outputs exist for num_regs = 16 and 32 alike (registers 9..15 are there in both); nothing in the RTL drop drives them.
// num_regs = 32 (GVU_REG_AW = 5, proposal): registers 16..31 are storage with no decode here (16..24 are claimed by the AGU_ND proposal).
// Behaviour of the RTL that this port keeps, edge by edge (one tick() = one rising clock edge):
//   * write: for i in {0, 8, 16, 24}: if wmask[i] then byte (i*8 +: 8) of the register gets wdata. As DELIVERED (b13_fixed = false)
//     only i = 0 lands inside the 32-bit register (B13): only byte 0 can be written, with wmask bit 0. With the proposed B13 patch
//     (b13_fixed = true, byte (i +: 8)) all four bytes work, mask = bit mask (gvu_core feeds it from AXI strb, one strb bit -> 8 mask bits).
//   * after the write, in this order, later wins: pipe_done clears CONTROL[0] (so pipe_done beats a host write of bit 0, K-19) and sets
//     CONTROL[1]; the clear request (wren && addr == 0 && wdata[2], mask NOT looked at) clears CONTROL[1]; lut_init_done toggles
//     SCRATCHPAD[2] (beats a host write of that bit, toggles the value from BEFORE the edge).
//   * read: o_rdata <= reg[addr] when rden, registered (1 cycle), no reset (0 here before the first read; x in a 4-state simulator).
//   * async reset (rstn low) clears all 16 registers at once; o_rdata is not reset and still loads while rstn is low.
// Word index (gvu_core.sv:252): the AXI-Lite byte address goes to the registers as reg_addr[3:0] as delivered (B17: offset 4n hits
// register 4n mod 16), as reg_addr[5:2] with the proposed B17 patch (word_addressed = true). With num_regs = 32 the index has 5 bits.
#ifndef RTL_REF_GVU_CONFIG_REGS_H
#define RTL_REF_GVU_CONFIG_REGS_H

#include <array>
#include <cstdint>

namespace rtl_ref_gvu
{
    enum GvuCfgReg : unsigned
    {
        CFG_CONTROL = 0, CFG_LUT = 1, CFG_SCRATCHPAD = 2, CFG_GEMM_FUSED = 3, CFG_SOFTMAX_0 = 4, CFG_SOFTMAX_1 = 5, CFG_SOFTMAX_2 = 6,
        CFG_POOLING_0 = 7, CFG_POOLING_1 = 8,
        // residual add proposal (not in the RTL drop)
        CFG_RES_SCALE_A = 9, CFG_RES_SCALE_B = 10, CFG_RES_SCALE_OUT = 11, CFG_RES_SHIFT = 12, CFG_RES_ZP = 13, CFG_RES_ROW = 14, CFG_RES_LASTMASK = 15
    };

    struct GvuConfigParams
    {
        bool b13_fixed{false};        // false: RTL as delivered (only byte 0 writable); true: patch B13 (all four bytes)
        bool word_addressed{false};   // false: index = byte address [3:0] (as delivered); true: [5:2] (patch B17)
        // Simulator-dependent behaviour of the delivered RTL (b13_fixed = false), measured: ModelSim ignores the part-select [i*8 +: 8]
        // for i >= 8 (outside the 32-bit register); Verilator 5.052 treats it as a select on the flat 512-bit register vector, so the
        // write (data 0) lands in byte 0 of register a + 2 * (i / 8). flat_array_oob = true reproduces Verilator. Default: standard SV.
        bool flat_array_oob{false};
        unsigned vbank0_aw{10};       // VBANK0_AW = clog2(800): width of the two pooling move fields
        unsigned num_regs{16};        // GVU_NUM_REGS: 16 (RTL drop, 4-bit index) or 32 (proposal, GVU_REG_AW = 5); other values count as 16
        static constexpr unsigned kMaxRegs = 32;
    };

    // One clock edge of inputs.
    struct GvuConfigIn
    {
        bool rstn{true};              // async reset, active low
        unsigned addr{0};             // register index (4 bits, 5 bits with num_regs = 32)
        bool wren{false}, rden{false};
        uint32_t wmask{0xFFFFFFFFu};  // bit mask
        uint32_t wdata{0};
        bool lut_init_done{false};    // i_lut_init_done
        bool gvu_pipe_done{false};    // i_gvu_pipe_done
    };

    // Everything gvu_config_regs drives (decode of the registers, valid after the edge).
    struct GvuConfigOut
    {
        bool pipe_en;                           // o_gvu_pipe_en
        unsigned op;                            // o_gvu_op (4 bits, gvu_opcode_t)
        bool lut_act_reset, lut_exp_reset, lut_recip_reset, lut_rsqrt_reset;
        bool sbank_buf_sel, vbank0_buf_sel, vbank1_buf_sel, vbank2_buf_sel;
        bool gemm_fused_skip_act;
        int8_t gemm_fused_z;
        unsigned num_tile, token_len;           // 9 bits each
        int8_t attn_z;
        int32_t attn_scale;
        unsigned attn_shift;                    // 8 bits
        unsigned pooling_mode, pooling_stride;  // 2, 3 bits
        unsigned pooling_num_w_tile, pooling_c, pooling_oh;   // 8 bits each
        unsigned pooling_c_move, pooling_h_move;              // vbank0_aw bits each
        // residual add proposal (registers 9..15, see above)
        int32_t res_scale_a, res_scale_b, res_scale_out;
        unsigned res_shift_a, res_shift_b, res_shift_out;     // 8 bits each
        unsigned res_round;                                   // 3 bits
        bool res_out_int16;
        int8_t res_zp_a, res_zp_b, res_zp_out;
        unsigned res_a_row, res_b_row, res_sum_row;           // 10 bits each
        uint32_t res_last_lane_mask;
        uint32_t rdata;                         // o_rdata
    };

    class GvuConfigRegs
    {
    public:
        explicit GvuConfigRegs(const GvuConfigParams &p = GvuConfigParams()) : P_(p) { P_.num_regs = (p.num_regs == 32) ? 32u : 16u; reset(); }
        const GvuConfigParams &params() const { return P_; }
        void reset() { reg_.fill(0u); }

        // One rising edge. Order as in reg_wr_proc / the read process of the RTL.
        void tick(const GvuConfigIn &in)
        {
            const unsigned idx_mask = P_.num_regs - 1u;
            const uint32_t cur_rd = reg_[in.addr & idx_mask];  // read process samples the value from before the edge
            if (!in.rstn)
                reset();
            else
            {
                std::array<uint32_t, GvuConfigParams::kMaxRegs> n = reg_;
                const unsigned a = in.addr & idx_mask;
                if (in.wren)
                    for (unsigned i = 0; i < 32; i += 8)
                        if ((in.wmask >> i) & 1u)
                        {
                            const unsigned lo = P_.b13_fixed ? i : i * 8;      // [i +: 8] (patch) or [i*8 +: 8] (as delivered)
                            if (lo + 8 <= 32)                                  // outside the 32-bit register: no write
                                n[a] = (n[a] & ~(0xFFu << lo)) | (((in.wdata >> lo) & 0xFFu) << lo);
                            else if (P_.flat_array_oob)                        // Verilator: flat vector, data read outside i_wdata = 0
                            {
                                const unsigned t = a + lo / 32;
                                if (t < P_.num_regs) n[t] = (n[t] & ~(0xFFu << (lo % 32)));
                            }
                        }
                if (in.gvu_pipe_done) n[CFG_CONTROL] &= ~1u;
                if (in.gvu_pipe_done) n[CFG_CONTROL] |= 2u;
                if (in.wren && a == CFG_CONTROL && ((in.wdata >> 2) & 1u)) n[CFG_CONTROL] &= ~2u;   // pipe_done_clear
                if (in.lut_init_done) n[CFG_SCRATCHPAD] = (n[CFG_SCRATCHPAD] & ~4u) | ((~reg_[CFG_SCRATCHPAD]) & 4u);
                reg_ = n;
            }
            if (in.rden) rdata_ = (in.rstn ? cur_rd : 0u);          // while rstn is low the registers are 0 when the edge samples them
        }

        uint32_t reg(unsigned i) const { return reg_[i & (P_.num_regs - 1u)]; }
        uint32_t rdata() const { return rdata_; }

        GvuConfigOut out() const
        {
            GvuConfigOut o;
            const uint32_t c = reg_[CFG_CONTROL];
            o.pipe_en = c & 1u; o.op = (c >> 3) & 15u;
            const uint32_t l = reg_[CFG_LUT];
            o.lut_act_reset = l & 1u; o.lut_exp_reset = (l >> 1) & 1u; o.lut_recip_reset = (l >> 2) & 1u; o.lut_rsqrt_reset = (l >> 3) & 1u;
            const uint32_t s = reg_[CFG_SCRATCHPAD];
            o.sbank_buf_sel = s & 1u; o.vbank0_buf_sel = (s >> 1) & 1u; o.vbank1_buf_sel = (s >> 2) & 1u; o.vbank2_buf_sel = (s >> 3) & 1u;
            const uint32_t g = reg_[CFG_GEMM_FUSED];
            o.gemm_fused_skip_act = g & 1u; o.gemm_fused_z = static_cast<int8_t>((g >> 8) & 0xFFu);
            const uint32_t s0 = reg_[CFG_SOFTMAX_0];
            o.token_len = s0 & 0x1FFu; o.num_tile = (s0 >> 9) & 0x1FFu;
            o.attn_scale = static_cast<int32_t>(reg_[CFG_SOFTMAX_1]);
            const uint32_t s2 = reg_[CFG_SOFTMAX_2];
            o.attn_shift = s2 & 0xFFu; o.attn_z = static_cast<int8_t>((s2 >> 8) & 0xFFu);
            const uint32_t p0 = reg_[CFG_POOLING_0];
            o.pooling_mode = p0 & 3u; o.pooling_stride = (p0 >> 2) & 7u; o.pooling_num_w_tile = (p0 >> 8) & 0xFFu;
            o.pooling_c = (p0 >> 16) & 0xFFu; o.pooling_oh = (p0 >> 24) & 0xFFu;
            const uint32_t m = (1u << P_.vbank0_aw) - 1u, p1 = reg_[CFG_POOLING_1];
            o.pooling_h_move = p1 & m; o.pooling_c_move = (p1 >> 10) & m;
            o.res_scale_a = static_cast<int32_t>(reg_[CFG_RES_SCALE_A]); o.res_scale_b = static_cast<int32_t>(reg_[CFG_RES_SCALE_B]);
            o.res_scale_out = static_cast<int32_t>(reg_[CFG_RES_SCALE_OUT]);
            const uint32_t rs = reg_[CFG_RES_SHIFT];
            o.res_shift_a = rs & 0xFFu; o.res_shift_b = (rs >> 8) & 0xFFu; o.res_shift_out = (rs >> 16) & 0xFFu;
            o.res_round = (rs >> 24) & 7u; o.res_out_int16 = (rs >> 27) & 1u;
            const uint32_t rz = reg_[CFG_RES_ZP];
            o.res_zp_a = static_cast<int8_t>(rz & 0xFFu); o.res_zp_b = static_cast<int8_t>((rz >> 8) & 0xFFu); o.res_zp_out = static_cast<int8_t>((rz >> 16) & 0xFFu);
            const uint32_t rr = reg_[CFG_RES_ROW];
            o.res_a_row = rr & 0x3FFu; o.res_b_row = (rr >> 10) & 0x3FFu; o.res_sum_row = (rr >> 20) & 0x3FFu;
            o.res_last_lane_mask = reg_[CFG_RES_LASTMASK];
            o.rdata = rdata_;
            return o;
        }

        // ---- the AXI-Lite side (gvu_core.sv: axi_lite_2ram in front of the registers) ----
        // strb (4 bits) -> bit mask (axi_lite_2ram.sv:518: every strb bit replicated over its 8 mask bits)
        static uint32_t strb_to_mask(unsigned strb)
        {
            uint32_t m = 0;
            for (unsigned b = 0; b < 4; b++) if ((strb >> b) & 1u) m |= 0xFFu << (8 * b);
            return m;
        }
        // register index the registers see for an AXI-Lite byte address
        unsigned reg_index(uint32_t byte_addr) const
        {
            const unsigned m = P_.num_regs - 1u;
            return P_.word_addressed ? ((byte_addr >> 2) & m) : (byte_addr & m);
        }
        // convenience: write / read the way the host does, one edge each
        void write_axi(uint32_t byte_addr, uint32_t data, unsigned strb = 0xF, bool pipe_done = false, bool lut_init_done = false)
        {
            GvuConfigIn in; in.addr = reg_index(byte_addr); in.wren = true; in.wdata = data; in.wmask = strb_to_mask(strb);
            in.gvu_pipe_done = pipe_done; in.lut_init_done = lut_init_done; tick(in);
        }
        uint32_t read_axi(uint32_t byte_addr)
        {
            GvuConfigIn in; in.addr = reg_index(byte_addr); in.rden = true; tick(in);
            return rdata_;   // READ_LATENCY(1) of axi_lite_2ram: the data is on o_rdata after this edge
        }

    private:
        GvuConfigParams P_;
        std::array<uint32_t, GvuConfigParams::kMaxRegs> reg_{};
        uint32_t rdata_{0};
    };
}

#endif
