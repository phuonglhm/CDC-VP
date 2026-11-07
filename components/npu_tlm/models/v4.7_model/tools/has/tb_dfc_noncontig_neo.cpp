// tb_dfc_noncontig_neo.cpp -- how many tiles of a compiled program break the DRAM-contiguous rule of the 2-D pointer of neo_lite RTL revision 1, without simulating the program.
// Replays mmio.txt ("W <addr hex> <value hex>" lines, as tb_has_mmio_replay) into has::MmioCompat, walks every GEMM_FUSED with has::TileIter and every ELEM_WISE slice / concat,
// and charges them to has::DmaDfc1d with HAS_NEO_REV 0 and 1 (the same calls HasNpuTop makes with HAS_GVU_OUT_VBANK2=1 / HAS_DMA_DFC_1D=1: write_cost for the result tile of a GEMM,
// cost for the copy of a slice / concat). DMA_TENSOR COPY is linear on both sides (contiguous by construction) and does not go through the 1-D command path.
//
//   tools/has/tb_dfc_noncontig_neo <mmio.txt> [--csv <file>]
//
// A tile is "contiguous" when its DRAM side is one run (one 2-D command), "pitch" when it has several runs at an even spacing (one DRAM-pitch register would make it one 2-D command),
// "need3d" when the spacing is uneven (a second level would be needed). Cycles are the LOWER BOUND of the price of the constraint (HW_RTL_ALIGNMENT.md, section 14), not a hardware path.
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "has/dma_dfc_1d.h"
#include "has/has_mmio_compat.h"

using namespace has;

namespace
{
    struct Acc
    {
        uint64_t ops{0}, noncontig{0}, need3d{0}, restarts{0}, cmds{0}, cyc0{0}, cyc1{0};
        void add(const DmaDfc1dStats &b, const DmaDfc1dStats &a1, uint64_t c0, uint64_t c1)
        {
            ops += a1.ops - b.ops; cmds += a1.commands - b.commands;
            noncontig += a1.dram_noncontig - b.dram_noncontig; need3d += a1.dram_need3d - b.dram_need3d; restarts += a1.restarts - b.restarts;
            cyc0 += c0; cyc1 += c1;
        }
    };

    // write-back: a.ops tiles, each one descriptor. copy: a.ops copies; the shape counters are per piece and side, so "one 2-D command" is not derived
    void line(const char *what, const Acc &a, bool copy)
    {
        const unsigned long long c0 = a.cyc0, c1 = a.cyc1;
        if (!copy)
            std::printf("  %-24s tiles %8llu | one 2-D command %8llu, pitch %8llu, need3d %8llu | commands %9llu, restarts %9llu | cycles rev0 %12llu, rev1 (restart 0) %12llu (+%llu)\n",
                        what, (unsigned long long)a.ops, (unsigned long long)(a.ops - a.noncontig), (unsigned long long)(a.noncontig - a.need3d), (unsigned long long)a.need3d,
                        (unsigned long long)a.cmds, (unsigned long long)a.restarts, c0, c1, c1 - c0);
        else
            std::printf("  %-24s copies %6llu | piece-sides not contiguous: pitch %8llu, need3d %8llu | commands %9llu, restarts %9llu | cycles rev0 %12llu, rev1 (restart 0) %12llu (+%llu)\n",
                        what, (unsigned long long)a.ops, (unsigned long long)(a.noncontig - a.need3d), (unsigned long long)a.need3d, (unsigned long long)a.cmds,
                        (unsigned long long)a.restarts, c0, c1, c1 - c0);
    }
}

int sc_main(int argc, char **argv)
{
    if (argc < 2) { std::fprintf(stderr, "usage: %s <mmio.txt> [--csv <file>]\n", argv[0]); return 2; }
    FILE *csv = nullptr;
    for (int i = 2; i + 1 < argc; i++)
        if (std::string(argv[i]) == "--csv") csv = std::fopen(argv[i + 1], "w");
    if (csv) std::fprintf(csv, "instr,kind,cout,oh,ow,cout_t,h_t,w_t,tiles,noncontig,need3d,restarts,cycles_rev0,cycles_rev1\n");

    std::ifstream f(argv[1]);
    std::string ln;
    MmioCompat m(1 << 20);
    std::vector<LayerInstr> prog;
    uint64_t writes = 0;
    while (std::getline(f, ln))
    {
        std::istringstream is(ln);
        std::string tag;
        if (!(is >> tag) || tag[0] == '#' || tag != "W") continue;
        uint32_t a, v;
        if (!(is >> std::hex >> a >> v)) { std::printf("bad line: %s\n", ln.c_str()); return 2; }
        m.write(a, v);
        writes++;
        LayerInstr li;
        while (m.pop(li)) { prog.push_back(li); m.retire(li); }
        if (m.last_error() != Err::NONE) { std::printf("MMIO error %d after write 0x%08x\n", int(m.last_error()), a); m.write(mmio::STATUS, 0); }
    }

    Acc wb, sl, cc;
    uint64_t gemm = 0, other = 0;
    long idx = 0;
    for (const LayerInstr &li : prog)
    {
        idx++;
        if (li.opcode == 0x12)
        {
            gemm++;
            const LayerGeom &g = li.geom;
            TileIter it(g);
            Acc layer;
            for (long i = 0; i < it.count(); i++)
            {
                const TileGeom t = it.at(i);
                const uint32_t ht = uint32_t(t.oy1 - t.oy0);
                const uint32_t real_c = uint32_t(std::max(0, std::min(t.c1, g.cout) - t.c0));
                const uint32_t real_w = uint32_t(std::max(0, std::min(t.ox1, g.ow) - t.ox0));
                const DmaNdDesc d = DmaNdDesc::tile_writeback_3d(li.out_addr, uint32_t(g.cout), uint32_t(g.oh), uint32_t(g.ow), uint32_t(t.c0), real_c, uint32_t(t.oy0), ht,
                                                                  uint32_t(t.ox0), real_w);
                const uint64_t c0 = DmaDfc1d::write_cost_rev(d, 0, 0);
                const DmaDfc1dStats mid = dma_dfc1d_stats();
                const uint64_t c1 = DmaDfc1d::write_cost_rev(d, 1, 0);
                // the shape counters come from the rev 1 call (rev 0 does not track unless HAS_DMA_DFC1D_REPORT)
                layer.add(mid, dma_dfc1d_stats(), c0, c1);
            }
            wb.ops += layer.ops; wb.noncontig += layer.noncontig; wb.need3d += layer.need3d; wb.restarts += layer.restarts; wb.cmds += layer.cmds;
            wb.cyc0 += layer.cyc0; wb.cyc1 += layer.cyc1;
            if (csv)
                std::fprintf(csv, "%ld,gemm,%d,%d,%d,%d,%d,%d,%llu,%llu,%llu,%llu,%llu,%llu\n", idx, g.cout, g.oh, g.ow, g.cout_t, g.h_t, g.w_t, (unsigned long long)layer.ops,
                             (unsigned long long)layer.noncontig, (unsigned long long)layer.need3d, (unsigned long long)layer.restarts, (unsigned long long)layer.cyc0,
                             (unsigned long long)layer.cyc1);
        }
        else if (li.opcode == 0x15 && (li.mode == 6 || li.mode == 7))
        {
            other++;
            std::vector<std::pair<DmaNdDesc, DmaNdDesc>> ops;
            if (li.mode == 6)
            {
                const DmaNdDesc sd = DmaNdDesc::tensor_chw(li.a_addr, uint32_t(li.c), uint32_t(li.h), uint32_t(li.w), 1)
                                         .sliced_chw(li.pool_mode, uint32_t(li.pool_p), uint32_t(li.pool_k), uint32_t(li.pool_s));
                ops.push_back({sd, DmaNdDesc::linear(li.out_addr, sd.total_bytes(), 1)});
            }
            else
            {
                const uint32_t off_a = (li.b_addr != 0) ? 0u : uint32_t(li.pool_p);
                const DmaNdDesc dd_a = DmaNdDesc::concat_chw(li.out_addr, uint32_t(li.c), uint32_t(li.h), uint32_t(li.w), li.pool_mode, off_a, uint32_t(li.pool_k), 1);
                ops.push_back({DmaNdDesc::linear(li.a_addr, dd_a.total_bytes(), 1), dd_a});
                if (li.b_addr != 0)
                {
                    const int dim = (li.pool_mode == 0) ? li.c : (li.pool_mode == 1 ? li.h : li.w);
                    const uint32_t len_b = uint32_t(std::max(1, dim - li.pool_k));
                    const DmaNdDesc dd_b = DmaNdDesc::concat_chw(li.out_addr, uint32_t(li.c), uint32_t(li.h), uint32_t(li.w), li.pool_mode, uint32_t(li.pool_k), len_b, 1);
                    ops.push_back({DmaNdDesc::linear(li.b_addr, dd_b.total_bytes(), 1), dd_b});
                }
            }
            Acc &dst = li.mode == 6 ? sl : cc;
            Acc layer;
            for (const auto &p : ops)
            {
                const uint64_t c0 = DmaDfc1d::cost_rev(p.first, p.second, 0, 0);
                const DmaDfc1dStats mid = dma_dfc1d_stats();
                const uint64_t c1 = DmaDfc1d::cost_rev(p.first, p.second, 1, 0);
                const DmaDfc1dStats e = dma_dfc1d_stats();
                layer.add(mid, e, c0, c1);
            }
            dst.ops += layer.ops; dst.noncontig += layer.noncontig; dst.need3d += layer.need3d; dst.restarts += layer.restarts; dst.cmds += layer.cmds;
            dst.cyc0 += layer.cyc0; dst.cyc1 += layer.cyc1;
            if (csv)
                std::fprintf(csv, "%ld,%s,%d,%d,%d,0,0,0,%llu,%llu,%llu,%llu,%llu,%llu\n", idx, li.mode == 6 ? "slice" : "concat", li.c, li.h, li.w, (unsigned long long)layer.ops,
                             (unsigned long long)layer.noncontig, (unsigned long long)layer.need3d, (unsigned long long)layer.restarts, (unsigned long long)layer.cyc0,
                             (unsigned long long)layer.cyc1);
        }
    }
    if (csv) std::fclose(csv);

    std::printf("[tb_dfc_noncontig_neo] %s: %llu MMIO writes, %zu instructions (GEMM_FUSED %llu, slice / concat %llu), %ld MMIO errors\n", argv[1], (unsigned long long)writes,
                prog.size(), (unsigned long long)gemm, (unsigned long long)other, long(m.n_errors));
    std::printf("  tiles: descriptor of a write-back (one per GEMM tile) or piece of a copy on one side; 'one 2-D command' = contiguous DRAM side; HAS_NEO_REV=1 rule\n");
    line("GEMM result write-back", wb, false);
    line("slice", sl, true);
    line("concat", cc, true);
    std::printf("  total cycles rev0 %llu, rev1 (restart 0) %llu (+%llu), restarts %llu\n", (unsigned long long)(wb.cyc0 + sl.cyc0 + cc.cyc0), (unsigned long long)(wb.cyc1 + sl.cyc1 + cc.cyc1),
                (unsigned long long)(wb.cyc1 + sl.cyc1 + cc.cyc1 - wb.cyc0 - sl.cyc0 - cc.cyc0), (unsigned long long)(wb.restarts + sl.restarts + cc.restarts));
    return 0;
}
