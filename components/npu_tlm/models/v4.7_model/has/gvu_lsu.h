// gvu_lsu.h -- vector-unit load/store unit (vector-unit specification table 1 "Load/store unit": address generator modes
// v32int32, v32int8, int32, int8 for the multi-bank Scratchpad SRAM; section 4, table 3 bank organisation).
// Accounting model: the pipelines keep computing on their own data; each stage reports the Scratchpad transfers it makes
// (bank, mode, element count), and the LSU counts transfers (one per vector or scalar access) and bytes per bank and mode.
// The counts back question H8 (Scratchpad capacity and ports) with data; they do not change results or timing.
#ifndef HAS_GVU_LSU_H
#define HAS_GVU_LSU_H

#include <cstdint>
#include <cstdio>
#include <string>

namespace has
{
    enum class LsuBank { SBANK = 0, VBANK0 = 1, VBANK1 = 2, VBANK2 = 3 };
    enum class LsuMode { V32I32 = 0, V32I8 = 1, S32 = 2, S8 = 3 };

    class GvuLsu
    {
    public:
        static constexpr int LANES = 32;
        struct Cnt { uint64_t rd{0}, wr{0}, rd_bytes{0}, wr_bytes{0}; };

        // `elems` elements moved in mode `m`: vector modes move LANES elements per access, scalar modes one.
        void load(LsuBank b, LsuMode m, uint64_t elems) { acc(b, m, elems, false); }
        void store(LsuBank b, LsuMode m, uint64_t elems) { acc(b, m, elems, true); }

        const Cnt &at(LsuBank b, LsuMode m) const { return c_[int(b)][int(m)]; }
        uint64_t accesses() const
        {
            uint64_t n = 0;
            for (auto &row : c_) for (auto &x : row) n += x.rd + x.wr;
            return n;
        }
        void reset() { *this = GvuLsu(); }

        // One line per bank / mode that was used: "vbank#0 v32int8 rd 120 (3840 B) wr 40 (1280 B)".
        std::string report() const
        {
            static const char *bank[] = {"sbank", "vbank#0", "vbank#1", "vbank#2"};
            static const char *mode[] = {"v32int32", "v32int8", "int32", "int8"};
            std::string s;
            char line[160];
            for (int b = 0; b < 4; b++)
                for (int m = 0; m < 4; m++)
                {
                    const Cnt &x = c_[b][m];
                    if (!x.rd && !x.wr) continue;
                    std::snprintf(line, sizeof line, "%s %s rd %llu (%llu B) wr %llu (%llu B)\n", bank[b], mode[m],
                                  (unsigned long long)x.rd, (unsigned long long)x.rd_bytes, (unsigned long long)x.wr,
                                  (unsigned long long)x.wr_bytes);
                    s += line;
                }
            return s;
        }

    private:
        Cnt c_[4][4];
        void acc(LsuBank b, LsuMode m, uint64_t elems, bool wr)
        {
            const bool vec = m == LsuMode::V32I32 || m == LsuMode::V32I8;
            const uint64_t esz = (m == LsuMode::V32I32 || m == LsuMode::S32) ? 4 : 1;
            const uint64_t n = vec ? (elems + LANES - 1) / LANES : elems;
            Cnt &x = c_[int(b)][int(m)];
            if (wr) { x.wr += n; x.wr_bytes += elems * esz; }
            else { x.rd += n; x.rd_bytes += elems * esz; }
        }
    };
} // namespace has

#endif // HAS_GVU_LSU_H
