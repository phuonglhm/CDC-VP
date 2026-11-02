// gvu_regs_neo_shadow.h -- HAS_GVU_REGS_NEO=1: the register map of neo_lite (has/rtl_ref_gvu_config_regs_neo.h) run in parallel with the register file of the model.
// The model keeps its own register file (GvuConfigRegs: map of the previous RTL drop plus the model's proposals: AGU_ND, average pool, residual add) as the FUNCTIONAL one, so
// values and the model's cycles do not change. Every tick of it is translated into writes of the new map (opcode apart from CONTROL, LUT reset as a pulse,
// 16-bit token / tile fields, POOLING at 12 / 13 with the C move at [25:16]); after every tick the fields present in both maps are compared (opcode, enable,
// done flag, GEMM_FUSED, Softmax, pooling): a difference is counted, not repaired. The buffer-select flops of the new map flip on the enable edge: the edges are counted.
// Cost (HAS_GVU_REGS_NEO): the DFC writes 14 registers + CONTROL for every command (neo_interface.sv counts GVU_CFG_REGS = 13 down to 0), one write per edge when the
// receiver is ready: MEASURED without mux and CDC (a lower bound), env HAS_GVU_NEO_WRITE_LAT (default 1) sets the edges per write; sequences are padded to 14 writes.
// NOT done: the pokes of the model into the scratchpad buffers do not follow the new flip parity (the model keeps its buffers).
#ifndef HAS_GVU_REGS_NEO_SHADOW_H
#define HAS_GVU_REGS_NEO_SHADOW_H

#include <cstdint>
#include <cstdlib>
#include "has/rtl_ref_gvu_config_regs.h"
#include "has/rtl_ref_gvu_config_regs_neo.h"

namespace has
{
    struct NeoShadowStats
    {
        uint64_t ticks{0}, neo_writes{0}, opcode_writes{0}, lut_pulses{0}, lut_bad{0}, field_checks{0}, mismatches{0}, done_race{0}, enable_edges{0}, pad_writes{0}, sequences{0};
        uint64_t bad_enable{0}, bad_done{0}, bad_op{0}, bad_gemm{0}, bad_softmax{0}, bad_pool{0};
    };
    inline NeoShadowStats &neo_shadow_stats() { static NeoShadowStats s; return s; }

    class GvuRegsNeoShadow
    {
    public:
        static constexpr unsigned kCfgWrites = 14;     // neo_interface: GVU_CFG_REGS (13) counted down to 0
        static bool on() { static const bool v = [] { const char *e = std::getenv("HAS_GVU_REGS_NEO"); return e && std::atoi(e) != 0; }(); return v; }
        static unsigned write_lat() { static const unsigned v = [] { const char *e = std::getenv("HAS_GVU_NEO_WRITE_LAT"); const int l = e ? std::atoi(e) : 1; return static_cast<unsigned>(l < 1 ? 1 : l); }(); return v; }

        // called after every tick of the model's register file (old index c.addr, byte-lane masks)
        void observe(const rtl_ref_gvu::GvuConfigIn &c, const rtl_ref_gvu::GvuConfigRegs &old)
        {
            namespace R = rtl_ref_gvu;
            NeoShadowStats &st = neo_shadow_stats();
            st.ticks++;
            R::GvuConfigNeoIn n;
            n.rstn = c.rstn;
            if (!c.rstn) { neo_.tick(n); return; }
            bool lut_expect = false; uint32_t lut_bits = 0;
            if (c.wren)
            {
                const bool lane0 = (c.wmask & 1u) != 0;
                switch (c.addr)
                {
                case 0:   // CONTROL: enable / clear done stay, the opcode moves to its own register
                    if (lane0)
                    {
                        R::GvuConfigNeoIn op; op.byte_addr = 4u * R::NEO_OPCODE; op.wren = true; op.wdata = (c.wdata >> 3) & 15u; op.wmask = 0xFu; neo_.tick(op); st.opcode_writes++; st.neo_writes++;
                        n.byte_addr = 0; n.wren = true; n.wdata = c.wdata & 5u; n.wmask = 5u;
                        if (c.gvu_pipe_done) { n.wmask = 4u; st.done_race++; }   // the model gives pipe_done the priority over a write of the enable bit; the new RTL gives it to the host
                        st.neo_writes++;
                    }
                    break;
                case 1:   // LUT reset: a pulse in the new map
                    if (lane0) { n.byte_addr = 4u * R::NEO_LUT; n.wren = true; n.wdata = c.wdata & 0xFu; n.wmask = 0xFu; lut_expect = true; lut_bits = c.wdata & 0xFu; st.neo_writes++; st.lut_pulses++; }
                    break;
                case 3: put(n, R::NEO_GEMM_FUSED, old.reg(3)); break;
                case 4: { const uint32_t v = old.reg(4); put(n, R::NEO_SOFTMAX_0, (((v >> 9) & 0x1FFu) << 16) | (v & 0x1FFu)); break; }
                case 5: put(n, R::NEO_SOFTMAX_1, old.reg(5)); break;
                case 6: put(n, R::NEO_SOFTMAX_2, old.reg(6)); break;
                case 7: put(n, R::NEO_POOLING_0, old.reg(7)); break;
                case 8: { const uint32_t v = old.reg(8); put(n, R::NEO_POOLING_1, (v & 0x3FFu) | (((v >> 10) & 0x3FFu) << 16)); break; }
                default: break;                                      // SCRATCHPAD (no write in the new map), the model's proposals (9..31)
                }
            }
            n.gvu_pipe_done = c.gvu_pipe_done;
            neo_.tick(n);
            st.enable_edges = neo_.enable_edges();
            if (lut_expect)
            {
                const R::GvuConfigNeoOut o = neo_.out();
                const uint32_t got = (o.lut_act_reset ? 1u : 0u) | (o.lut_exp_reset ? 2u : 0u) | (o.lut_recip_reset ? 4u : 0u) | (o.lut_rsqrt_reset ? 8u : 0u);
                if (got != lut_bits) st.lut_bad++;
            }
            compare(old);
        }

        const rtl_ref_gvu::GvuConfigRegsNeo &neo() const { return neo_; }

    private:
        rtl_ref_gvu::GvuConfigRegsNeo neo_;
        void put(rtl_ref_gvu::GvuConfigNeoIn &n, unsigned reg, uint32_t v) { n.byte_addr = 4u * reg; n.wren = true; n.wdata = v; n.wmask = 0xFFFFFFFFu; neo_shadow_stats().neo_writes++; }
        void compare(const rtl_ref_gvu::GvuConfigRegs &old)
        {
            NeoShadowStats &st = neo_shadow_stats();
            const rtl_ref_gvu::GvuConfigOut a = old.out();
            const rtl_ref_gvu::GvuConfigNeoOut b = neo_.out();
            bool bad = false;
            st.field_checks++;
            if (a.pipe_en != b.pipe_en) { st.bad_enable++; bad = true; }
            if (static_cast<bool>(old.reg(rtl_ref_gvu::CFG_CONTROL) & 2u) != b.done_intr) { st.bad_done++; bad = true; }
            if (a.op != b.op) { st.bad_op++; bad = true; }
            if (a.gemm_fused_skip_act != b.gemm_fused_skip_act || a.gemm_fused_z != b.gemm_fused_z) { st.bad_gemm++; bad = true; }
            if (a.token_len != b.token_len || a.num_tile != b.num_tile || a.attn_scale != b.attn_scale || a.attn_shift != b.attn_shift || a.attn_z != b.attn_z) { st.bad_softmax++; bad = true; }
            if (a.pooling_mode != b.pooling_mode || a.pooling_stride != b.pooling_stride || a.pooling_num_w_tile != b.pooling_num_w_tile || a.pooling_c != b.pooling_c ||
                a.pooling_oh != b.pooling_oh || a.pooling_c_move != b.pooling_c_move || a.pooling_h_move != b.pooling_h_move) { st.bad_pool++; bad = true; }
            if (bad) st.mismatches++;
        }
    };
}

#endif
