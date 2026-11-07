// rtl_ref_gvu_pool_feeder_neo.h -- the pooling feeder of neo_lite RTL revision 1 needs no reset between runs (gvu_pooling_feeder.sv:73-81
// clears first_load_cnt in every cycle with ~i_feeder_enable), around the frozen feeder (inside has/rtl_ref_gvu_vlsu.h) WITHOUT changing it.
// Pure C++17, no SystemC.
//
// The frozen feeder (revision 0) keeps first_load_cnt from one run to the next, so the model resets the unit before every run (QD-09). With
// neo_rev 1 the frozen unit is reset through its public reset_pooling() in the first idle cycle (pooling enable low) after a run that used
// the pooling path. That is the state revision 1 has at the start of the next run: after the last write of a run the tiling counters and the
// vbank2 pooling address of the VLSU are back at 0, tile_last_r is low and the feeder FIFOs are empty (o_pooling_last_wr needs them empty),
// so the only register a reset changes is first_load_cnt, which the RTL clears while the enable is low.
// Not modelled: the RTL also clears first_load_cnt inside a run once the last tile is read (feeder enable low while the FIFOs drain); on
// revision 1 the single-run traces of the frozen port equal the RTL on 8 / 8 stride-1 sets, so that clear changes nothing visible.
// DEPENDENCY ON THE FROZEN PORT (check again whenever has/rtl_ref_gvu_vlsu.h changes): reset_pooling() resets exactly the pooling registers
// that have a reset in the RTL (counters, offsets, tile_last_r, vbank2 pooling address, feeder).
#ifndef RTL_REF_GVU_POOL_FEEDER_NEO_H
#define RTL_REF_GVU_POOL_FEEDER_NEO_H

namespace rtl_ref_gvu
{
    class PoolFeederNeoClear
    {
    public:
        // once per clock edge, after the frozen unit's tick; Unit has reset_pooling() (GvuVlsu)
        template <class Unit>
        void tick(Unit &frozen, bool pooling_enable)
        {
            if (pooling_enable) { used_ = true; return; }
            if (!used_) return;
            frozen.reset_pooling();
            used_ = false;
            clears_++;
        }
        void reset() { used_ = false; }
        unsigned long long clears() const { return clears_; }

    private:
        bool used_{false};
        unsigned long long clears_{0};
    };
} // namespace rtl_ref_gvu

#endif
