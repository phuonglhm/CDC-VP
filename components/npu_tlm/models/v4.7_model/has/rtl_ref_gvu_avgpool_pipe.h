// rtl_ref_gvu_avgpool_pipe.h -- the average pool tail of the vector unit (AVG_POOL, MAS "Avg_Pool" drawing).
//
// What it models: every lane keeps a 16-bit sum of its window (two's complement wrap, Adder#1 ADD_16); the completed sum goes
// through Mul#1 (MA_32X16: Avg_Scale x sum), the v32int64 register #1 and Round-Shift#1 (Avg_Shift, round mode, narrow to int8).
// Depth of the tail from the cycle of a lane's last window element to the cycle of the vbank2 write: kAvgTailDepth cycles
//   sum register 1 + Mul#1 MA_32X16 2 + v32int64 register 1 + Round-Shift#1 1 (S2_PIPE = 0)  = 5.
// This is the depth of the RTL patch for AVG_POOL (ModelSim, gvu_top.sv with AVG_TAIL_D = 5; the delivered RTL has no data path).
//
//  * GvuAvgTail   : the tail alone, used by GvuTop (has/rtl_ref_gvu_top.h, POOLING_AVG): push the sums of one row of 32 lanes with
//                   their write request, get the int8 row and the request back kAvgTailDepth cycles later. Scale, shift, round mode
//                   and narrow mode are those of the cycle of the push and must stay constant during a run (the RTL reads the
//                   registers in the stage cycles).
//  * GvuAvgPoolPipe: a window-major runner for any (k, s): rows of windows in, int8 rows out, with the cycle count of the run.
//
// Uses only the units of has/rtl_ref_gvu_fu.h. Pure C++17, no SystemC.
#ifndef RTL_REF_GVU_AVGPOOL_PIPE_H
#define RTL_REF_GVU_AVGPOOL_PIPE_H

#include <array>
#include <cstdint>
#include <deque>
#include <vector>
#include "has/rtl_ref_gvu_fu.h"

namespace rtl_ref_gvu
{
    static constexpr int kAvgTailDepth = 5;

    struct AvgCfg
    {
        int32_t scale{1};
        uint8_t shift{0};                          // 6 bits
        RoundMode round{RND_NEAR_UP};
        NarrowMode narrow{NARROW_SATURATE};
    };

    // One lane: 16-bit wrapped sum -> Mul#1 MA_32X16 (scale x sum) -> Round-Shift#1 (64 bit, narrow to 8 bit).
    inline int8_t avg_lane(int16_t sum16, const AvgCfg &c)
    {
        const uint64_t prod = vmul32x32_lane(MA_32X16, static_cast<uint32_t>(c.scale), static_cast<uint32_t>(static_cast<int32_t>(sum16)));
        const uint32_t r = vroundshift64_lane(prod, c.shift & 0x3Fu, c.round, c.narrow, NW_8).out_data;
        return static_cast<int8_t>(r & 0xFFu);
    }

    // The tail of one GvuTop: a fixed delay of kAvgTailDepth cycles around the arithmetic.
    class GvuAvgTail
    {
    public:
        struct Beat
        {
            bool valid{false};
            std::array<int8_t, GVU_LANES> data{};
            uint32_t mask{0};          // lanes written
            uint32_t addr{0};          // vbank2 port 1 row
        };
        GvuAvgTail() { reset(); }
        void reset() { line_.assign(kAvgTailDepth, Beat{}); }
        // Beat that leaves the tail in this cycle (pushed kAvgTailDepth cycles ago).
        const Beat &out() const { return line_.front(); }
        // Clock edge: shift the line and enter the beat of this cycle (b.valid false = nothing).
        void tick(bool valid, const std::array<int16_t, GVU_LANES> &sum, uint32_t mask, uint32_t addr, const AvgCfg &c)
        {
            Beat b;
            if (valid)
            {
                b.valid = true;
                b.mask = mask;
                b.addr = addr;
                for (int l = 0; l < GVU_LANES; l++)
                    if (mask >> l & 1u) b.data[static_cast<size_t>(l)] = avg_lane(sum[static_cast<size_t>(l)], c);
            }
            line_.pop_front();
            line_.push_back(b);
        }
        bool busy() const
        {
            for (const Beat &b : line_) if (b.valid) return true;
            return false;
        }
    private:
        std::deque<Beat> line_;
    };

    // Window-major runner. windows[r][e][l]: element e (0 .. k*k - 1) of the window of output row r at lane l (int8).
    // Output row r: lane l = avg_lane(wrap16(sum_e windows[r][e][l])). One window element per cycle per row, rows back to back; the
    // tail adds kAvgTailDepth cycles: cycles = rows * k * k + kAvgTailDepth.
    class GvuAvgPoolPipe
    {
    public:
        static std::vector<std::array<int8_t, GVU_LANES>> run(const std::vector<std::vector<std::array<int8_t, GVU_LANES>>> &windows,
                                                               const AvgCfg &c, uint64_t *cycles = nullptr)
        {
            std::vector<std::array<int8_t, GVU_LANES>> out(windows.size());
            uint64_t n = 0;
            for (size_t r = 0; r < windows.size(); r++)
            {
                std::array<int16_t, GVU_LANES> sum{};
                for (const auto &e : windows[r])
                {
                    for (int l = 0; l < GVU_LANES; l++)
                        sum[static_cast<size_t>(l)] = static_cast<int16_t>(static_cast<uint16_t>(sum[static_cast<size_t>(l)] + e[static_cast<size_t>(l)]));
                    n++;
                }
                for (int l = 0; l < GVU_LANES; l++) out[r][static_cast<size_t>(l)] = avg_lane(sum[static_cast<size_t>(l)], c);
            }
            if (cycles) *cycles = n + kAvgTailDepth;
            return out;
        }
    };
} // namespace rtl_ref_gvu

#endif // RTL_REF_GVU_AVGPOOL_PIPE_H
