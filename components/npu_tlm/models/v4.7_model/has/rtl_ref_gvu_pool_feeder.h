// rtl_ref_gvu_pool_feeder.h -- pooling window feeder of the vector unit, ported line by line from the latest hardware RTL drop
// (neo_core/gvu/vlsu/gvu_pooling_feeder.sv), with the two library cells it uses (fifo_v3 with FALL_THROUGH, shift_reg of
// depth 2) written from the behaviour of the pulp common_cells library: HW has not delivered those two files (B8 of the
// review 1051), so their behaviour here is an assumption until the files arrive. Pure C++17, no SystemC.
//
// What the feeder does: every vbank0 read (one row of 32 bytes = 32 columns of one W tile) is pushed, two cycles later,
// into the 32 lane FIFOs (depth 5), byte l into FIFO l. Lane l of the output stream takes the byte of FIFO l + x_cnt[l],
// so each output lane sees the k columns of its window one per cycle, row after row, with first / last flags per lane
// for the accumulate mode of the comparator. Lanes start one cycle after each other (x_cnt[l - 1] == 1), or every k-th
// lane starts on its own when the windows do not overlap. Indices are 5 bits wide as in the RTL: lane + offset wraps at
// 32 (bug B3 of the review: with stride 1 and several W tiles the last k - 1 lanes of a tile take bytes of the same
// tile). Supported: 2x2 s1/s2, 3x3 s1, 4x4 s1/s2/s4, 5x5 s1; the other combinations are TODO in the RTL and give no
// output here either.
#ifndef RTL_REF_GVU_POOL_FEEDER_H
#define RTL_REF_GVU_POOL_FEEDER_H

#include <array>
#include <cstdint>
#include "has/rtl_ref_gvu_scratchpad.h"

namespace rtl_ref_gvu
{
    enum PoolingMode : int { POOLING_2X2 = 0, POOLING_3X3 = 1, POOLING_4X4 = 2, POOLING_5X5 = 3 };   // pooling_mode_t

    // fifo_v3 of common_cells, FALL_THROUGH = 1, DATA_WIDTH = 8, flush and testmode tied to 0. Empty with a push in the same
    // cycle reads as not empty and hands the pushed byte straight to the output; a push into a full FIFO is dropped.
    template <int DEPTH>
    class FifoFt8
    {
    public:
        struct Out { bool full, empty; uint8_t data; };
        Out eval(bool push, uint8_t data_i) const
        {
            Out o;
            o.full = cnt_ == DEPTH;
            o.empty = cnt_ == 0 && !push;
            o.data = (cnt_ == 0 && push) ? data_i : mem_[static_cast<size_t>(rp_)];
            return o;
        }
        void tick(bool push, bool pop, uint8_t data_i)
        {
            const Out o = eval(push, data_i);
            int rp = rp_, wp = wp_, cnt = cnt_;
            if (push && !o.full)
            {
                mem_[static_cast<size_t>(wp_)] = data_i;
                wp = (wp_ == DEPTH - 1) ? 0 : wp_ + 1;
                cnt = cnt_ + 1;
            }
            if (pop && !o.empty)
            {
                rp = (rp == DEPTH - 1) ? 0 : rp + 1;
                cnt = cnt - 1;
            }
            if (push && pop && !o.full && !o.empty) cnt = cnt_;
            if (cnt_ == 0 && push && pop) { cnt = cnt_; rp = rp_; wp = wp_; }   // pass-through: pointers unchanged
            rp_ = rp; wp_ = wp; cnt_ = cnt;
        }
        void reset() { rp_ = wp_ = cnt_ = 0; mem_.fill(0); }

    private:
        std::array<uint8_t, DEPTH> mem_{};
        int rp_{0}, wp_{0}, cnt_{0};
    };

    class GvuPoolingFeeder
    {
    public:
        static constexpr int NUM_LANE = 32, MAX_POOLING_WINDOW = 5;
        struct In
        {
            bool i_feeder_enable{false};
            int i_pooling_mode{POOLING_2X2};
            unsigned i_window_size{2};       // 3 bits
            unsigned i_pooling_stride{1};    // 3 bits
            SpBytes i_sram_rdata;            // vbank0 read data of this cycle (32 bytes)
        };
        struct Out
        {
            bool o_sram_rd{false};
            uint32_t o_feeder_valid{0};
            std::array<uint8_t, NUM_LANE> o_feeder_data{};
            uint32_t o_feeder_first{0};      // "fisrt" in the RTL port name
            uint32_t o_feeder_last{0};
            bool o_tile_last{false};
            bool o_all_empty{false};
        };

        Out eval(const In &i) const { Comb c; comb(i, c); return c.o; }

        void tick(const In &i)
        {
            Comb c;
            comb(i, c);
            const unsigned ws = i.i_window_size & 7u;
            // first_load_cnt (3 bits)
            if (c.all_empty && i.i_feeder_enable) first_load_cnt_ = (first_load_cnt_ + 1) & 7u;
            else if (first_load_cnt_ != 0 && first_load_cnt_ != ws) first_load_cnt_ = (first_load_cnt_ + 1) & 7u;
            // lane FIFOs: push the delayed read into every lane, pop the lanes that start a window column
            for (int l = 0; l < NUM_LANE; l++)
            {
                const uint8_t d = l < static_cast<int>(i.i_sram_rdata.size()) ? i.i_sram_rdata[static_cast<size_t>(l)] : 0;
                fifo_[static_cast<size_t>(l)].tick(sram_rd_d_[1], c.pop[l], d);
            }
            // lane window counters
            for (int l = 0; l < NUM_LANE; l++)
            {
                unsigned &x = x_cnt_[static_cast<size_t>(l)], &y = y_cnt_[static_cast<size_t>(l)];
                const bool st = (c.x_start >> l) & 1u;
                if (st) x = (x + 1) & 7u;
                else if (x != 0) x = (x == ((ws - 1) & 7u)) ? 0u : ((x + 1) & 7u);
                if (st) y = (y == ((ws - 1) & 7u)) ? 0u : ((y + 1) & 7u);
            }
            // shift_reg Depth 2 on o_sram_rd
            sram_rd_d_[1] = sram_rd_d_[0];
            sram_rd_d_[0] = c.o.o_sram_rd;
        }

        void reset()
        {
            for (auto &f : fifo_) f.reset();
            first_load_cnt_ = 0;
            sram_rd_d_ = {false, false};
            x_cnt_.fill(0);
            y_cnt_.fill(0);
        }

    private:
        std::array<FifoFt8<MAX_POOLING_WINDOW>, NUM_LANE> fifo_{};
        unsigned first_load_cnt_{0};
        std::array<bool, 2> sram_rd_d_{false, false};
        std::array<unsigned, NUM_LANE> x_cnt_{}, y_cnt_{};

        struct Comb
        {
            Out o;
            uint32_t empty{0}, x_start{0};
            std::array<bool, NUM_LANE> pop{};
            std::array<uint8_t, NUM_LANE> fifo_out{};
            bool all_empty{false};
        };

        void comb(const In &i, Comb &c) const
        {
            const unsigned ws = i.i_window_size & 7u, stride = i.i_pooling_stride & 7u;
            const bool push = sram_rd_d_[1];
            for (int l = 0; l < NUM_LANE; l++)
            {
                const uint8_t d = l < static_cast<int>(i.i_sram_rdata.size()) ? i.i_sram_rdata[static_cast<size_t>(l)] : 0;
                const auto f = fifo_[static_cast<size_t>(l)].eval(push, d);
                if (f.empty) c.empty |= 1u << l;
                c.fifo_out[static_cast<size_t>(l)] = f.data;
            }
            c.all_empty = c.empty == 0xFFFFFFFFu;
            c.o.o_all_empty = c.all_empty;
            const bool overlap = stride < ws;
            auto xc = [&](int l) { return x_cnt_[static_cast<size_t>(l & 31)]; };
            auto yc = [&](int l) { return y_cnt_[static_cast<size_t>(l & 31)]; };
            auto empty = [&](int l) { return ((c.empty >> (l & 31)) & 1u) != 0; };
            // pooling_lanes_cnt_start_ctrl
            for (int l = 0; l < NUM_LANE; l++)
            {
                bool st;
                if (overlap) st = (l == 0) ? (!empty(0) && xc(0) == 0) : (xc(l - 1) == 1);
                else
                {
                    const int m = i.i_pooling_mode == POOLING_5X5 ? 5 : i.i_pooling_mode == POOLING_4X4 ? 4 : i.i_pooling_mode == POOLING_3X3 ? 3 : 2;
                    st = (l % m == 0) ? (!empty(l) && xc(l) == 0) : (xc(l - 1) == 1);
                }
                if (st) c.x_start |= 1u << l;
            }
            auto xs = [&](int l) { return ((c.x_start >> (l & 31)) & 1u) != 0; };
            // sram_rd_ctrl
            bool rd = false;
            if (i.i_feeder_enable)
            {
                if (overlap) rd = xs(NUM_LANE - 2);
                else
                {
                    if (ws == 2) rd = xs(0);
                    if (ws == 4) rd = xs(1);
                }
                if (first_load_cnt_ != ws) rd = true;
            }
            c.o.o_sram_rd = rd;
            // pooling_lanes_ctrl: pop
            for (int l = 0; l < NUM_LANE; l++) c.pop[static_cast<size_t>(l)] = !empty(l) && xs(l);
            // output_lane_comb: lanes [0, n), window base lane = step * lane, last lane = base + k - 1, tile_last on lane n - 1
            int n = 0, step = 1, last_off = 0;
            const int k = static_cast<int>(ws);
            switch (i.i_pooling_mode)
            {
            case POOLING_2X2: if (stride == 1) { n = 32; step = 1; } else if (stride == 2) { n = 16; step = 2; } break;
            case POOLING_3X3: if (stride == 1) { n = 32; step = 1; } break;
            case POOLING_4X4: if (stride == 1) { n = 32; step = 1; } else if (stride == 2) { n = 16; step = 2; } else if (stride == 4) { n = 8; step = 4; } break;
            case POOLING_5X5: if (stride == 1) { n = 32; step = 1; } break;
            default: break;
            }
            last_off = (i.i_pooling_mode == POOLING_5X5 ? 5 : i.i_pooling_mode == POOLING_4X4 ? 4 : i.i_pooling_mode == POOLING_3X3 ? 3 : 2) - 1;
            for (int l = 0; l < n; l++)
            {
                const int base = l * step;
                const int lane_idx = (base + static_cast<int>(xc(base))) & 31;   // 5-bit index, wraps (B3)
                const int last_idx = (base + last_off) & 31;
                const bool v = xs(lane_idx);
                if (v) c.o.o_feeder_valid |= 1u << l;
                c.o.o_feeder_data[static_cast<size_t>(l)] = c.fifo_out[static_cast<size_t>(lane_idx)];
                if (xs(base) && yc(base) == 0) c.o.o_feeder_first |= 1u << l;
                if (v && xs(last_idx) && yc(last_idx) == ((static_cast<unsigned>(k) - 1u) & 7u)) c.o.o_feeder_last |= 1u << l;
            }
            if (n > 0) c.o.o_tile_last = ((c.o.o_feeder_last >> (n - 1)) & 1u) != 0;
        }
    };
} // namespace rtl_ref_gvu

#endif // RTL_REF_GVU_POOL_FEEDER_H
