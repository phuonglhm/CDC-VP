// tb_has_tile_iter.cpp -- checks has/has_tile_iter.h against every tile of an exported network (prog.bin, "FEHP").
// For each conv step the per-layer instruction fields are taken the way the DFC would receive them: output tensor
// C/H/W, kh/kw/stride from the step, tile size (cout_t, h_t, w_t) = size of the first tile, padding = minus the
// input-window origin of the first tile. The iterator must then regenerate ALL tiles of the layer, in the same order,
// with identical output region, input window and Y_used (X_used = channels of the tile is what the testbench passes).
//
//   tools/has/tb_has_tile_iter <net dir> [--verbose]      e.g. fe_work/has/net_has
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include "has/has_tile_iter.h"

namespace
{
    struct Cursor
    {
        const std::vector<uint8_t> &b;
        size_t pos{0};
        uint32_t u32() { uint32_t v; std::memcpy(&v, &b.at(pos + 3) - 3, 4); pos += 4; return v; }
        int32_t i32() { return static_cast<int32_t>(u32()); }
        uint8_t u8() { return b.at(pos++); }
    };
    struct Tensor { uint32_t addr, c, h, w; };
    struct Tile { int c0, c1, oy0, oy1, ox0, ox1, iy0, iy1, ix0, ix1, yu; };
}

int main(int argc, char **argv)
{
    if (argc < 2) { std::fprintf(stderr, "usage: %s <net dir> [--verbose]\n", argv[0]); return 2; }
    const bool verbose = argc > 2 && std::string(argv[2]) == "--verbose";
    std::ifstream f(std::string(argv[1]) + "/prog.bin", std::ios::binary);
    std::vector<uint8_t> buf((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (buf.size() < 20 || std::memcmp(buf.data(), "FEHP", 4) != 0) { std::printf("not a FEHP prog.bin\n"); return 2; }

    Cursor c{buf};
    c.pos = 4 + 16;
    for (int i = 0; i < 4; i++) c.u32();   // staging addresses
    std::vector<Tensor> tensors(c.u32());
    for (auto &t : tensors) { t.addr = c.u32(); t.c = c.u32(); t.h = c.u32(); t.w = c.u32(); }

    const uint32_t nsteps = c.u32();
    long layers = 0, tiles = 0, bad_tiles = 0, bad_layers = 0, bad_count = 0, bad_yu = 0, bad_win = 0, bad_reg = 0;
    long asym_pad = 0;
    for (uint32_t si = 0; si < nsteps; si++)
    {
        const uint8_t kind = c.u8();
        if (kind == 0)
        {
            const uint32_t in = c.u32(), out = c.u32();
            c.u32(); c.u8();                                   // skip, silu
            c.u32();                                           // cin
            const uint32_t cout = c.u32(), kh = c.u32(), kw = c.u32(), sh = c.u32(), sw = c.u32();
            for (int i = 0; i < 5; i++) c.u32();               // w/lut/scale/shift/bias addr
            std::vector<Tile> tl(c.u32());
            for (auto &t : tl)
            {
                t.c0 = c.i32(); t.c1 = c.i32(); t.oy0 = c.i32(); t.oy1 = c.i32(); t.ox0 = c.i32(); t.ox1 = c.i32();
                t.iy0 = c.i32(); t.iy1 = c.i32(); t.ix0 = c.i32(); t.ix1 = c.i32(); t.yu = c.i32();
            }
            (void)in;
            layers++;
            tiles += long(tl.size());
            if (tl.empty()) { bad_layers++; continue; }
            const Tensor &dst = tensors.at(out);

            has::LayerGeom g;
            g.cout = int(dst.c); g.oh = int(dst.h); g.ow = int(dst.w);
            if (int(cout) != g.cout) { std::printf("step %u: cout %u != output tensor C %u\n", si, cout, dst.c); }
            g.kh = int(kh); g.kw = int(kw); g.sy = int(sh); g.sx = int(sw);
            g.cout_t = tl[0].c1 - tl[0].c0; g.h_t = tl[0].oy1 - tl[0].oy0; g.w_t = tl[0].ox1 - tl[0].ox0;
            g.pad_t = -(tl[0].iy0 - tl[0].oy0 * g.sy);
            g.pad_l = -(tl[0].ix0 - tl[0].ox0 * g.sx);
            if (g.pad_t != g.pad_l) asym_pad++;

            has::TileIter it(g);
            long layer_bad = 0;
            if (it.count() != long(tl.size()))
            {
                bad_count++;
                layer_bad++;
                std::printf("step %u: iterator gives %ld tiles, prog.bin has %zu\n", si, it.count(), tl.size());
            }
            const long n = std::min(it.count(), long(tl.size()));
            for (long i = 0; i < n; i++)
            {
                const has::TileGeom a = it.at(i);
                const Tile &b = tl[i];
                const bool reg = a.c0 == b.c0 && a.c1 == b.c1 && a.oy0 == b.oy0 && a.oy1 == b.oy1 && a.ox0 == b.ox0 && a.ox1 == b.ox1;
                const bool win = a.iy0 == b.iy0 && a.iy1 == b.iy1 && a.ix0 == b.ix0 && a.ix1 == b.ix1;
                const bool yu = a.y_used == b.yu;
                if (!reg) bad_reg++;
                if (!win) bad_win++;
                if (!yu) bad_yu++;
                if (!(reg && win && yu))
                {
                    bad_tiles++;
                    if (layer_bad++ < 3)
                        std::printf("step %u tile %ld: iter c[%d,%d) oy[%d,%d) ox[%d,%d) iy[%d,%d) ix[%d,%d) yu %d | prog c[%d,%d) oy[%d,%d) ox[%d,%d) iy[%d,%d) ix[%d,%d) yu %d\n",
                                    si, i, a.c0, a.c1, a.oy0, a.oy1, a.ox0, a.ox1, a.iy0, a.iy1, a.ix0, a.ix1, a.y_used,
                                    b.c0, b.c1, b.oy0, b.oy1, b.ox0, b.ox1, b.iy0, b.iy1, b.ix0, b.ix1, b.yu);
                }
            }
            if (layer_bad) bad_layers++;
            if (verbose)
                std::printf("step %3u  C%4d H%4d W%4d  k%dx%d s%d p%d/%d  tile %2dx%2dx%3d  tiles %4zu  %s\n", si, g.cout, g.oh, g.ow,
                            g.kh, g.kw, g.sy, g.pad_t, g.pad_l, g.cout_t, g.h_t, g.w_t, tl.size(), layer_bad ? "MISMATCH" : "ok");
        }
        else if (kind == 1) { for (int i = 0; i < 4; i++) c.u32(); }
        else if (kind == 2) { c.u32(); const uint32_t n = c.u32(); for (uint32_t i = 0; i < n; i++) c.u32(); }
        else if (kind == 4) { for (int i = 0; i < 3; i++) c.u32(); }
        else if (kind == 5) { for (int i = 0; i < 12; i++) c.u32(); }
        else if (kind == 6) { for (int i = 0; i < 5; i++) c.u32(); }
        else { std::printf("unknown step kind %u at step %u\n", kind, si); return 2; }
    }
    if (c.pos != buf.size()) std::printf("WARNING: %zu trailing bytes in prog.bin\n", buf.size() - c.pos);

    std::printf("[tb_has_tile_iter] conv layers %ld, tiles %ld | mismatching tiles %ld (region %ld, input window %ld, Y_used %ld), "
                "tile-count mismatches %ld, layers with any mismatch %ld, layers with pad_t != pad_l %ld\n",
                layers, tiles, bad_tiles, bad_reg, bad_win, bad_yu, bad_count, bad_layers, asym_pad);
    const bool pass = bad_tiles == 0 && bad_count == 0 && bad_layers == 0 && layers > 0;
    std::printf("[tb_has_tile_iter] RESULT: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
