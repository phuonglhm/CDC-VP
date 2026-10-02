// tb_has_mmio_replay.cpp -- replay check of a compiled register-write stream (docs/SW_INTEGRATION_GUIDE.md, section 1).
// Replays an MMIO write stream into has::MmioCompat, walks every GEMM_FUSED with has::TileIter and compares, in program order,
// against the tiles and ELEM_WISE parameters of the reference prog.bin (FEHP). Addresses are NOT compared (the emitted DRAM
// layout may differ from net_has); geometry, tiling, activation and integer parameters are.
//
//   tools/has/tb_has_mmio_replay <net dir> <mmio.txt>     mmio.txt lines: "W <addr hex> <value hex>" | "H <host op ...>" | "# ..."
//   tools/has/tb_has_mmio_replay <net dir> --selftest     build the stream from prog.bin itself (proves the format covers the net)
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>
#include "has/has_mmio_compat.h"

using namespace has;
namespace M = has::mmio;

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
    struct Step
    {
        uint8_t kind{0};
        uint32_t in{0}, out{0}, in_b{0}, silu{0}, cin{0}, cout{0}, kh{0}, kw{0}, sh{0}, sw{0};
        uint32_t w_addr{0}, lut_addr{0}, scale_addr{0}, shift_addr{0}, bias_addr{0};
        std::vector<Tile> tiles;
        uint32_t k{0}, s{0}, p{0};
        AddQ ap;
    };
    struct Prog { std::vector<Tensor> tensors; std::vector<Step> steps; };

    bool load_prog(const std::string &dir, Prog &pg)
    {
        std::ifstream f(dir + "/prog.bin", std::ios::binary);
        std::vector<uint8_t> buf((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        if (buf.size() < 20 || std::memcmp(buf.data(), "FEHP", 4) != 0) return false;
        Cursor c{buf};
        c.pos = 4 + 16;
        for (int i = 0; i < 4; i++) c.u32();
        pg.tensors.resize(c.u32());
        for (auto &t : pg.tensors) { t.addr = c.u32(); t.c = c.u32(); t.h = c.u32(); t.w = c.u32(); }
        pg.steps.resize(c.u32());
        for (auto &s : pg.steps)
        {
            s.kind = c.u8();
            if (s.kind == 0)
            {
                s.in = c.u32(); s.out = c.u32(); c.u32(); s.silu = c.u8();
                s.cin = c.u32(); s.cout = c.u32(); s.kh = c.u32(); s.kw = c.u32(); s.sh = c.u32(); s.sw = c.u32();
                s.w_addr = c.u32(); s.lut_addr = c.u32(); s.scale_addr = c.u32(); s.shift_addr = c.u32(); s.bias_addr = c.u32();
                s.tiles.resize(c.u32());
                for (auto &t : s.tiles)
                {
                    t.c0 = c.i32(); t.c1 = c.i32(); t.oy0 = c.i32(); t.oy1 = c.i32(); t.ox0 = c.i32(); t.ox1 = c.i32();
                    t.iy0 = c.i32(); t.iy1 = c.i32(); t.ix0 = c.i32(); t.ix1 = c.i32(); t.yu = c.i32();
                }
            }
            else if (s.kind == 1) { for (int i = 0; i < 4; i++) c.u32(); }
            else if (s.kind == 2) { c.u32(); const uint32_t n = c.u32(); for (uint32_t i = 0; i < n; i++) c.u32(); }
            else if (s.kind == 4) { for (int i = 0; i < 3; i++) c.u32(); }
            else if (s.kind == 5)
            {
                s.in = c.u32(); s.in_b = c.u32(); s.out = c.u32();
                s.ap.zpA = c.i32(); s.ap.zpB = c.i32(); s.ap.zpO = c.i32();
                s.ap.SA = c.u32(); s.ap.sA = int(c.u32()); s.ap.SB = c.u32(); s.ap.sB = int(c.u32()); s.ap.SO = c.u32(); s.ap.sO = int(c.u32());
            }
            else if (s.kind == 6) { s.in = c.u32(); s.out = c.u32(); s.k = c.u32(); s.s = c.u32(); s.p = c.u32(); }
            else return false;
        }
        return c.pos == buf.size();
    }

    struct W { uint32_t a, v; };
    void w(std::vector<W> &o, uint32_t a, uint32_t v) { o.push_back({a, v}); }
    uint32_t ext(int i) { return M::EXT_BASE + 4u * uint32_t(i); }

    // Self-test stream: what a compiler would emit for this prog.bin (docs/SW_INTEGRATION_GUIDE.md).
    std::vector<W> build_stream(const Prog &pg, long &host_steps)
    {
        std::vector<W> o;
        host_steps = 0;
        for (const Step &s : pg.steps)
        {
            if (s.kind == 0)
            {
                const Tensor &ti = pg.tensors.at(s.in), &to = pg.tensors.at(s.out);
                const Tile &t0 = s.tiles.at(0);
                w(o, M::IN_ADDR, ti.addr); w(o, M::W_ADDR, s.w_addr); w(o, M::OUT_ADDR, to.addr); w(o, M::BIAS_ADDR, s.bias_addr);
                w(o, M::KH, s.kh); w(o, M::KW, s.kw); w(o, M::STRIDE, s.sh);
                w(o, M::PAD, uint32_t(-(t0.iy0 - t0.oy0 * int(s.sh))));
                w(o, M::ACT_TYPE, s.silu ? 2 : 0); w(o, M::HAS_SKIP, 0);
                const uint32_t v[] = {ti.c, ti.h, ti.w, to.c, to.h, to.w, uint32_t(t0.c1 - t0.c0), uint32_t(t0.oy1 - t0.oy0),
                                      uint32_t(t0.ox1 - t0.ox0), s.scale_addr, s.shift_addr, s.lut_addr, 0, 0};
                for (int i = 0; i < 14; i++)
                    if (i != M::X_LUT_ADDR || s.silu) w(o, ext(i), v[i]);
                w(o, M::PUSH_A, 0x12);
            }
            else if (s.kind == 5)
            {
                const Tensor &ta = pg.tensors.at(s.in), &tb = pg.tensors.at(s.in_b), &to = pg.tensors.at(s.out);
                const uint32_t n = to.c * to.h * to.w;
                w(o, M::A_ADDR, ta.addr); w(o, M::B_ADDR, tb.addr); w(o, M::OUT_ADDR, to.addr);
                w(o, M::LEN, n); w(o, M::MODE_PACK, 0); w(o, M::A_LEN, n); w(o, M::B_LEN, n);
                const uint32_t v[] = {uint32_t(s.ap.zpA), uint32_t(s.ap.zpB), uint32_t(s.ap.zpO), s.ap.SA, uint32_t(s.ap.sA),
                                      s.ap.SB, uint32_t(s.ap.sB), s.ap.SO, uint32_t(s.ap.sO)};
                for (int i = 0; i < 9; i++) w(o, ext(M::X_ZP_A + i), v[i]);
                w(o, M::PUSH_A, 0x15);
            }
            else if (s.kind == 6)
            {
                const Tensor &ti = pg.tensors.at(s.in), &to = pg.tensors.at(s.out);
                w(o, M::A_ADDR, ti.addr); w(o, M::OUT_ADDR, to.addr); w(o, M::MODE_PACK, 1); w(o, M::STRIDE, s.s);
                w(o, ext(M::X_IN_C), ti.c); w(o, ext(M::X_IN_H), ti.h); w(o, ext(M::X_IN_W), ti.w);
                w(o, ext(M::X_POOL_K), s.k); w(o, ext(M::X_POOL_P), s.p); w(o, ext(M::X_POOL_MODE), 0);
                w(o, M::PUSH_A, 0x15);
            }
            else host_steps++;
        }
        return o;
    }
}

int main(int argc, char **argv)
{
    if (argc < 3) { std::fprintf(stderr, "usage: %s <net dir> <mmio.txt | --selftest>\n", argv[0]); return 2; }
    Prog pg;
    if (!load_prog(argv[1], pg)) { std::printf("cannot parse %s/prog.bin (FEHP)\n", argv[1]); return 2; }

    std::vector<W> stream;
    long host_lines = 0, host_steps_ref = 0;
    const bool self = std::string(argv[2]) == "--selftest";
    if (self)
        stream = build_stream(pg, host_steps_ref);
    else
    {
        std::ifstream f(argv[2]);
        std::string line;
        long ln = 0;
        while (std::getline(f, line))
        {
            ln++;
            std::istringstream is(line);
            std::string tag;
            if (!(is >> tag) || tag[0] == '#') continue;
            if (tag == "H") { host_lines++; continue; }
            uint32_t a, v;
            if (tag != "W" || !(is >> std::hex >> a >> v)) { std::printf("bad line %ld: %s\n", ln, line.c_str()); return 2; }
            stream.push_back({a, v});
        }
        for (const Step &s : pg.steps) host_steps_ref += (s.kind == 1 || s.kind == 2 || s.kind == 4);
    }

    // Replay: pop after every write so the queue never limits a straight-line stream.
    MmioCompat m(1 << 20);
    std::vector<LayerInstr> got;
    long foreign = 0, pushes = 0;
    for (const W &x : stream)
    {
        if (!m.write(x.a, x.v)) foreign++;
        if (x.a == M::PUSH_A) pushes++;
        LayerInstr li;
        while (m.pop(li)) { got.push_back(li); m.retire(li); }
        if (m.last_error() != Err::NONE)
        {
            std::printf("MMIO error %d after write 0x%08x <- 0x%x (push %ld)\n", int(m.last_error()), x.a, x.v, pushes);
            m.write(M::STATUS, 0);
        }
    }

    // Compare in program order.
    long gi = 0, conv = 0, tiles = 0, bad_tiles = 0, bad_layers = 0, elem = 0, bad_elem = 0, missing = 0;
    for (size_t si = 0; si < pg.steps.size(); si++)
    {
        const Step &s = pg.steps[si];
        if (s.kind != 0 && s.kind != 5 && s.kind != 6) continue;
        if (gi >= long(got.size())) { missing++; continue; }
        const LayerInstr &li = got[size_t(gi++)];
        if (s.kind == 0)
        {
            conv++;
            long lb = 0;
            if (li.opcode != 0x12 || li.act != (s.silu ? 2 : 0) || !li.per_channel || li.in_c != int(s.cin)) lb++;
            TileIter it(li.geom);
            if (it.count() != long(s.tiles.size())) lb++;
            for (long i = 0; i < std::min(it.count(), long(s.tiles.size())); i++)
            {
                const TileGeom a = it.at(i);
                const Tile &b = s.tiles[size_t(i)];
                tiles++;
                if (!(a.c0 == b.c0 && a.c1 == b.c1 && a.oy0 == b.oy0 && a.oy1 == b.oy1 && a.ox0 == b.ox0 && a.ox1 == b.ox1 &&
                      a.iy0 == b.iy0 && a.iy1 == b.iy1 && a.ix0 == b.ix0 && a.ix1 == b.ix1 && a.y_used == b.yu))
                { bad_tiles++; lb++; }
            }
            if (lb) { bad_layers++; if (bad_layers <= 5) std::printf("step %zu: GEMM_FUSED mismatch (%ld)\n", si, lb); }
        }
        else
        {
            elem++;
            bool ok = li.opcode == 0x15 && li.mode == (s.kind == 5 ? 0 : 1);
            if (ok && s.kind == 5)
                ok = !li.from_float_scale && li.add.zpA == s.ap.zpA && li.add.zpB == s.ap.zpB && li.add.zpO == s.ap.zpO &&
                     li.add.SA == s.ap.SA && li.add.sA == s.ap.sA && li.add.SB == s.ap.SB && li.add.sB == s.ap.sB &&
                     li.add.SO == s.ap.SO && li.add.sO == s.ap.sO;
            if (ok && s.kind == 6)
            {
                const Tensor &ti = pg.tensors.at(s.in);
                ok = li.pool_k == int(s.k) && li.pool_s == int(s.s) && li.pool_p == int(s.p) && li.c == int(ti.c) &&
                     li.h == int(ti.h) && li.w == int(ti.w);
            }
            if (!ok) { bad_elem++; std::printf("step %zu: ELEM_WISE mismatch\n", si); }
        }
    }
    const long extra = long(got.size()) - gi;
    std::printf("[tb_has_mmio_replay] %s: %zu MMIO writes, %ld pushes, %zu instructions decoded, %ld MMIO errors, %ld writes outside the block\n",
                self ? "selftest (stream built from prog.bin)" : argv[2], stream.size(), pushes, got.size(), long(m.n_errors), foreign);
    std::printf("[tb_has_mmio_replay] GEMM_FUSED %ld layers, %ld tiles, %ld tiles differ, %ld layers differ | ELEM_WISE %ld, %ld differ | "
                "missing %ld, extra %ld | host steps: prog %ld, stream H lines %ld\n",
                conv, tiles, bad_tiles, bad_layers, elem, bad_elem, missing, extra, host_steps_ref, host_lines);
    const bool pass = m.n_errors == 0 && foreign == 0 && bad_tiles == 0 && bad_layers == 0 && bad_elem == 0 && missing == 0 && extra == 0 &&
                      conv > 0;
    std::printf("[tb_has_mmio_replay] RESULT: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
