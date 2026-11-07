// rtl_ref_gvu_vlsu_pool_dense.h -- the vbank2 write of max pool with stride 2 / 4 as neo_lite RTL revision 1 builds it (gvu_vlsu.sv:252-283, 404;
// gvu_top.sv:1700-1701), around the frozen vector load / store unit (has/rtl_ref_gvu_vlsu.h through has/gvu_agu_nd_vlsu.h) WITHOUT changing it.
// Pure C++17, no SystemC.
//
// revision 0 (and the frozen port): every W tile of a row writes its OL output lanes (lanes 0..OL-1) into its own vbank2 word.
// revision 1: the results are packed dense, 32 per word. A counter of W tiles (wr_lane_tile_count, 8 bit, back to 0 at num_w_tile - 1) gives
// lane_tile_sel = count[1:0] (s 4) / count[0] (s 2); the lane mask and the data move up by sel * 32 / s lanes; the pooling address of vbank2
// steps at the end of a tile only when the word is full (sel == s - 1) or at the last W tile of the row. A row of results then takes
// ceil(num_w_tile / s) words.
// MEASURED in ModelSim: the delivered counter counts write CYCLES (vbank2_pooling_wr = |i_pooling_valid),
// which is a tile only when a tile is written in one cycle (k == s); with 4x4 s2 a tile is written lane by lane over 16 cycles and the
// delivered unit gets 2,521 of 4,563 outputs wrong. This port counts tiles (the end of a tile = pooling_feeder_last, the signal the address
// already uses): the intended behaviour. A copy of the delivered counting is deferred until the hardware team answers.
//
// DEPENDENCY ON THE FROZEN PORT (check again whenever has/rtl_ref_gvu_vlsu.h changes): the end of a tile is taken from the frozen unit
// through its public const eval() only: in AGU_POOLING its o_vbank2_addr_1 is its pooling address register, which steps by one exactly at
// pooling_feeder_last (rtl_ref_gvu_vlsu.h tick: `if (pc.f.o_tile_last) pool_v2_addr_r_ + 1`) and goes to 0 at o_pooling_last_wr. So
// "eval() before tick() != eval() after tick(), and not the last write" is that edge. Nothing of the frozen unit's state is written here.
// An end of tile in the same cycle as the last write of the run is not seen (the address goes to 0 anyway).
//
// GvuVlsuNeoPool has the interface of GvuVlsuNdPort (bind / eval / tick / reset_pooling / frozen / unit) and forwards everything unchanged.
// The dense write is on only when the unit is built for revision 1 (constructor argument, GvuTopParams::neo_rev; ports do not read the environment)
// and the caller says the cycle is a max pool (eval_pool / tick with maxpool = true; GvuTop: opcode MAXPOOL) with stride 2 or 4.
#ifndef RTL_REF_GVU_VLSU_POOL_DENSE_H
#define RTL_REF_GVU_VLSU_POOL_DENSE_H

#include <array>
#include <cstdint>
#include "has/gvu_agu_nd_vlsu.h"
#include "has/rtl_ref_gvu_pool_feeder_neo.h"

namespace rtl_ref_gvu
{
    // Where a max pool result sits in vbank2 (row of words from the base of the run, lane in the word): one word per W tile and OL lanes
    // (revision 0, the frozen port), or dense, 32 lanes and ceil(Wt / s) words per output row (revision 1, stride 2 / 4).
    struct PoolLayout { unsigned words_per_row, lanes; };
    inline PoolLayout pool_layout(int neo_rev, int s, int Wt, int OL)
    {
        if (neo_rev >= 1 && (s == 2 || s == 4)) return {static_cast<unsigned>((Wt + s - 1) / s), static_cast<unsigned>(GVU_LANES)};
        return {static_cast<unsigned>(Wt), static_cast<unsigned>(OL)};
    }

    class GvuVlsuNeoPool
    {
    public:
        explicit GvuVlsuNeoPool(const ScratchpadParams &p = ScratchpadParams(), int neo_rev = 0) : u_(p), rev1_(neo_rev >= 1), amask_((p.VBANK2_AW() + 2) >= 32 ? 0xFFFFFFFFu : ((1u << (p.VBANK2_AW() + 2)) - 1u)) {}

        void bind(const AguNdCtrl &c, const AguNdDesc &src, const AguNdDesc &dst, const std::array<uint8_t, 5> &sel = {{0, 0, 0, 0, 0}}) { u_.bind(c, src, dst, sel); }

        GvuVlsu::Out eval(const GvuVlsu::In &v) const { return u_.eval(v); }
        // the max pool write path of GvuTop: dense vbank2 address and lane mask when active
        GvuVlsu::Out eval_pool(const GvuVlsu::In &v, bool maxpool) const
        {
            GvuVlsu::Out o = u_.eval(v);
            if (active(v, maxpool))
            {
                o.o_vbank2_addr_1 = addr_;
                o.o_vbank2_wmask_1 = static_cast<uint32_t>((static_cast<uint64_t>(v.i_pooling_valid) << shift(v)) & 0xFFFFFFFFull);
            }
            return o;
        }
        // lanes the pooling result moves up in this cycle (o_pooling_wmask_shift of gvu_vlsu.sv), 0 when not dense
        unsigned pool_wmask_shift(const GvuVlsu::In &v, bool maxpool) const { return active(v, maxpool) ? shift(v) : 0u; }

        void tick(const GvuVlsu::In &v, bool maxpool = false)
        {
            tick_dense(v, maxpool);
            if (rev1_) feeder_clear_.tick(u_.frozen(), v.i_pooling_enable);   // revision 1: no reset of the feeder needed between runs
        }
        unsigned long long feeder_clears() const { return feeder_clear_.clears(); }

    private:
        void tick_dense(const GvuVlsu::In &v, bool maxpool)
        {
            if (!active(v, maxpool)) { u_.tick(v); return; }
            const GvuVlsu::Out pre = u_.frozen().eval(v);
            u_.tick(v);
            const GvuVlsu::Out post = u_.frozen().eval(v);
            const bool last_wr = pre.o_pooling_last_wr;
            const bool tile_end = !last_wr && post.o_vbank2_addr_1 != pre.o_vbank2_addr_1;   // pooling_feeder_last (see the header comment)
            const unsigned s = v.i_pooling_stride & 7u, sel = lane_sel(v);
            const bool cnt_last = static_cast<int>(cnt_) == static_cast<int>(v.i_pooling_num_w_tile & 0xFFu) - 1;   // 8 bit == 8 bit - 1 on 32 bits: 0 never matches
            if (tile_end && (static_cast<int>(sel) == static_cast<int>(s) - 1 || cnt_last)) addr_ = (addr_ + 1u) & amask_;
            if (last_wr) addr_ = 0;
            if (tile_end) cnt_ = cnt_last ? 0u : ((cnt_ + 1u) & 0xFFu);
        }

    public:
        // the RTL resets the counter and the address with rstn only, as the pooling registers of the frozen unit
        void reset_pooling() { u_.reset_pooling(); cnt_ = 0; addr_ = 0; feeder_clear_.reset(); }

        GvuVlsu &frozen() { return u_.frozen(); }
        const GvuVlsu &frozen() const { return u_.frozen(); }
        const GvuVlsuNd &unit() const { return u_.unit(); }

    private:
        bool active(const GvuVlsu::In &v, bool maxpool) const
        {
            const unsigned s = v.i_pooling_stride & 7u;
            return rev1_ && maxpool && (s == 2 || s == 4) && (v.i_vbank2_agu_mode & 3) == AGU_POOLING;
        }
        unsigned lane_sel(const GvuVlsu::In &v) const
        {
            const unsigned s = v.i_pooling_stride & 7u;
            return s == 4 ? (cnt_ & 3u) : (s == 2 ? (cnt_ & 1u) : 0u);
        }
        unsigned shift(const GvuVlsu::In &v) const
        {
            const unsigned s = v.i_pooling_stride & 7u;
            return s == 4 ? lane_sel(v) * (GVU_LANES / 4) : (s == 2 ? lane_sel(v) * (GVU_LANES / 2) : 0u);
        }

        GvuVlsuNdPort u_;
        bool rev1_;
        unsigned amask_;
        unsigned cnt_{0};    // wr_lane_tile_count
        unsigned addr_{0};   // vbank2_pooling_addr (dense)
        PoolFeederNeoClear feeder_clear_;
    };
} // namespace rtl_ref_gvu

#endif
