// tb_dfc_core_dma_neo.cpp -- how many one-dimensional DFC commands the transfers of the core (input window A, weights B, bias preload C) are made of, and the edges
// has/dfc_core_dma.h would add (HAS_DFC_CORE_DMA=1), counted on a compiled program without simulating it. Replays mmio.txt ("W <addr hex> <value hex>") into has::MmioCompat,
// walks every GEMM_FUSED with has::TileIter in the planner order (order 0) and, per tile and Cin part, takes the same transfers HasNpuTop::prefetch_ab / prefetch_c issue:
// A every step, B when the (channel block, Cin part) is not one of the two blocks kept (HasNpuTop's weight cache), C once per tile.
//   tools/has/tb_dfc_core_dma_neo <mmio.txt>
// The totals are the edges the knob adds to the transfers, NOT the cycles it adds to the program (transfers run under the core of the tile before).
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "has/dfc_core_dma.h"
#include "has/has_mmio_compat.h"

using namespace has;

int sc_main(int argc, char **argv)
{
    if (argc < 2) { std::fprintf(stderr, "usage: %s <mmio.txt>\n", argv[0]); return 2; }
    std::ifstream f(argv[1]);
    std::string ln;
    MmioCompat m(1 << 20);
    std::vector<LayerInstr> prog;
    while (std::getline(f, ln))
    {
        std::istringstream is(ln);
        std::string tag;
        if (!(is >> tag) || tag != "W") continue;
        uint32_t a, v;
        if (!(is >> std::hex >> a >> v)) { std::printf("bad line: %s\n", ln.c_str()); return 2; }
        m.write(a, v);
        LayerInstr li;
        while (m.pop(li)) { prog.push_back(li); m.retire(li); }
        if (m.last_error() != Err::NONE) { std::printf("MMIO error %d\n", int(m.last_error())); m.write(mmio::STATUS, 0); }
    }

    uint64_t gemm = 0, steps = 0, tiles = 0, a_cmds = 0, b_loads = 0, c_tr = 0, a_ex = 0, b_ex = 0, c_ex = 0, a_max = 0;
    for (const LayerInstr &li : prog)
    {
        if (li.opcode != 0x12) continue;
        gemm++;
        LayerGeom g = li.geom;
        g.order = 0;
        TileIter it(g);
        const uint32_t cin = uint32_t(li.in_c), ct = (li.cin_t > 0 && li.cin_t < li.in_c) ? uint32_t(li.cin_t) : cin, nsplit = cin / ct;
        int64_t cur = -1, other = -1;
        for (long i = 0; i < it.count(); i++)
        {
            const TileGeom t = it.at(i);
            tiles++;
            c_tr++; c_ex += DfcCoreDma::extra_of(1);
            for (uint32_t part = 0; part < nsplit; part++)
            {
                steps++;
                const uint64_t n = DfcCoreDma::a_commands(li.in_h, li.in_w, t.iy0, t.iy1, t.ix0, t.ix1, part * ct, ct);
                a_cmds += n; a_ex += DfcCoreDma::extra_of(n); a_max = std::max(a_max, n);
                const int64_t key = int64_t(t.c0) * nsplit + part;
                if (key == cur) continue;
                if (key == other) { std::swap(cur, other); continue; }
                b_loads++; b_ex += DfcCoreDma::extra_of(1);
                other = cur; cur = key;
            }
        }
    }
    std::printf("[tb_dfc_core_dma_neo] %s: GEMM_FUSED %llu layers, %llu tiles, %llu core passes\n", argv[1], (unsigned long long)gemm, (unsigned long long)tiles, (unsigned long long)steps);
    std::printf("  A: %llu commands (max %llu per window, mean %.2f), extra edges %llu\n", (unsigned long long)a_cmds, (unsigned long long)a_max, steps ? double(a_cmds) / double(steps) : 0.0, (unsigned long long)a_ex);
    std::printf("  B: %llu weight transfers, extra edges %llu\n", (unsigned long long)b_loads, (unsigned long long)b_ex);
    std::printf("  C: %llu preload transfers, extra edges %llu\n", (unsigned long long)c_tr, (unsigned long long)c_ex);
    std::printf("  total extra edges added to the transfers: %llu (per tile %.1f)\n", (unsigned long long)(a_ex + b_ex + c_ex), tiles ? double(a_ex + b_ex + c_ex) / double(tiles) : 0.0);
    return 0;
}
