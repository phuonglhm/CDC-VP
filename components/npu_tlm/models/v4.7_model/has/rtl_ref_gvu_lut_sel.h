// rtl_ref_gvu_lut_sel.h -- the LUT unit of GvuTop for the revision of the neo_lite RTL: a facade over the unit of RTL revisions 0 / 1
// (has/rtl_ref_gvu_lut.h, LutUnit) and the unit of RTL revision 2 (has/rtl_ref_gvu_lut_r2.h, LutUnitR2).
// Revision < 2: every call goes to LutUnit unchanged, so GvuTop behaves as it did before this header existed.
// Revision >= 2: GvuTop drives LutUnitR2 through the same In / Out (the table memory has three read stages, the lookup lane a request /
// response handshake): Softmax, LayerNorm and the stream registers see the answer when o_lookup_valid says so.
// GEMM_FUSED is the exception (decided with the coordinator): its activation byte is taken from the table in the cycle of the beat
// (gemm_byte), as with revision 1, and the three LUT cycles of the RTL stay inside the six stages that revision 2 adds to the depth of the
// GEMM_FUSED path (rtl_ref_gvu::GEMM_STREAM_DEPTH_REV2_DELTA; INFERRED from the RTL comment of the pipeline controller: 11 -> 17 = multiplier 2,
// Round-Shift 1, LUT 3). A later simulation of the whole unit decides whether the LUT cycles should instead be modelled in the GEMM_FUSED stream.
#ifndef RTL_REF_GVU_LUT_SEL_H
#define RTL_REF_GVU_LUT_SEL_H

#include "has/rtl_ref_gvu_lut.h"
#include "has/rtl_ref_gvu_lut_r2.h"

namespace rtl_ref_gvu
{
    class LutUnitSel
    {
    public:
        using In = LutUnit::In;
        using Out = LutUnit::Out;
        LutUnitSel(const LutParams &p, int neo_rev) : r1_(p), r2_(p), use_r2_(neo_rev >= 2) {}
        bool uses_r2() const { return use_r2_; }

        Out eval(const In &i) const
        {
            if (!use_r2_) return r1_.eval(i);
            const LutUnitR2::Out o2 = r2_.eval(to_r2(i));
            Out o{};
            o.o_lookup_ready = o2.o_lookup_ready; o.o_lookup_valid = o2.o_lookup_valid; o.o_init_done = o2.o_init_done; o.o_pipe_almost_done = o2.o_pipe_almost_done;
            for (size_t l = 0; l < o.o_8b_lookup_data.size(); l++) { o.o_8b_lookup_data[l] = o2.o_8b_lookup_data[l]; o.o_16b_lookup_data[l] = o2.o_16b_lookup_data[l]; o.o_e_shift[l] = o2.o_e_shift[l]; }
            return o;
        }
        void tick(const In &i, bool i_rstn = true)
        {
            if (!use_r2_) r1_.tick(i, i_rstn); else r2_.tick(to_r2(i), i_rstn);
        }
        // Done flag of the table load of the table selected by wsel. Revision 2: also needs the write enable of the cycle.
        bool o_init_done(LutType wsel, bool wen) const { return use_r2_ ? r2_.mem.o_init_done(wsel, wen) : r1_.mem.o_init_done(wsel); }
        // State read for callers without an In of the cycle (as LutUnit::o_pipe_almost_done); revision 2 answers for PWL only (the valid of pipe3).
        bool o_pipe_almost_done(LutIdxMode m = LUT_PWL) const { return use_r2_ ? r2_.o_pipe_almost_done(m) : r1_.o_pipe_almost_done(m); }
        // Byte of the activation table for lane l in the cycle of the request (GEMM_FUSED): revision < 2 the output of the unit as before,
        // revision 2 the table read at the request address (act / exp: index + 128 with compensation).
        uint8_t gemm_byte(const In &in, const Out &out, int l) const
        {
            if (!use_r2_) return out.o_8b_lookup_data[static_cast<size_t>(l)];
            const unsigned a = (in.i_8b_lookup_idx[static_cast<size_t>(l)] + (in.i_lut_compens ? 128u : 0u)) & 0xFFu;
            return r2_.mem.rdata8(in.i_lut_rsel, a);
        }

        // Table and write-counter reads for tools (tools/trace_gvu_top.cpp): the table memory of the revision in use.
        // Revision < 2: the LutMem of LutUnit as before. Revision 2: LutMemR2; it has no write address register, so lut_waddr() is 0 there.
        uint8_t peek8(LutType t, unsigned a) const { return use_r2_ ? r2_.mem.peek8(t, a) : r1_.mem.peek8(t, a); }
        uint16_t peek16(LutType t, unsigned a) const { return use_r2_ ? r2_.mem.peek16(t, a) : r1_.mem.peek16(t, a); }
        unsigned wr_cnt() const { return use_r2_ ? r2_.mem.wr_cnt() : r1_.mem.wr_cnt(); }
        unsigned lut_waddr() const { return use_r2_ ? 0u : r1_.mem.lut_waddr(); }

    private:
        LutUnit r1_;
        LutUnitR2 r2_;
        bool use_r2_;
        static LutUnitR2::In to_r2(const In &i)
        {
            LutUnitR2::In o;
            o.i_pipe_flush = i.i_pipe_flush; o.i_lookup_valid = i.i_lookup_valid; o.i_lut_wen = i.i_lut_wen; o.i_lut_wsel = i.i_lut_wsel; o.i_lut_wdata = i.i_lut_wdata;
            o.i_lut_rsel = i.i_lut_rsel; o.i_lut_idx_mode = i.i_lut_idx_mode; o.i_lut_compens = i.i_lut_compens;
            o.i_base = i.i_base; o.i_W = i.i_W; o.i_lerp_shift = i.i_lerp_shift; o.i_lookup_ready = i.i_lookup_ready;
            for (size_t l = 0; l < o.i_8b_lookup_idx.size(); l++) { o.i_8b_lookup_idx[l] = i.i_8b_lookup_idx[l]; o.i_32b_lookup_idx[l] = i.i_32b_lookup_idx[l]; }
            return o;
        }
    };
} // namespace rtl_ref_gvu

#endif // RTL_REF_GVU_LUT_SEL_H
