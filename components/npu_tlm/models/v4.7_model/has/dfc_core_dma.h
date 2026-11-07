// dfc_core_dma.h -- HAS_DFC_CORE_DMA=1 (with HAS_NEO_REV>=1): the DMA of the SAURIA core the way the data flow controller of neo_lite RTL revision 1 issues it.
// The DFC has ONE reader / writer; a tile's operands go in as one-dimensional commands one after the other (neo_dma_controller.sv: BRING_A, BRING_B, BRING_C),
// each command = ceil(bytes / 16) + 1 edges of data and, from the second command of a transfer on, `chain` edges (15, HAS_DMA_DFC1D_CHAIN) from the last beat of the
// command before to the first request of the next (HW_RTL_ALIGNMENT.md, section 14). The model moves a whole operand in one transfer with no cost per command; with the
// knob on every read transfer of a tile starts `extra` edges late: extra = commands + (commands - 1) * chain (0 commands: 0).
//   A (input window): one command per contiguous DRAM run of the in-bounds window (rows of the window; a window as wide as the tensor merges its rows; one that is also
//      as high as the tensor merges its channel planes) = the fewest commands a compiler can program (y_lim / c_lim / ett registers, neo_interface.sv READ_ARGS)
//   B (weights of a channel block): ONE command (Ck_eq: the whole tile in a single transaction, the compiler lays the block out contiguously)
//   C (bias preload): ONE command (ESTIMATE: how the hardware fills the partial sums is not known)
// These cycles are a LOWER BOUND of the price of the command sequence, not the extra cycles of the program: the next tile's transfers run under the core of the tile before
// (exec_tiles_ovl), so they show only where the transfers outlast the core. Off (default): extra is 0 and nothing changes, edge for edge.
// Env: HAS_DFC_CORE_DMA (0/1, default 0; has no effect with HAS_NEO_REV=0), HAS_DMA_DFC1D_CHAIN (15, shared with dma_dfc_1d.h), HAS_DMA_DFC1D_REPORT (counters on stderr at exit).
#ifndef HAS_DFC_CORE_DMA_H
#define HAS_DFC_CORE_DMA_H

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "has/dma_dfc_1d.h"
#include "has/neo_rev.h"

namespace has
{
    struct DfcCoreDmaStats { uint64_t transfers{0}, commands{0}, extra{0}, a_cmds{0}, b_cmds{0}, c_cmds{0}; };
    inline DfcCoreDmaStats &dfc_core_dma_stats()
    {
        static struct Holder
        {
            DfcCoreDmaStats s;
            ~Holder()
            {
                const char *e = std::getenv("HAS_DMA_DFC1D_REPORT");
                if (e && std::atoi(e) != 0 && s.transfers)
                    std::fprintf(stderr, "DFCCORE transfers=%llu commands=%llu (A %llu, B %llu, C %llu) extra=%llu\n", static_cast<unsigned long long>(s.transfers),
                                 static_cast<unsigned long long>(s.commands), static_cast<unsigned long long>(s.a_cmds), static_cast<unsigned long long>(s.b_cmds),
                                 static_cast<unsigned long long>(s.c_cmds), static_cast<unsigned long long>(s.extra));
            }
        } h;
        return h.s;
    }

    struct DfcCoreDma
    {
        static bool on() { static const bool v = DmaDfc1d::env_u("HAS_DFC_CORE_DMA", 0) != 0 && neo_rev() >= 1; return v; }

        // edges added to the first burst of a transfer made of `cmds` commands
        static uint32_t extra_of(uint64_t cmds, uint64_t chain_edges = DmaDfc1d::chain())
        {
            return cmds == 0 ? 0u : static_cast<uint32_t>(cmds + (cmds - 1) * chain_edges);
        }

        // commands for the in-bounds part of an input window: rows [iy0, iy1) x columns [ix0, ix1) (clipped to the image), channels [c0, c0 + nc) of [in_c][in_h][in_w]
        static uint64_t a_commands(int in_h, int in_w, int iy0, int iy1, int ix0, int ix1, uint32_t c0, uint32_t nc)
        {
            const int y0 = std::max(iy0, 0), y1 = std::min(iy1, in_h), x0 = std::max(ix0, 0), x1 = std::min(ix1, in_w);
            if (y1 <= y0 || x1 <= x0) return 1;
            const DmaNdDesc d = DmaNdDesc::tile_window_3d(0, 0, uint32_t(in_h), uint32_t(in_w), c0, nc, uint32_t(y0), uint32_t(y1 - y0), uint32_t(x0), uint32_t(x1 - x0));
            const uint64_t L = d.length(), run = std::min(std::max<uint64_t>(1, DmaDfc1d::run_of(d)), L);
            return (L + run - 1) / run;
        }

        // the three entry points called by HasNpuTop: 0 when the knob is off
        static uint32_t extra_a(int in_h, int in_w, int iy0, int iy1, int ix0, int ix1, uint32_t c0, uint32_t nc)
        {
            if (!on()) return 0;
            const uint64_t n = a_commands(in_h, in_w, iy0, iy1, ix0, ix1, c0, nc);
            return book(n, dfc_core_dma_stats().a_cmds);
        }
        static uint32_t extra_b() { return on() ? book(1, dfc_core_dma_stats().b_cmds) : 0u; }
        static uint32_t extra_c() { return on() ? book(1, dfc_core_dma_stats().c_cmds) : 0u; }

    private:
        static uint32_t book(uint64_t cmds, uint64_t &per_kind)
        {
            DfcCoreDmaStats &st = dfc_core_dma_stats();
            const uint32_t e = extra_of(cmds);
            st.transfers++; st.commands += cmds; st.extra += e; per_kind += cmds;
            return e;
        }
    };
}

#endif
