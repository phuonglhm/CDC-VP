// rtl_ref_gvu_vlsu.h -- vector load / store unit of the vector unit, ported line by line from the RTL
// (neo_core/gvu/gvu_vlsu.sv). Pure C++17, no SystemC. The structural model of the same unit in the instruction path is
// has/gvu_lsu.h; nothing here is used by it.
// The unit turns the read / write requests of the pipeline controller into the enable, write-enable, address and mask
// inputs of the Scratchpad ports (has/rtl_ref_gvu_scratchpad.h). It holds five address registers: one per sbank port,
// one for vbank0, one for vbank1 and one for vbank2 (the ports of vbank1 / vbank2 share the register of their bank and
// take its low bits on their own address width). At every clock edge:
//   no request on the register: it follows the start address input;
//   request, AGU_INCR:  it counts up by one on its own width (it wraps, no end-of-bank check);
//   request, AGU_FIXED: it follows the start address input;
//   request, AGU_POOLING (code 2, AGU_WRAPPED before the latest hardware RTL drop) and the unused fourth code: it keeps its value;
//   i_agu_addr_reset: every register takes its start address input, whatever the requests are.
// The address output is the register, so the first access of a burst uses the start address that was present one clock
// before the request; in AGU_INCR a start address change during a burst has no effect.
// The request of vbank1 / vbank2 is rd | wr whatever the port select is: with i_vbank1_port_sel = 3 no port is enabled
// and the register still advances.
// Port enables: sbank and vbank0 are enabled by their rd | wr requests; vbank1 and vbank2 have one rd / wr pair each and
// i_vbank1_port_sel (0 / 1 / 2) and i_vbank2_port_sel route it to one port. The registers have no reset (zero here
// until first loaded).
// Write masks: the lane mask (one bit per lane) is expanded to 4 bits per lane for the v32int32 ports and 2 bits per lane
// for the v32int16 port; the v32int8 ports take it unchanged.
// Pooling (latest hardware RTL drop, MAXPOOL / AVGPOOL): the pooling feeder (has/rtl_ref_gvu_pool_feeder.h) asks for vbank0 rows;
// their address is c_offset + h_offset + w_tile + window_row * num_w_tile, counters from inner to outer: window row (k),
// W tile, output row (OH), channel (C); h_offset steps by i_pooling_h_move per output row, c_offset by i_pooling_c_move
// per channel. A feeder read also enables vbank0 (and counts as a request for its address register). Results come back
// on i_pooling_valid (one bit per lane, from the comparator) and are written to vbank2 port 1 (v32int8) at a pooling
// address that steps at the feeder's tile_last and returns to 0 at o_pooling_last_wr (all lane FIFOs empty after the
// last window row was read). vbank0 / vbank2 take the pooling address when their AGU mode is AGU_POOLING.
#ifndef RTL_REF_GVU_VLSU_H
#define RTL_REF_GVU_VLSU_H

#include <array>
#include <cstdint>
#include "has/rtl_ref_gvu_scratchpad.h"
#include "has/rtl_ref_gvu_pool_feeder.h"

namespace rtl_ref_gvu
{
    enum AguMode : int { AGU_FIXED = 0, AGU_INCR = 1, AGU_POOLING = 2, AGU_WRAPPED = AGU_POOLING };   // agu_mode_t (2 bits)

    class GvuVlsu
    {
    public:
        struct In
        {
            bool i_agu_addr_reset{false};
            bool i_sbank_rd_0{false}, i_sbank_wr_0{false}; int i_sbank_agu_mode_0{AGU_INCR}; unsigned i_sbank_start_addr_0{0}, i_sbank_wmask_0{0};
            bool i_sbank_rd_1{false}, i_sbank_wr_1{false}; int i_sbank_agu_mode_1{AGU_INCR}; unsigned i_sbank_start_addr_1{0}; bool i_sbank_wmask_1{false};
            bool i_vbank0_rd{false}, i_vbank0_wr{false}; int i_vbank0_agu_mode{AGU_INCR}; unsigned i_vbank0_start_addr{0}; uint32_t i_vbank0_lane_wmask{0};
            unsigned i_vbank1_port_sel{0}; bool i_vbank1_rd{false}, i_vbank1_wr{false}; int i_vbank1_agu_mode{AGU_INCR};
            unsigned i_vbank1_start_addr{0}; uint32_t i_vbank1_lane_wmask{0};          // VBANK1_AW + 2 bits (the int8 port width)
            bool i_vbank2_port_sel{false}, i_vbank2_rd{false}, i_vbank2_wr{false}; int i_vbank2_agu_mode{AGU_INCR};
            unsigned i_vbank2_start_addr{0}; uint32_t i_vbank2_lane_wmask{0};          // VBANK2_AW + 2 bits (the int8 port width)
            // pooling (latest hardware RTL drop)
            bool i_pooling_enable{false};
            int i_pooling_mode{POOLING_2X2};
            unsigned i_pooling_stride{1};          // 3 bits
            unsigned i_pooling_num_w_tile{1};      // 8 bits, ceil(W / 32)
            unsigned i_pooling_c{1};               // 8 bits
            unsigned i_pooling_oh{1};              // 8 bits
            unsigned i_pooling_c_move{0};          // VBANK0_AW bits
            unsigned i_pooling_h_move{0};          // VBANK0_AW bits
            SpBytes i_pooling_rdata;               // vbank0 read data of this cycle
            uint32_t i_pooling_valid{0};           // comparator lane valid (pooling results of this cycle)
        };
        struct Out
        {
            bool o_sbank_en_0, o_sbank_wen_0; unsigned o_sbank_addr_0, o_sbank_wmask_0;
            bool o_sbank_en_1, o_sbank_wen_1; unsigned o_sbank_addr_1; bool o_sbank_wmask_1;
            bool o_vbank0_en, o_vbank0_wen; unsigned o_vbank0_addr; uint32_t o_vbank0_wmask;
            unsigned o_vbank1_port_sel;
            bool o_vbank1_en_0, o_vbank1_wen_0; unsigned o_vbank1_addr_0; SpBytes o_vbank1_wmask_0;   // 4 bits per lane
            bool o_vbank1_en_1, o_vbank1_wen_1; unsigned o_vbank1_addr_1; SpBytes o_vbank1_wmask_1;   // 2 bits per lane
            bool o_vbank1_en_2, o_vbank1_wen_2; unsigned o_vbank1_addr_2; uint32_t o_vbank1_wmask_2;
            bool o_vbank2_port_sel;
            bool o_vbank2_en_0, o_vbank2_wen_0; unsigned o_vbank2_addr_0; SpBytes o_vbank2_wmask_0;   // 4 bits per lane
            bool o_vbank2_en_1, o_vbank2_wen_1; unsigned o_vbank2_addr_1; uint32_t o_vbank2_wmask_1;
            // pooling window stream (to the comparator) and end of the pooling run
            uint32_t o_pooling_window_valid{0};
            std::array<uint8_t, GvuPoolingFeeder::NUM_LANE> o_pooling_window_data{};
            uint32_t o_feeder_first{0}, o_feeder_last{0};
            bool o_pooling_last_wr{false};
        };
        explicit GvuVlsu(const ScratchpadParams &p = ScratchpadParams()) : P_(p) {}

        Out eval(const In &i) const
        {
            Out o;
            const PoolComb pc = pool_comb(i);
            o.o_sbank_en_0 = i.i_sbank_rd_0 || i.i_sbank_wr_0; o.o_sbank_wen_0 = i.i_sbank_wr_0; o.o_sbank_addr_0 = sbank_addr_0_r_; o.o_sbank_wmask_0 = i.i_sbank_wmask_0 & 0xFu;
            o.o_sbank_en_1 = i.i_sbank_rd_1 || i.i_sbank_wr_1; o.o_sbank_wen_1 = i.i_sbank_wr_1; o.o_sbank_addr_1 = sbank_addr_1_r_; o.o_sbank_wmask_1 = i.i_sbank_wmask_1;
            o.o_vbank0_en = i.i_vbank0_rd || i.i_vbank0_wr || pc.rd; o.o_vbank0_wen = i.i_vbank0_wr;
            o.o_vbank0_addr = (i.i_vbank0_agu_mode & 3) == AGU_POOLING ? pc.v0_addr : vbank0_addr_r_; o.o_vbank0_wmask = i.i_vbank0_lane_wmask;
            const unsigned ps1 = i.i_vbank1_port_sel & 3u;
            const bool rw1 = i.i_vbank1_rd || i.i_vbank1_wr;
            o.o_vbank1_port_sel = ps1;
            o.o_vbank1_en_0 = rw1 && ps1 == 0; o.o_vbank1_wen_0 = i.i_vbank1_wr && ps1 == 0; o.o_vbank1_addr_0 = vbank1_addr_r_ & mask(P_.VBANK1_AW()); o.o_vbank1_wmask_0 = expand(i.i_vbank1_lane_wmask, 4);
            o.o_vbank1_en_1 = rw1 && ps1 == 1; o.o_vbank1_wen_1 = i.i_vbank1_wr && ps1 == 1; o.o_vbank1_addr_1 = vbank1_addr_r_ & mask(P_.VBANK1_AW() + 1); o.o_vbank1_wmask_1 = expand(i.i_vbank1_lane_wmask, 2);
            o.o_vbank1_en_2 = rw1 && ps1 == 2; o.o_vbank1_wen_2 = i.i_vbank1_wr && ps1 == 2; o.o_vbank1_addr_2 = vbank1_addr_r_; o.o_vbank1_wmask_2 = i.i_vbank1_lane_wmask;
            const bool rw2 = i.i_vbank2_rd || i.i_vbank2_wr;
            const bool pool_wr = i.i_pooling_valid != 0;
            const bool pool2 = (i.i_vbank2_agu_mode & 3) == AGU_POOLING;
            o.o_vbank2_port_sel = i.i_vbank2_port_sel;
            o.o_vbank2_en_0 = rw2 && !i.i_vbank2_port_sel; o.o_vbank2_wen_0 = i.i_vbank2_wr && !i.i_vbank2_port_sel; o.o_vbank2_addr_0 = vbank2_addr_r_ & mask(P_.VBANK2_AW()); o.o_vbank2_wmask_0 = expand(i.i_vbank2_lane_wmask, 4);
            o.o_vbank2_en_1 = (rw2 || pool_wr) && i.i_vbank2_port_sel; o.o_vbank2_wen_1 = (i.i_vbank2_wr || pool_wr) && i.i_vbank2_port_sel;
            o.o_vbank2_addr_1 = pool2 ? pool_v2_addr_r_ : vbank2_addr_r_; o.o_vbank2_wmask_1 = pool2 ? i.i_pooling_valid : i.i_vbank2_lane_wmask;
            o.o_pooling_window_valid = pc.f.o_feeder_valid;
            o.o_pooling_window_data = pc.f.o_feeder_data;
            o.o_feeder_first = pc.f.o_feeder_first;
            o.o_feeder_last = pc.f.o_feeder_last;
            o.o_pooling_last_wr = pc.last_wr;
            return o;
        }
        void tick(const In &i)
        {
            const PoolComb pc = pool_comb(i);
            step(sbank_addr_0_r_, i.i_sbank_rd_0 || i.i_sbank_wr_0, i.i_sbank_agu_mode_0, i.i_sbank_start_addr_0, P_.SBANK_AW());
            step(sbank_addr_1_r_, i.i_sbank_rd_1 || i.i_sbank_wr_1, i.i_sbank_agu_mode_1, i.i_sbank_start_addr_1, P_.SBANK_AW() + 2);
            step(vbank0_addr_r_, i.i_vbank0_rd || i.i_vbank0_wr || pc.rd, i.i_vbank0_agu_mode, i.i_vbank0_start_addr, P_.VBANK0_AW());
            step(vbank1_addr_r_, i.i_vbank1_rd || i.i_vbank1_wr, i.i_vbank1_agu_mode, i.i_vbank1_start_addr, P_.VBANK1_AW() + 2);
            step(vbank2_addr_r_, i.i_vbank2_rd || i.i_vbank2_wr, i.i_vbank2_agu_mode, i.i_vbank2_start_addr, P_.VBANK2_AW() + 2);
            if (i.i_agu_addr_reset)
            {
                sbank_addr_0_r_ = i.i_sbank_start_addr_0 & mask(P_.SBANK_AW());
                sbank_addr_1_r_ = i.i_sbank_start_addr_1 & mask(P_.SBANK_AW() + 2);
                vbank0_addr_r_ = i.i_vbank0_start_addr & mask(P_.VBANK0_AW());
                vbank1_addr_r_ = i.i_vbank1_start_addr & mask(P_.VBANK1_AW() + 2);
                vbank2_addr_r_ = i.i_vbank2_start_addr & mask(P_.VBANK2_AW() + 2);
            }
            // pooling: tile_last_r, tiling loop counters and offsets, vbank2 pooling address, feeder
            if (pc.tile_last) pool_tile_last_r_ = true;
            else if (pc.last_wr) pool_tile_last_r_ = false;
            const unsigned m8 = 0xFFu, m0 = mask(P_.VBANK0_AW());
            const unsigned c_cnt = pool_c_cnt_, oh_cnt = pool_oh_cnt_, w_cnt = pool_w_cnt_, win_cnt = pool_win_cnt_;
            if (pc.rd) pool_win_cnt_ = pc.win_last ? 0u : ((win_cnt + 1u) & 7u);
            if (pc.win_last) pool_w_cnt_ = pc.w_last ? 0u : ((w_cnt + 1u) & m8);
            if (pc.w_last && pc.win_last) pool_oh_cnt_ = pc.oh_last ? 0u : ((oh_cnt + 1u) & m8);
            if (pc.oh_last && pc.w_last && pc.win_last) pool_c_cnt_ = pc.c_last ? 0u : ((c_cnt + 1u) & m8);
            const unsigned h_off = pool_h_off_, c_off = pool_c_off_;
            if (pc.w_last && pc.win_last) pool_h_off_ = pc.oh_last ? 0u : ((h_off + i.i_pooling_h_move) & m0);
            if (pc.oh_last && pc.w_last && pc.win_last) pool_c_off_ = pc.c_last ? 0u : ((c_off + i.i_pooling_c_move) & m0);
            const unsigned mv2 = mask(P_.VBANK2_AW() + 2);
            if (pc.f.o_tile_last) pool_v2_addr_r_ = (pool_v2_addr_r_ + 1u) & mv2;
            if (pc.last_wr) pool_v2_addr_r_ = 0;
            feeder_.tick(pc.fin);
        }
        // Reset of the registers that have one in the RTL (the pooling part; the AGU address registers have none).
        void reset_pooling()
        {
            pool_win_cnt_ = pool_w_cnt_ = pool_oh_cnt_ = pool_c_cnt_ = 0;
            pool_c_off_ = pool_h_off_ = 0;
            pool_tile_last_r_ = false;
            pool_v2_addr_r_ = 0;
            feeder_.reset();
        }

    private:
        ScratchpadParams P_;
        unsigned sbank_addr_0_r_{0}, sbank_addr_1_r_{0}, vbank0_addr_r_{0}, vbank1_addr_r_{0}, vbank2_addr_r_{0};
        // pooling state
        unsigned pool_win_cnt_{0}, pool_w_cnt_{0}, pool_oh_cnt_{0}, pool_c_cnt_{0}, pool_c_off_{0}, pool_h_off_{0}, pool_v2_addr_r_{0};
        bool pool_tile_last_r_{false};
        GvuPoolingFeeder feeder_;

        struct PoolComb
        {
            GvuPoolingFeeder::In fin;
            GvuPoolingFeeder::Out f;
            bool rd{false}, win_last{false}, w_last{false}, oh_last{false}, c_last{false}, tile_last{false}, last_wr{false};
            unsigned v0_addr{0};
        };
        PoolComb pool_comb(const In &i) const
        {
            PoolComb pc;
            const unsigned ws = i.i_pooling_mode == POOLING_5X5 ? 5u : i.i_pooling_mode == POOLING_4X4 ? 4u : i.i_pooling_mode == POOLING_3X3 ? 3u : 2u;
            pc.fin.i_feeder_enable = i.i_pooling_enable && !pool_tile_last_r_;
            pc.fin.i_pooling_mode = i.i_pooling_mode;
            pc.fin.i_window_size = ws;
            pc.fin.i_pooling_stride = i.i_pooling_stride & 7u;
            pc.fin.i_sram_rdata = i.i_pooling_rdata;
            pc.f = feeder_.eval(pc.fin);
            pc.rd = pc.f.o_sram_rd;
            const unsigned m8 = 0xFFu;
            pc.win_last = pool_win_cnt_ == ws - 1u && pc.rd;
            // 8-bit counter == 8-bit input - 1 evaluated on 32 bits as in the RTL: an input of 0 never matches
            auto last = [](unsigned cnt, unsigned n8) { return static_cast<long>(cnt) == static_cast<long>(n8 & 0xFFu) - 1; };
            pc.w_last = last(pool_w_cnt_, i.i_pooling_num_w_tile);
            pc.oh_last = last(pool_oh_cnt_, i.i_pooling_oh);
            pc.c_last = last(pool_c_cnt_, i.i_pooling_c);
            pc.tile_last = pc.c_last && pc.oh_last && pc.w_last && pc.win_last;
            pc.last_wr = pc.f.o_all_empty && pool_tile_last_r_;
            pc.v0_addr = (pool_c_off_ + pool_h_off_ + pool_w_cnt_ + pool_win_cnt_ * (i.i_pooling_num_w_tile & m8)) & mask(P_.VBANK0_AW());
            return pc;
        }
        static unsigned mask(int aw) { return (1u << aw) - 1u; }
        static void step(unsigned &r, bool req, int agu_mode, unsigned start, int aw)
        {
            const unsigned m = mask(aw);
            if (!req) r = start & m;
            else if ((agu_mode & 3) == AGU_INCR) r = (r + 1u) & m;
            else if ((agu_mode & 3) == AGU_FIXED) r = start & m;
        }
        // {n{lane_wmask[i]}} << n * i for every lane
        SpBytes expand(uint32_t lane_wmask, int n) const
        {
            SpBytes r(static_cast<size_t>(n * P_.NUM_LANE / 8), 0);
            for (int l = 0; l < P_.NUM_LANE && l < 32; l++)
                if ((lane_wmask >> l) & 1u)
                    for (int b = 0; b < n; b++) { const int k = n * l + b; r[static_cast<size_t>(k / 8)] = static_cast<uint8_t>(r[static_cast<size_t>(k / 8)] | (1u << (k % 8))); }
            return r;
        }
    };
} // namespace rtl_ref_gvu

#endif // RTL_REF_GVU_VLSU_H
