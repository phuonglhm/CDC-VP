// tb_dfc_tile_loop_neo.cpp -- has/rtl_ref_dfc_tile_loop_neo.h (the tile pointer generator of the neo_lite DFC) against has::TileIter, on every GEMM_FUSED of a compiled program.
// Reads mmio.txt ("W <addr hex> <value hex>"), decodes it with has::MmioCompat (as tb_has_mmio_replay) and, for each layer and each model tile order (TileIter order 0 / 1 / 2),
// maps the order onto the RTL loops, runs the port tile by tile and compares the tile visited with TileIter::at(i):
//   order 0 (ox fastest, oy, cout slowest)  -> loop_order 0, x = ox, y = oy, k = cout
//   order 1 (cout fastest, ox, oy slowest)  -> loop_order 2 (k, then x, y), x = ox, y = oy
//   order 2 (oy fastest, ox, cout slowest)  -> loop_order 0 with the roles of x and y swapped: x = oy, y = ox (the RTL steps are free)
// Also: Cin split (the model walks every tile's Cin parts one after the other, cout outermost) against the nearest RTL nesting (loop_order 1), the weights fetched again
// (RTL weights_change vs the model's two-block weight cache), and the single-tile-per-loop edge cases.
//   tools/has/tb_dfc_tile_loop_neo <mmio.txt> [--verbose]
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "has/has_mmio_compat.h"
#include "has/rtl_ref_dfc_tile_loop_neo.h"

using namespace has;
using namespace has::rtl_ref_dfc;

namespace
{
    struct Agg { long layers{0}, tiles{0}, seq_bad_layers{0}, seq_bad_tiles{0}, last_bad{0}, rtl_loads{0}, model_loads{0}; };

    // weights of the model: two blocks kept (key = cout block); a block already in the other half is flipped in without a load
    long model_loads(const std::vector<int> &keys)
    {
        long loads = 1;
        int cur = keys.at(0), other = -1;
        for (size_t i = 1; i < keys.size(); i++)
        {
            if (keys[i] == cur) continue;
            if (keys[i] == other) { std::swap(cur, other); continue; }
            loads++;
            other = cur;
            cur = keys[i];
        }
        return loads;
    }
}

int main(int argc, char **argv)
{
    if (argc < 2) { std::fprintf(stderr, "usage: %s <mmio.txt> [--verbose]\n", argv[0]); return 2; }
    const bool verbose = argc > 2 && std::string(argv[2]) == "--verbose";
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

    Agg ag[3];
    long gemm = 0, split_layers = 0, split_same = 0, single_tile = 0, order1_same_loads = 0, order1_layers = 0, bad_total = 0, o1_diff_nk2 = 0, o1_diff_other = 0, o1_nk2_equal = 0;
    for (const LayerInstr &li : prog)
    {
        if (li.opcode != 0x12) continue;
        gemm++;
        for (int order = 0; order < 3; order++)
        {
            LayerGeom g = li.geom;
            g.order = order;
            TileIter it(g);
            const long n = it.count();
            const int nk = it.n_cout_tiles(), ny = it.n_row_tiles(), nx = int(n / (long(nk) * ny));
            TileLoopParams p;
            p.ifmaps_x = p.ifmaps_y = p.ifmaps_c = p.psums_x = p.psums_y = p.psums_k = p.weights_k = p.weights_c = 1;   // pointers = tile indices
            p.k_lim = uint16_t(nk - 1);
            if (order == 2) { p.x_lim = uint16_t(ny - 1); p.y_lim = uint16_t(nx - 1); p.loop_order = 0; }
            else { p.x_lim = uint16_t(nx - 1); p.y_lim = uint16_t(ny - 1); p.loop_order = order == 0 ? 0 : 2; }
            TileLoopNeo rtl(p);
            long bad = 0;
            std::vector<int> keys;
            long rtl_loads = 1;
            for (long i = 0; i < n; i++)
            {
                const TileGeom t = it.at(i);
                const TileLoopOut &o = rtl.out();
                const int xi = int(order == 2 ? o.psums_y : o.psums_x), yi = int(order == 2 ? o.psums_x : o.psums_y), ci = int(o.psums_k);
                const bool ok = t.ox0 == xi * g.w_t && t.oy0 == yi * g.h_t && t.c0 == ci * g.cout_t;
                if (!ok) bad++;
                keys.push_back(t.c0);
                if (rtl.last_iter() != (i + 1 == n)) ag[order].last_bad++;
                rtl.advance();
                if (i + 1 < n && rtl.out().weights_change) rtl_loads++;
            }
            ag[order].layers++; ag[order].tiles += n;
            if (bad) { ag[order].seq_bad_layers++; ag[order].seq_bad_tiles += bad; bad_total += bad; }
            ag[order].rtl_loads += rtl_loads;
            ag[order].model_loads += model_loads(keys);
            if (order == 0 && n == 1) single_tile++;
            if (order == 1)
            {
                order1_layers++;
                const bool eq = rtl_loads == model_loads(keys);
                if (eq) order1_same_loads++;
                if (!eq) (nk == 2 ? o1_diff_nk2 : o1_diff_other)++;
                if (eq && nk == 2) o1_nk2_equal++;
            }
        }

        // Cin split: model order = for tile i (order 0), for part 0..nsplit-1 ; RTL nearest = loop_order 1 (c fastest, then k, then x, y)
        if (li.cin_t > 0 && li.cin_t < li.in_c)
        {
            split_layers++;
            const int nsplit = li.in_c / li.cin_t;
            LayerGeom g = li.geom;
            g.order = 0;
            TileIter it(g);
            const long n = it.count();
            const int nk = it.n_cout_tiles(), ny = it.n_row_tiles(), nx = int(n / (long(nk) * ny));
            TileLoopParams p;
            p.ifmaps_x = p.ifmaps_y = p.ifmaps_c = p.psums_x = p.psums_y = p.psums_k = p.weights_k = p.weights_c = 1;
            p.x_lim = uint16_t(nx - 1); p.y_lim = uint16_t(ny - 1); p.k_lim = uint16_t(nk - 1); p.c_lim = uint16_t(nsplit - 1); p.loop_order = 1;
            TileLoopNeo rtl(p);
            bool same = true;
            for (long s = 0; s < n * nsplit; s++)
            {
                const TileGeom t = it.at(s / nsplit);
                const TileLoopOut &o = rtl.out();
                if (!(t.ox0 == int(o.psums_x) * g.w_t && t.oy0 == int(o.psums_y) * g.h_t && t.c0 == int(o.psums_k) * g.cout_t && int(rtl.c()) == int(s % nsplit))) { same = false; break; }
                rtl.advance();
            }
            if (same) split_same++;
        }
    }

    std::printf("[tb_dfc_tile_loop_neo] %s: %ld GEMM_FUSED layers (%ld with one tile only)\n", argv[1], gemm, single_tile);
    static const char *names[3] = {"order 0 (ox, oy, cout)", "order 1 (cout, ox, oy)", "order 2 (oy, ox, cout)"};
    for (int o = 0; o < 3; o++)
        std::printf("  %-24s tiles %7ld | layers whose tile sequence differs from the RTL port %ld (tiles %ld), last_iter wrong %ld | weight fetches: RTL %ld, model (two blocks) %ld\n", names[o],
                    ag[o].tiles, ag[o].seq_bad_layers, ag[o].seq_bad_tiles, ag[o].last_bad, ag[o].rtl_loads, ag[o].model_loads);
    std::printf("  Cin split: %ld layers; sequence the same as the RTL nesting c, k, x, y: %ld\n", split_layers, split_same);
    std::printf("  order 1: layers with equal weight fetches %ld of %ld; layers that differ: %ld with 2 cout blocks, %ld others; layers with 2 cout blocks that are equal: %ld\n",
                order1_same_loads, order1_layers, o1_diff_nk2, o1_diff_other, o1_nk2_equal);
    (void)verbose;
    const bool pass = bad_total == 0 && ag[0].last_bad + ag[1].last_bad + ag[2].last_bad == 0 && gemm > 0;
    std::printf("[tb_dfc_tile_loop_neo] RESULT: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
