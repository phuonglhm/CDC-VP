// rtl_ref_dfc_tile_loop_neo.h -- C++ port of the tile pointer generator of the data flow controller (neo_lite RTL revision 1 and revision 0, the file is identical in both:
// rtl/src/df_controller/neo_dma_pointer_generator.sv, SAURIA side, pointer_sel = 0). One advance() = one clock with `advance` high.
// Pure C++ (no SystemC); nothing in the model calls it: tools/has/tb_dfc_tile_loop_neo.cpp compares it with has::TileIter.
//
// What the RTL does: four counters x, y, c, k (16 bit, limits x_lim .. k_lim) walk the tile grid. loop_order picks the nesting (fastest first):
//   0: x, y, c, k     1: c, k, (x, y)     2 and 3: k, c, (x, y)          (x always runs before y in the spatial pair)
// Per counter a pointer (counter * step, 32 bit) feeds the DMA of the ifmaps (x, y, c), psums (x, y, k) and weights (c, k); a *_change flag says which of
// the three operands a new tile needs again (weights are re-fetched only when c or k changes, and only if that limit is not 0).
#ifndef HAS_RTL_REF_DFC_TILE_LOOP_NEO_H
#define HAS_RTL_REF_DFC_TILE_LOOP_NEO_H

#include <cstdint>

namespace has
{
    namespace rtl_ref_dfc
    {
        struct TileLoopParams
        {
            uint16_t x_lim{0}, y_lim{0}, c_lim{0}, k_lim{0};        // tiles - 1 along each loop
            uint32_t psums_x{0}, psums_y{0}, psums_k{0};            // pointer steps (TilePointers.psums)
            uint32_t ifmaps_x{0}, ifmaps_y{0}, ifmaps_c{0};
            uint32_t weights_k{0}, weights_c{0};
            unsigned loop_order{0};                                 // 0..3
        };

        struct TileLoopOut
        {
            uint32_t ifmap_x{0}, ifmap_y{0}, ifmap_c{0}, psums_x{0}, psums_y{0}, psums_k{0}, weights_c{0}, weights_k{0};
            bool ifmaps_change{true}, psums_change{true}, weights_change{true};
        };

        class TileLoopNeo
        {
        public:
            explicit TileLoopNeo(const TileLoopParams &p) : p_(p) { reset(); }
            // rst / pointer_reset: counters to 0, all three operands "changed" (the first tile loads everything)
            void reset() { x_ = y_ = c_ = k_ = 0; o_ = TileLoopOut{}; }
            const TileLoopOut &out() const { return o_; }
            unsigned x() const { return x_; }
            unsigned y() const { return y_; }
            unsigned c() const { return c_; }
            unsigned k() const { return k_; }
            // last_iter, c_tile_last, k_tile_last (combinational on the counters)
            bool last_iter() const { return ov_x() && ov_y() && ov_c() && ov_k(); }
            bool c_tile_last() const { return ov_c(); }
            bool k_tile_last() const { return ov_k(); }

            void advance()
            {
                bool spatial, c_cond, k_cond;
                if (p_.loop_order == 0) { spatial = true; c_cond = ov_x() && ov_y(); k_cond = c_cond && ov_c(); }
                else if (p_.loop_order == 1) { c_cond = true; k_cond = ov_c(); spatial = k_cond && ov_k(); }
                else { k_cond = true; c_cond = ov_k(); spatial = c_cond && ov_c(); }
                const bool ox = ov_x(), oy = ov_y(), oc = ov_c(), ok = ov_k();   // all conditions are read before any register changes
                o_.ifmaps_change = o_.psums_change = o_.weights_change = false;
                if (spatial)
                {
                    if (p_.x_lim != 0 || p_.y_lim != 0) { o_.ifmaps_change = true; o_.psums_change = true; }
                    if (ox) { x_ = 0; o_.ifmap_x = 0; o_.psums_x = 0; }
                    else { x_++; o_.ifmap_x += p_.ifmaps_x; o_.psums_x += p_.psums_x; }
                    if (ox)
                    {
                        if (oy) { y_ = 0; o_.ifmap_y = 0; o_.psums_y = 0; }
                        else { y_++; o_.ifmap_y += p_.ifmaps_y; o_.psums_y += p_.psums_y; }
                    }
                }
                if (c_cond)
                {
                    if (p_.c_lim != 0) { o_.ifmaps_change = true; o_.weights_change = true; }
                    if (oc) { c_ = 0; o_.ifmap_c = 0; o_.weights_c = 0; }
                    else { c_++; o_.ifmap_c += p_.ifmaps_c; o_.weights_c += p_.weights_c; }
                }
                if (k_cond)
                {
                    if (p_.k_lim != 0) { o_.psums_change = true; o_.weights_change = true; }
                    if (ok) { k_ = 0; o_.psums_k = 0; o_.weights_k = 0; }
                    else { k_++; o_.psums_k += p_.psums_k; o_.weights_k += p_.weights_k; }
                }
            }

        private:
            bool ov_x() const { return x_ == p_.x_lim; }
            bool ov_y() const { return y_ == p_.y_lim; }
            bool ov_c() const { return c_ == p_.c_lim; }
            bool ov_k() const { return k_ == p_.k_lim; }
            TileLoopParams p_;
            unsigned x_{0}, y_{0}, c_{0}, k_{0};
            TileLoopOut o_;
        };
    } // namespace rtl_ref_dfc
} // namespace has

#endif // HAS_RTL_REF_DFC_TILE_LOOP_NEO_H
