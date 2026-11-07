// gvu_regs_neo_shadow.h -- HAS_GVU_REGS_NEO=1: the register map of neo_lite (has/rtl_ref_gvu_config_regs_neo.h) as the FUNCTIONAL register file of the vector unit.
// GvuRegFile is the file that GvuTopStream holds: with the knob off it is the register file of the previous RTL drop (has/rtl_ref_gvu_config_regs.h) and nothing
// changes; with the knob on every access goes to GvuConfigRegsNeo (32 registers: the 14 of the RTL plus the model's proposals above them, see has/gvu_regs_dfc.h
// namespace neo) and the command sequences are the ones of gvu_regs_dfc::neo. What the knob changes besides the map:
//   * the four buffer-select flops flip on the enable edge of every command; the model loads and reads the buffers they select (HAS_GVU_NEO_PP=0: fixed buffers);
//   * each command is written in full: 14 configuration registers before CONTROL (neo_interface.sv counts GVU_CFG_REGS = 13 down to 0), 1 write per edge
//     (HAS_GVU_NEO_WRITE_LAT, MEASURED without mux and CDC), and the multiplexer + clock-domain crossing of neo_top adds HAS_GVU_NEO_CDC_LAT edges (default 4,
//     MEASURED on the Verilator model of neo_top) once per chain of writes.
// Not done: Softmax / FA / LN keep fixed buffers (GvuTop flips the vbank1 bit on lut_init_done for them); a read-back of the result is a back door of the model.
#ifndef HAS_GVU_REGS_NEO_SHADOW_H
#define HAS_GVU_REGS_NEO_SHADOW_H

#include <cstdint>
#include <cstdlib>
#include "has/rtl_ref_gvu_config_regs.h"
#include "has/rtl_ref_gvu_config_regs_neo.h"
#include "has/neo_rev.h"

namespace has
{
    struct NeoShadowStats
    {
        uint64_t ticks{0}, neo_writes{0}, lut_pulse_writes{0}, done_race{0}, enable_edges{0}, pad_writes{0}, sequences{0}, cdc_chains{0};
        uint64_t pp_runs{0}, pp_loads_buf1{0}, pp_flop_bad{0}, shift_out_cut{0}, forced_swaps{0};
    };
    inline NeoShadowStats &neo_shadow_stats() { static NeoShadowStats s; return s; }

    struct GvuRegsNeoKnobs
    {
        static constexpr unsigned kCfgWrites = 14;     // neo_interface: GVU_CFG_REGS (13) counted down to 0
        static bool on() { static const bool v = [] { const char *e = std::getenv("HAS_GVU_REGS_NEO"); return e && std::atoi(e) != 0; }(); return v; }
        static unsigned write_lat() { static const unsigned v = [] { const char *e = std::getenv("HAS_GVU_NEO_WRITE_LAT"); const int l = e ? std::atoi(e) : 1; return static_cast<unsigned>(l < 1 ? 1 : l); }(); return v; }
        static unsigned cdc_lat() { static const unsigned v = [] { const char *e = std::getenv("HAS_GVU_NEO_CDC_LAT"); const int l = e ? std::atoi(e) : 4; return static_cast<unsigned>(l < 0 ? 0 : l); }(); return v; }
        static bool pp_on() { static const bool v = [] { const char *e = std::getenv("HAS_GVU_NEO_PP"); return !e || std::atoi(e) != 0; }(); return v; }
    };

    class GvuRegFile
    {
    public:
        GvuRegFile() {}
        // neo = false: the previous map with the parameters cp; neo = true: GvuConfigRegsNeo with 32 registers (cp only gives the addressing) of the revision HAS_NEO_REV
        void configure(const rtl_ref_gvu::GvuConfigParams &cp, bool neo)
        {
            old_ = rtl_ref_gvu::GvuConfigRegs(cp);
            neo_on_ = neo;
            if (neo) neo_ = rtl_ref_gvu::GvuConfigRegsNeo(rtl_ref_gvu::GvuConfigRegsNeo::kMaxRegs, neo_rev());
        }
        bool neo() const { return neo_on_; }
        const rtl_ref_gvu::GvuConfigParams &params() const { return old_.params(); }
        unsigned reg_index(uint32_t byte_addr) const { return old_.reg_index(byte_addr); }       // word addressed, 5 bits: the same for both maps
        static uint32_t strb_to_mask(unsigned strb) { return rtl_ref_gvu::GvuConfigRegs::strb_to_mask(strb); }

        void tick(const rtl_ref_gvu::GvuConfigIn &in)
        {
            if (!neo_on_) { old_.tick(in); return; }
            namespace R = rtl_ref_gvu;
            NeoShadowStats &st = neo_shadow_stats();
            st.ticks++;
            R::GvuConfigNeoIn n;
            n.rstn = in.rstn; n.byte_addr = in.addr << 2; n.wren = in.wren; n.rden = in.rden; n.wmask = in.wmask; n.wdata = in.wdata; n.gvu_pipe_done = in.gvu_pipe_done;
            if (in.wren) st.neo_writes++;
            if (in.wren && in.addr == R::NEO_LUT) st.lut_pulse_writes++;
            if (in.wren && in.addr == R::NEO_CONTROL && in.gvu_pipe_done && (in.wmask & 1u)) st.done_race++;   // the RTL gives the host the priority over pipe_done on the enable bit
            neo_.tick(n);
            st.enable_edges = neo_.enable_edges(); st.forced_swaps = neo_.forced_swaps();
        }
        void write_axi(uint32_t byte_addr, uint32_t data, unsigned strb = 0xF, bool pipe_done = false, bool lut_init_done = false)
        {
            rtl_ref_gvu::GvuConfigIn in; in.addr = reg_index(byte_addr); in.wren = true; in.wdata = data; in.wmask = strb_to_mask(strb);
            in.gvu_pipe_done = pipe_done; in.lut_init_done = lut_init_done; tick(in);
            if (neo_on_ && neo_.lut_pulse()) { rtl_ref_gvu::GvuConfigIn e; tick(e); }              // the call style has no clock between its writes: let the pulse end
        }
        uint32_t reg(unsigned i) const { return neo_on_ ? neo_.reg(i) : old_.reg(i); }
        bool lut_pulse() const { return neo_on_ && neo_.lut_pulse(); }
        bool buf(unsigned bank) const { return neo_on_ && neo_.buf(bank); }                          // 0 sbank, 1 vbank0, 2 vbank1, 3 vbank2

        rtl_ref_gvu::GvuConfigOut out() const
        {
            if (!neo_on_) return old_.out();
            namespace R = rtl_ref_gvu;
            const R::GvuConfigNeoOut b = neo_.out();
            R::GvuConfigOut o{};
            o.pipe_en = b.pipe_en; o.op = b.op;
            o.lut_act_reset = b.lut_act_reset; o.lut_exp_reset = b.lut_exp_reset; o.lut_recip_reset = b.lut_recip_reset; o.lut_rsqrt_reset = b.lut_rsqrt_reset;
            o.sbank_buf_sel = b.sbank_buf_sel; o.vbank0_buf_sel = b.vbank0_buf_sel; o.vbank1_buf_sel = b.vbank1_buf_sel; o.vbank2_buf_sel = b.vbank2_buf_sel;
            o.gemm_fused_skip_act = b.gemm_fused_skip_act; o.gemm_fused_z = b.gemm_fused_z;
            const bool res = b.op == 6u;                                                             // gvu_opcode_t GVU_RESIDUAL
            o.token_len = res ? b.residual_sequence_len : b.token_len; o.num_tile = res ? 1u : b.num_tile;
            o.attn_z = b.attn_z; o.attn_scale = b.attn_scale; o.attn_shift = b.attn_shift;
            o.pooling_mode = b.pooling_mode; o.pooling_stride = b.pooling_stride; o.pooling_num_w_tile = b.pooling_num_w_tile;
            o.pooling_c = b.pooling_c; o.pooling_oh = b.pooling_oh; o.pooling_c_move = b.pooling_c_move; o.pooling_h_move = b.pooling_h_move;
            // residual add of the model: RESIDUAL_0..2 of the RTL plus the proposals 25..29
            o.res_scale_a = b.residual_scale_a; o.res_scale_b = b.residual_scale_b; o.res_scale_out = static_cast<int32_t>(neo_.reg(25));
            const uint32_t sh = neo_.reg(26), zp = neo_.reg(27), rw = neo_.reg(28);
            o.res_shift_a = sh & 0xFFu; o.res_shift_b = (sh >> 8) & 0xFFu; o.res_shift_out = b.residual_out_shift;
            o.res_round = (sh >> 24) & 7u; o.res_out_int16 = (sh >> 27) & 1u;
            o.res_zp_a = static_cast<int8_t>(zp & 0xFFu); o.res_zp_b = static_cast<int8_t>((zp >> 8) & 0xFFu); o.res_zp_out = b.residual_out_z;
            o.res_a_row = rw & 0x3FFu; o.res_b_row = (rw >> 10) & 0x3FFu; o.res_sum_row = (rw >> 20) & 0x3FFu;
            o.res_last_lane_mask = neo_.reg(29);
            o.rdata = b.rdata;
            return o;
        }

    private:
        rtl_ref_gvu::GvuConfigRegs old_;
        rtl_ref_gvu::GvuConfigRegsNeo neo_;
        bool neo_on_{false};
    };
}

#endif
