// dma_dfc_1d.h -- HAS_DMA_DFC_1D=1: slice / concat (ELEM_WISE modes 6 / 7) the way the hardware does it, with its DMA of one dimension: the data flow controller
// issues one 1-D command per contiguous run (neo_dma_controller.sv; a command = 5 AXI-Lite writes for the first one, then only "clear interrupt + CONTROL"),
// the copy goes DRAM -> scratchpad bank -> DRAM in pieces of at most `stage` bytes, one command after the other (no overlap).
// Cost (MEASURED on neo_lite@9b704bb with pulp axi / common_cells, one clock, no mux and no CDC = a LOWER BOUND; docs/ra_rtl_neo_lite/do_dfc_dma.md):
//   first command of the operation: 19 edges before its first AXI request (14 writes + 5), every command: data = ceil(bytes / 16) + 1 edges (128-bit bus, 1 beat per edge),
//   command -> next command: 15 edges (last beat to first request), the operation ends 7 edges after the last beat (5 to the interrupt + 2 to clear it).
// Env: HAS_DMA_DFC_1D (0/1, default 0), HAS_DMA_DFC1D_FIRST (19), HAS_DMA_DFC1D_CHAIN (15), HAS_DMA_DFC1D_TAIL (7), HAS_DMA_DFC1D_STAGE (25600 = one vbank0 buffer).
#ifndef HAS_DMA_DFC_1D_H
#define HAS_DMA_DFC_1D_H

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include "has/has_dma.h"

namespace has
{
    struct DmaDfc1dStats { uint64_t ops{0}, commands{0}, bytes{0}, cycles{0}; };
    inline DmaDfc1dStats &dma_dfc1d_stats() { static DmaDfc1dStats s; return s; }

    struct DmaDfc1d
    {
        static uint64_t env_u(const char *n, uint64_t d) { const char *e = std::getenv(n); return e ? static_cast<uint64_t>(std::atoll(e)) : d; }
        static bool on() { static const bool v = env_u("HAS_DMA_DFC_1D", 0) != 0; return v; }
        static uint64_t first() { static const uint64_t v = env_u("HAS_DMA_DFC1D_FIRST", 19); return v; }
        static uint64_t chain() { static const uint64_t v = env_u("HAS_DMA_DFC1D_CHAIN", 15); return v; }
        static uint64_t tail() { static const uint64_t v = env_u("HAS_DMA_DFC1D_TAIL", 7); return v; }
        static uint64_t stage() { static const uint64_t v = std::max<uint64_t>(16, env_u("HAS_DMA_DFC1D_STAGE", 25600)); return v; }

        // longest contiguous byte run of the descriptor (byte elements; dimensions merge while the stride equals the run so far)
        static uint64_t run_of(const DmaNdDesc &d)
        {
            if (d.s[0] != 1) return 1;
            uint64_t run = std::max(1u, d.n[0]);
            for (int j = 1; j < 4; j++)
            {
                if (std::max(1u, d.n[j]) == 1) continue;
                if (d.s[j] != static_cast<int64_t>(run)) break;
                run *= std::max(1u, d.n[j]);
            }
            return run;
        }

        static bool out_vbank2() { static const bool v = env_u("HAS_GVU_OUT_VBANK2", 0) != 0; return v; }
        // Edges of the write-back of a result tile vbank2 -> DRAM (DMA_SEND_VBANK2): one 1-D command per contiguous run of the destination descriptor, chained
        static uint64_t write_cost(const DmaNdDesc &d)
        {
            const uint64_t L = d.length() * std::max(1u, d.elem_bytes), run = std::min<uint64_t>(std::max<uint64_t>(1, run_of(d)), L);
            uint64_t cycles = tail(), cmds = 0;
            for (uint64_t k = 0; k < L; k += run) { const uint64_t b = std::min(run, L - k); cycles += (b + 15) / 16 + 1 + chain(); cmds++; }
            DmaDfc1dStats &st = dma_dfc1d_stats();
            st.ops++; st.commands += cmds; st.bytes += L; st.cycles += cycles;
            return cycles;
        }

        // Edges the hardware needs to copy src -> dst (same length, byte elements) in DRAM through the scratchpad (the data itself is moved by DmaEngine::gather_dram_nd).
        static uint64_t cost(const DmaNdDesc &src, const DmaNdDesc &dst)
        {
            const uint64_t L = src.length();
            const uint64_t rs = std::min(run_of(src), L), rd = std::min(run_of(dst), L);
            // commands: per piece of at most `stage` bytes, one read command per source run it touches, one write command per destination run
            uint64_t cycles = first() + tail(), cmds = 0;
            auto cmd = [&](uint64_t bytes) { cycles += (bytes + 15) / 16 + 1 + (cmds ? chain() : 0); cmds++; };
            for (uint64_t k0 = 0; k0 < L; k0 += stage())
            {
                const uint64_t k1 = std::min(L, k0 + stage());
                for (uint64_t k = k0; k < k1;) { const uint64_t e = std::min(k1, (k / rs + 1) * rs); cmd(e - k); k = e; }
                for (uint64_t k = k0; k < k1;) { const uint64_t e = std::min(k1, (k / rd + 1) * rd); cmd(e - k); k = e; }
            }
            DmaDfc1dStats &st = dma_dfc1d_stats();
            st.ops++; st.commands += cmds; st.bytes += L; st.cycles += cycles;
            return cycles;
        }
    };
}

#endif
