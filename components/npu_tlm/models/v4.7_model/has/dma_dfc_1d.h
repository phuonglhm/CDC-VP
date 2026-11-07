// dma_dfc_1d.h -- HAS_DMA_DFC_1D=1: slice / concat (ELEM_WISE modes 6 / 7) the way the hardware does it, with its DMA of one dimension: the data flow controller
// issues one 1-D command per contiguous run (neo_dma_controller.sv; a command = 5 AXI-Lite writes for the first one, then only "clear interrupt + CONTROL"),
// the copy goes DRAM -> scratchpad bank -> DRAM in pieces of at most `stage` bytes, one command after the other (no overlap).
// Cost in CORE cycles (HW_RTL_ALIGNMENT.md, section 14): first command of the operation 6 cycles before its first AXI request, every command: data = ceil(bytes / 16) + 1
//   (128-bit bus, 1 beat per cycle), command -> next command 15, the operation ends 3 cycles after the last beat. MEASURED on neo_lite RTL revision 1 (Verilator, system clock converted
//   to the core clock: first 5 to 7, next read 9 to 21 / write 5 to 7, end 3 to 4); the single-clock ModelSim numbers of neo_lite RTL revision 0 were 19 / 15 / 7 edges (set them with the
//   variables below to reproduce the earlier releases).
// HAS_NEO_REV=1 (neo_lite RTL revision 1): the DRAM side of a 2-D pointer is one contiguous region (address of repetition r = tile offset + r * btt, neo_dma_controller.sv:379-394),
//   so a tile whose DRAM runs are not adjacent cannot be one 2-D command: every run after the first is a NEW DFC command (the previous one ends, `tail`, then the
//   configuration of a new one, `first`, plus HAS_DMA_DFC1D_RESTART edges for the host-side re-start, not measured: 0) instead of the chained 15. These cycles are a LOWER BOUND of the
//   price of the constraint, not a path the hardware has (the DFC cannot re-open a command inside one GEMM_FUSED). Off (HAS_NEO_REV=0): the formulas above, edge for edge.
// Counters (DmaDfc1dStats, counted when HAS_NEO_REV>=1 or HAS_DMA_DFC1D_REPORT=1; they never change cycles or data): a "tile" is a descriptor of a write-back, or one piece of at most
//   `stage` bytes on one side (read / write) of a copy. dram_noncontig = tiles with more than one DRAM run (not one 2-D command); dram_need3d = of those, tiles whose run starts are not evenly
//   spaced (one DRAM-pitch register would not be enough); restarts = commands re-opened by the rule above (HAS_NEO_REV>=1 only).
// Env: HAS_DMA_DFC_1D (0/1, default 0), HAS_DMA_DFC1D_FIRST (6), HAS_DMA_DFC1D_CHAIN (15), HAS_DMA_DFC1D_TAIL (3), HAS_DMA_DFC1D_STAGE (25600 = one vbank0 buffer),
//   HAS_DMA_DFC1D_RESTART (0), HAS_DMA_DFC1D_REPORT (0/1: one line of the counters on stderr at exit).
#ifndef HAS_DMA_DFC_1D_H
#define HAS_DMA_DFC_1D_H

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "has/has_dma.h"
#include "has/neo_rev.h"

namespace has
{
    struct DmaDfc1dStats
    {
        uint64_t ops{0}, commands{0}, bytes{0}, cycles{0};
        uint64_t dram_noncontig{0}, dram_need3d{0}, restarts{0};
    };
    // the single instance; HAS_DMA_DFC1D_REPORT=1 prints its counters once at exit (copies of the struct print nothing)
    inline DmaDfc1dStats &dma_dfc1d_stats()
    {
        static struct Holder
        {
            DmaDfc1dStats s;
            ~Holder()
            {
                const char *e = std::getenv("HAS_DMA_DFC1D_REPORT");
                if (e && std::atoi(e) != 0)
                    std::fprintf(stderr, "DFC1D ops=%llu commands=%llu bytes=%llu cycles=%llu dram_noncontig=%llu dram_need3d=%llu restarts=%llu\n",
                                 static_cast<unsigned long long>(s.ops), static_cast<unsigned long long>(s.commands), static_cast<unsigned long long>(s.bytes),
                                 static_cast<unsigned long long>(s.cycles), static_cast<unsigned long long>(s.dram_noncontig), static_cast<unsigned long long>(s.dram_need3d),
                                 static_cast<unsigned long long>(s.restarts));
            }
        } h;
        return h.s;
    }

    struct DmaDfc1d
    {
        static uint64_t env_u(const char *n, uint64_t d) { const char *e = std::getenv(n); return e ? static_cast<uint64_t>(std::atoll(e)) : d; }
        static bool on() { static const bool v = env_u("HAS_DMA_DFC_1D", 0) != 0; return v; }
        static uint64_t first() { static const uint64_t v = env_u("HAS_DMA_DFC1D_FIRST", 6); return v; }
        static uint64_t chain() { static const uint64_t v = env_u("HAS_DMA_DFC1D_CHAIN", 15); return v; }
        static uint64_t tail() { static const uint64_t v = env_u("HAS_DMA_DFC1D_TAIL", 3); return v; }
        static uint64_t stage() { static const uint64_t v = std::max<uint64_t>(16, env_u("HAS_DMA_DFC1D_STAGE", 25600)); return v; }
        static uint64_t restart() { static const uint64_t v = env_u("HAS_DMA_DFC1D_RESTART", 0); return v; }
        static bool report() { static const bool v = env_u("HAS_DMA_DFC1D_REPORT", 0) != 0; return v; }

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

        // Run starts of one tile on the DRAM side: how many, and whether they are evenly spaced.
        struct Shape
        {
            uint64_t runs{0};
            int64_t prev{0}, pitch{0};
            bool even{true};
            void add(uint32_t addr)
            {
                const int64_t a = static_cast<int64_t>(addr);
                if (runs == 1) pitch = a - prev;
                else if (runs > 1 && a - prev != pitch) even = false;
                prev = a;
                runs++;
            }
            // closes the tile: counters, and the number of commands re-opened by the revision 1 rule
            uint64_t close(DmaDfc1dStats &st, unsigned rev)
            {
                if (runs <= 1) return 0;
                st.dram_noncontig++;
                if (!even) st.dram_need3d++;
                if (rev >= 1) st.restarts += runs - 1;
                return runs - 1;
            }
        };
        static bool track(unsigned rev) { return rev >= 1 || report(); }

        // Edges of the write-back of a result tile vbank2 -> DRAM (DMA_SEND_VBANK2): one 1-D command per contiguous run of the destination descriptor, chained
        static uint64_t write_cost(const DmaNdDesc &d) { return write_cost_rev(d, neo_rev()); }
        static uint64_t write_cost_rev(const DmaNdDesc &d, unsigned rev, uint64_t restart_edges = restart())
        {
            const uint64_t L = d.length() * std::max(1u, d.elem_bytes), run = std::min<uint64_t>(std::max<uint64_t>(1, run_of(d)), L);
            DmaDfc1dStats &st = dma_dfc1d_stats();
            const bool tr = track(rev);
            Shape sh;
            uint64_t cycles = tail(), cmds = 0;
            for (uint64_t k = 0; k < L; k += run)
            {
                const uint64_t b = std::min(run, L - k);
                // revision 1: a run after the first is a new DFC command, not the next repetition of the 2-D pointer
                cycles += (b + 15) / 16 + 1 + ((rev >= 1 && k > 0) ? tail() + restart_edges + first() : chain());
                cmds++;
                if (tr) sh.add(d.addr_at(k));
            }
            if (tr) sh.close(st, rev);
            st.ops++; st.commands += cmds; st.bytes += L; st.cycles += cycles;
            return cycles;
        }

        // Edges the hardware needs to copy src -> dst (same length, byte elements) in DRAM through the scratchpad (the data itself is moved by DmaEngine::gather_dram_nd).
        static uint64_t cost(const DmaNdDesc &src, const DmaNdDesc &dst) { return cost_rev(src, dst, neo_rev()); }
        static uint64_t cost_rev(const DmaNdDesc &src, const DmaNdDesc &dst, unsigned rev, uint64_t restart_edges = restart())
        {
            const uint64_t L = src.length();
            const uint64_t rs = std::min(run_of(src), L), rd = std::min(run_of(dst), L);
            DmaDfc1dStats &st = dma_dfc1d_stats();
            const bool tr = track(rev);
            // commands: per piece of at most `stage` bytes, one read command per source run it touches, one write command per destination run
            uint64_t cycles = first() + tail(), cmds = 0;
            auto cmd = [&](uint64_t bytes, bool reopen)
            {
                cycles += (bytes + 15) / 16 + 1 + (cmds ? (reopen ? tail() + restart_edges + first() : chain()) : 0);
                cmds++;
            };
            auto side = [&](const DmaNdDesc &d, uint64_t run, uint64_t k0, uint64_t k1)
            {
                Shape sh;
                for (uint64_t k = k0; k < k1;)
                {
                    const uint64_t e = std::min(k1, (k / run + 1) * run);
                    cmd(e - k, rev >= 1 && k > k0);
                    if (tr) sh.add(d.addr_at(k));
                    k = e;
                }
                if (tr) sh.close(st, rev);
            };
            for (uint64_t k0 = 0; k0 < L; k0 += stage())
            {
                const uint64_t k1 = std::min(L, k0 + stage());
                side(src, rs, k0, k1);
                side(dst, rd, k0, k1);
            }
            st.ops++; st.commands += cmds; st.bytes += L; st.cycles += cycles;
            return cycles;
        }
    };
}

#endif
