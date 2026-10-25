// tb_gvu_agu_nd_move.cpp -- tests of GvuVlsuNd (has/gvu_agu_nd_vlsu.h) and of the MOVE operation (has/gvu_agu_nd_move.h) on the
// frozen Scratchpad and VLSU ports. Pure C++17.
//   T5 GvuVlsuNd with general programs on two ports: the address output of the frozen unit = a stand-alone GvuAguNd, every cycle
//   T6 MOVE cases (slice / split / concat on the channel axis, concat on H, slice of W tiles, [C][H][Wt] -> [H][C][Wt], tiles of
//      YOLOv8m C2f model.8 / model.6 and SPPF model.9, int32 ports): destination bank = reference (dst[a_dst(k)] = src[a_src(k)]),
//      byte for byte, rows outside the destination unchanged, source unchanged; cycles = N + 3 without idle cycles; then 300
//      random programs (negative strides included) whose every address lies inside its bank
//   T7 idle cycles (30 % of the read requests held back): idle cycles keep the address -> 0 rows wrong; idle_reload (as AGU_INCR
//      does) -> rows wrong (the B18 kind of fault)
//   T8 GvuVlsuNdPort: (a) unbound, and bound with AGU_ND_CTRL off = the frozen GvuVlsu, every Out field, every cycle;
//      (b) vbank0 with a source and a destination picked per phase by sel (residual add: A read, result written), phases
//      switched by i_agu_addr_reset with the pick of the new phase: address of every access = addr_at of the program of the phase
// Build (repository root): g++ -std=c++17 -O2 -Wall -Wextra -I. tools/has/tb_gvu_agu_nd_move.cpp -o tb_gvu_agu_nd_move
#include <algorithm>
#include <cstdio>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
#include "has/gvu_agu_nd_move.h"

using namespace rtl_ref_gvu;

static std::mt19937 rng(1181u);
static unsigned urand(unsigned lo, unsigned hi) { return std::uniform_int_distribution<unsigned>(lo, hi)(rng); }
static int irand(int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); }
static bool coin(double p) { return std::bernoulli_distribution(p)(rng); }

// ---- rows of a MOVE port in buffer 0 of the Scratchpad
static int bank(int mp) { return mp == MV_V0_I8 ? 0 : (mp == MV_V1_I8 || mp == MV_V1_I32) ? 1 : 2; }
static unsigned rows(int mp) { return mp == MV_V0_I8 ? 800u : mp == MV_V1_I8 ? 128u : mp == MV_V2_I8 ? 1024u : mp == MV_V1_I32 ? 32u : 256u; }
static SpBytes get_row(GvuScratchpad &sp, int mp, unsigned r)
{
    if (mp == MV_V0_I8 || GvuMove::wide(mp)) return sp.sram(bank(mp), 0).word(r);
    const SpBytes w = sp.sram(bank(mp), 0).word(r >> 2);
    return SpBytes(w.begin() + (r & 3u) * 32u, w.begin() + (r & 3u) * 32u + 32);
}
static void put_row(GvuScratchpad &sp, int mp, unsigned r, const SpBytes &d)
{
    if (mp == MV_V0_I8 || GvuMove::wide(mp)) { sp.sram(bank(mp), 0).poke(r, d); return; }
    SpBytes w = sp.sram(bank(mp), 0).word(r >> 2);
    for (size_t i = 0; i < 32; i++) w[(r & 3u) * 32u + i] = d[i];
    sp.sram(bank(mp), 0).poke(r >> 2, w);
}
static void fill(GvuScratchpad &sp, int mp)
{
    const size_t wb = GvuMove::wide(mp) ? 128 : 32;
    for (unsigned r = 0; r < rows(mp); r++)
    {
        SpBytes d(wb);
        for (auto &b : d) b = static_cast<uint8_t>(urand(0, 255));
        put_row(sp, mp, r, d);
    }
}
static std::vector<unsigned> seq(const AguNdDesc &d, unsigned nrows)
{
    // A MOVE program must stay inside the bank: the AGU wraps modulo 2^AW, and vbank0 has 800 of its 1024 rows (a read above
    // row 799 returns the previous read data, has/rtl_ref_gvu_scratchpad.h), so the reference takes no modulo.
    std::vector<unsigned> a;
    for (unsigned i3 = 0; i3 < d.n[3]; i3++)
        for (unsigned i2 = 0; i2 < d.n[2]; i2++)
            for (unsigned i1 = 0; i1 < d.n[1]; i1++)
                for (unsigned i0 = 0; i0 < d.n[0]; i0++)
                {
                    const int64_t x = int64_t(d.base) + int64_t(i0) * d.s[0] + int64_t(i1) * d.s[1] + int64_t(i2) * d.s[2] + int64_t(i3) * d.s[3];
                    if (x < 0 || x >= int64_t(nrows)) throw std::logic_error("test program leaves the bank");
                    a.push_back(static_cast<unsigned>(x));
                }
    return a;
}
// Lowest and highest offset of a program relative to its base.
static void span(const AguNdDesc &d, int64_t &lo, int64_t &hi)
{
    lo = hi = 0;
    for (int k = 0; k < 4; k++)
    {
        const int64_t e = int64_t(d.n[k] - 1u) * d.s[k];
        if (e < 0) lo += e; else hi += e;
    }
}
static AguNdDesc desc(unsigned base, std::vector<unsigned> n, std::vector<int> s)
{
    AguNdDesc d;
    d.base = base;
    for (size_t k = 0; k < 4; k++) { d.n[k] = k < n.size() ? n[k] : 1u; d.s[k] = k < s.size() ? s[k] : 0; }
    return d;
}

struct MoveCheck { uint64_t moves{0}, rows{0}, cycles{0}, bad_rows{0}, bad_cycles{0}, idle{0}; };

// One MOVE on a fresh Scratchpad with random contents, checked against the reference.
static void one_move(MoveCheck &mc, const char *name, int src, const AguNdDesc &ds, int dst, const AguNdDesc &dd, bool idle_reload = false,
                     double hold = 0.0, bool quiet = false)
{
    GvuScratchpad sp;
    fill(sp, src); fill(sp, dst);
    std::vector<SpBytes> s0, exp;
    for (unsigned r = 0; r < rows(src); r++) s0.push_back(get_row(sp, src, r));
    for (unsigned r = 0; r < rows(dst); r++) exp.push_back(get_row(sp, dst, r));
    const std::vector<unsigned> as = seq(ds, rows(src)), ad = seq(dd, rows(dst));
    for (size_t k = 0; k < as.size(); k++) exp[ad[k]] = s0[as[k]];
    GvuMove mv(sp);
    std::function<bool(uint64_t)> gate;
    if (hold > 0) gate = [hold](uint64_t) { return !coin(hold); };
    const GvuMove::Result r = mv.run(src, ds, dst, dd, idle_reload, gate);
    uint64_t bad = 0;
    for (unsigned q = 0; q < rows(dst); q++) if (get_row(sp, dst, q) != exp[q]) bad++;
    for (unsigned q = 0; q < rows(src); q++) if (get_row(sp, src, q) != s0[q]) bad++;
    const bool cyc_ok = hold > 0 ? r.cycles == r.rows + 3 + r.idle : r.cycles == r.rows + 3;
    if (!quiet) std::printf("  %-44s N %5llu cycles %5llu idle %4llu  rows wrong %llu%s\n", name, (unsigned long long)r.rows, (unsigned long long)r.cycles,
                            (unsigned long long)r.idle, (unsigned long long)bad, cyc_ok ? "" : "  CYCLES != N + 3 + idle");
    mc.moves++; mc.rows += r.rows; mc.cycles += r.cycles; mc.bad_rows += bad; mc.idle += r.idle;
    if (!cyc_ok) mc.bad_cycles++;
}

// T5: general programs through GvuVlsuNd = stand-alone GvuAguNd.
static int t5()
{
    const ScratchpadParams P;
    const int aw[5] = {P.SBANK_AW(), P.SBANK_AW() + 2, P.VBANK0_AW(), P.VBANK1_AW() + 2, P.VBANK2_AW() + 2};
    uint64_t cyc = 0, bad = 0;
    for (int round = 0; round < 300; round++)
    {
        const unsigned pa = urand(0, 4), pb = (pa + urand(1, 4)) % 5;
        GvuVlsuNd b(P);
        GvuAguNd ra(aw[pa]), rb(aw[pb]);
        GvuVlsuNd::In ib;
        ib.ctrl.src_port = pa; ib.ctrl.src_on = true; ib.ctrl.dst_port = pb; ib.ctrl.dst_on = true; ib.ctrl.stop = coin(0.3);
        for (int k = 0; k < 4; k++) { ib.src.n[k] = urand(1, 9); ib.src.s[k] = irand(-200, 200); ib.dst.n[k] = urand(1, 9); ib.dst.s[k] = irand(-200, 200); }
        ib.src.base = urand(0, (1u << aw[pa]) - 1u); ib.dst.base = urand(0, (1u << aw[pb]) - 1u);
        for (int c = 0; c < 2000; c++, cyc++)
        {
            GvuVlsu::In &v = ib.v;
            v = GvuVlsu::In{};
            v.i_agu_addr_reset = c == 0 || coin(0.002);
            bool rq[5];
            for (unsigned p = 0; p < 5; p++) rq[p] = coin(0.6);
            v.i_sbank_rd_0 = rq[0]; v.i_sbank_rd_1 = rq[1]; v.i_vbank0_rd = rq[2]; v.i_vbank1_rd = rq[3]; v.i_vbank1_port_sel = 2; v.i_vbank2_rd = rq[4]; v.i_vbank2_port_sel = true;
            int *m[5] = {&v.i_sbank_agu_mode_0, &v.i_sbank_agu_mode_1, &v.i_vbank0_agu_mode, &v.i_vbank1_agu_mode, &v.i_vbank2_agu_mode};
            *m[pa] = AGU_ND; *m[pb] = AGU_ND;
            GvuAguNd::In na, nb;
            na.i_req = rq[pa]; na.i_reset = v.i_agu_addr_reset; na.d = ib.src; na.stop = ib.ctrl.stop;
            nb.i_req = rq[pb]; nb.i_reset = v.i_agu_addr_reset; nb.d = ib.dst; nb.stop = ib.ctrl.stop;
            const GvuVlsu::Out o = b.eval(ib);
            const unsigned x[5] = {o.o_sbank_addr_0, o.o_sbank_addr_1, o.o_vbank0_addr, o.o_vbank1_addr_2, o.o_vbank2_addr_1};
            if (c > 0 && (x[pa] != ra.eval(na).o_addr || x[pb] != rb.eval(nb).o_addr))
            {
                if (bad < 5) std::printf("  T5 bad: round %d cycle %d ports %u/%u: %u/%u %u/%u\n", round, c, pa, pb, x[pa], ra.eval(na).o_addr, x[pb], rb.eval(nb).o_addr);
                bad++;
            }
            b.tick(ib); ra.tick(na); rb.tick(nb);
        }
    }
    std::printf("T5 GvuVlsuNd address = GvuAguNd, general programs: %llu cycles, %llu differ\n", (unsigned long long)cyc, (unsigned long long)bad);
    return bad ? 1 : 0;
}

static int t6()
{
    MoveCheck mc;
    std::printf("T6 MOVE cases (rows of 32 lanes, [C][H][Wt] layout):\n");
    // channel concat: A [6][5][3] at vbank0 row 0, B [4][5][3] at row 100 -> [10][5][3] at vbank2 int8 row 10
    one_move(mc, "concat C: A [6][5][3] -> out[0:6]", MV_V0_I8, desc(0, {3, 5, 6}, {1, 3, 15}), MV_V2_I8, desc(10, {90}, {1}));
    one_move(mc, "concat C: B [4][5][3] -> out[6:10]", MV_V0_I8, desc(100, {60}, {1}), MV_V2_I8, desc(100, {3, 5, 4}, {1, 3, 15}));
    // concat on H: A [4][3][2], B [4][5][2] -> [4][8][2]
    one_move(mc, "concat H: A [4][3][2] -> out[:,0:3]", MV_V0_I8, desc(0, {24}, {1}), MV_V2_I8, desc(0, {2, 3, 4}, {1, 2, 16}));
    one_move(mc, "concat H: B [4][5][2] -> out[:,3:8]", MV_V0_I8, desc(200, {2, 5, 4}, {1, 2, 10}), MV_V2_I8, desc(6, {2, 5, 4}, {1, 2, 16}));
    // slice of W tiles 1..3 of X [3][4][5] (vbank2) -> vbank0
    one_move(mc, "slice Wt: X[:,:,1:4] of [3][4][5]", MV_V2_I8, desc(1, {3, 4, 3}, {1, 5, 20}), MV_V0_I8, desc(50, {36}, {1}));
    // permutation [5][6][2] -> [6][5][2]
    one_move(mc, "permute [C][H][Wt] -> [H][C][Wt], 5x6x2", MV_V0_I8, desc(0, {2, 5, 6}, {1, 12, 2}), MV_V2_I8, desc(0, {60}, {1}));
    // split of X [8][4][1] (vbank2) into C 0..2 and 3..7
    one_move(mc, "split C: X[0:3] of [8][4][1]", MV_V2_I8, desc(0, {12}, {1}), MV_V0_I8, desc(0, {12}, {1}));
    one_move(mc, "split C: X[3:8] of [8][4][1]", MV_V2_I8, desc(12, {4, 5}, {1, 4}), MV_V0_I8, desc(300, {20}, {1}));
    // YOLOv8m C2f model.8 (20x20, Wt 1), a tile of 8 channels per input: 4 inputs [8][20][1] in vbank0 -> concat [32][20][1] in vbank2
    for (unsigned j = 0; j < 4; j++)
    {
        const std::string n = "C2f model.8 tile: in" + std::to_string(j) + " [8][20][1] -> out[" + std::to_string(8 * j) + ":]";
        one_move(mc, n.c_str(), MV_V0_I8, desc(200 * j, {20, 8}, {1, 20}), MV_V2_I8, desc(160 * j, {160}, {1}));
    }
    // C2f split: cv1 tile [16][20][1] in vbank2 -> two halves [8][20][1] in vbank0 (rows 0 and 400)
    one_move(mc, "C2f model.8 split: cv1[0:8] of [16][20][1]", MV_V2_I8, desc(0, {160}, {1}), MV_V0_I8, desc(0, {160}, {1}));
    one_move(mc, "C2f model.8 split: cv1[8:16] of [16][20][1]", MV_V2_I8, desc(160, {20, 8}, {1, 20}), MV_V0_I8, desc(400, {160}, {1}));
    // SPPF model.9: [cv1, mp1, mp2, mp3] tiles [8][20][1] -> [32][20][1]
    for (unsigned j = 0; j < 4; j++)
    {
        const std::string n = "SPPF model.9 tile: part" + std::to_string(j) + " [8][20][1] -> out[" + std::to_string(8 * j) + ":]";
        one_move(mc, n.c_str(), MV_V0_I8, desc(160 * j, {160}, {1}), MV_V2_I8, desc(160 * j, {20, 8}, {1, 20}));
    }
    // C2f model.6 (40x40, Wt 2): 3 inputs [4][40][2] -> [12][40][2]
    for (unsigned j = 0; j < 3; j++)
    {
        const std::string n = "C2f model.6 tile: in" + std::to_string(j) + " [4][40][2] -> out[" + std::to_string(4 * j) + ":]";
        one_move(mc, n.c_str(), MV_V0_I8, desc(320 * j / 2, {2, 40, 4}, {1, 2, 80}), MV_V2_I8, desc(320 * j, {320}, {1}));
    }
    // other port pairs
    one_move(mc, "vbank2 int8 -> vbank1 int8, [4][4][2] permute", MV_V2_I8, desc(7, {2, 4, 4}, {1, 8, 2}), MV_V1_I8, desc(3, {32}, {1}));
    one_move(mc, "vbank0 -> vbank1 int8, slice rows 500..599", MV_V0_I8, desc(500, {100}, {1}), MV_V1_I8, desc(20, {100}, {1}));
    one_move(mc, "vbank2 int32 -> vbank1 int32, [4][8] transpose", MV_V2_I32, desc(0, {4, 8}, {8, 1}), MV_V1_I32, desc(0, {32}, {1}));
    one_move(mc, "vbank1 int32 -> vbank2 int32, concat at 200", MV_V1_I32, desc(0, {32}, {1}), MV_V2_I32, desc(200, {32}, {1}));
    // random programs (inside the banks, any port pair of the same row width): strides up to about 1/20 of the source bank,
    // redrawn until the program fits; the base is then drawn so that every address of the program lies in the bank
    MoveCheck rnd;
    for (int t = 0; t < 300; t++)
    {
        const int pairs[6][2] = {{MV_V0_I8, MV_V2_I8}, {MV_V2_I8, MV_V0_I8}, {MV_V0_I8, MV_V1_I8}, {MV_V1_I8, MV_V2_I8}, {MV_V2_I32, MV_V1_I32}, {MV_V1_I32, MV_V2_I32}};
        const int *pp = pairs[t % 6];
        const int smax = std::max(2, static_cast<int>(rows(pp[0]) / 20u));
        AguNdDesc ds, dd;
        int64_t lo, hi;
        do
        {
            for (int k = 0; k < 4; k++) { ds.n[k] = urand(1, 4); ds.s[k] = irand(-smax, smax); }
            span(ds, lo, hi);
        } while (hi - lo >= int64_t(rows(pp[0])) || ds.length() > rows(pp[1]));
        dd.n = {{ds.length(), 1, 1, 1}}; dd.s = {{1, 0, 0, 0}};
        ds.base = urand(static_cast<unsigned>(-lo), static_cast<unsigned>(int64_t(rows(pp[0])) - 1 - hi));
        dd.base = urand(0, rows(pp[1]) - dd.n[0]);
        one_move(rnd, "random", pp[0], ds, pp[1], dd, false, 0.0, true);
    }
    std::printf("  random programs: %llu moves, %llu rows, rows wrong %llu, cycle mismatches %llu\n", (unsigned long long)rnd.moves,
                (unsigned long long)rnd.rows, (unsigned long long)rnd.bad_rows, (unsigned long long)rnd.bad_cycles);
    std::printf("T6 MOVE: %llu named moves, %llu rows, %llu cycles, rows wrong %llu, cycle mismatches %llu\n", (unsigned long long)mc.moves,
                (unsigned long long)mc.rows, (unsigned long long)mc.cycles, (unsigned long long)mc.bad_rows, (unsigned long long)mc.bad_cycles);
    return (mc.bad_rows || mc.bad_cycles || rnd.bad_rows || rnd.bad_cycles) ? 1 : 0;
}

static int t7()
{
    MoveCheck hold, reload;
    for (int t = 0; t < 20; t++)
    {
        one_move(hold, "", MV_V0_I8, desc(0, {20, 8}, {1, 20}), MV_V2_I8, desc(160, {2, 10, 8}, {1, 2, 20}), false, 0.3, true);
        one_move(reload, "", MV_V0_I8, desc(0, {20, 8}, {1, 20}), MV_V2_I8, desc(160, {2, 10, 8}, {1, 2, 20}), true, 0.3, true);
    }
    std::printf("T7 idle cycles (30 %%): hold -> %llu moves, %llu idle cycles, rows wrong %llu, cycle mismatches %llu; idle_reload -> %llu moves, rows wrong %llu\n",
                (unsigned long long)hold.moves, (unsigned long long)hold.idle, (unsigned long long)hold.bad_rows, (unsigned long long)hold.bad_cycles,
                (unsigned long long)reload.moves, (unsigned long long)reload.bad_rows);
    return (hold.bad_rows || hold.bad_cycles || reload.bad_rows == 0) ? 1 : 0;
}

static bool same(const GvuVlsu::Out &a, const GvuVlsu::Out &b)
{
    return a.o_sbank_en_0 == b.o_sbank_en_0 && a.o_sbank_wen_0 == b.o_sbank_wen_0 && a.o_sbank_addr_0 == b.o_sbank_addr_0 && a.o_sbank_wmask_0 == b.o_sbank_wmask_0 &&
           a.o_sbank_en_1 == b.o_sbank_en_1 && a.o_sbank_wen_1 == b.o_sbank_wen_1 && a.o_sbank_addr_1 == b.o_sbank_addr_1 && a.o_sbank_wmask_1 == b.o_sbank_wmask_1 &&
           a.o_vbank0_en == b.o_vbank0_en && a.o_vbank0_wen == b.o_vbank0_wen && a.o_vbank0_addr == b.o_vbank0_addr && a.o_vbank0_wmask == b.o_vbank0_wmask &&
           a.o_vbank1_port_sel == b.o_vbank1_port_sel &&
           a.o_vbank1_en_0 == b.o_vbank1_en_0 && a.o_vbank1_wen_0 == b.o_vbank1_wen_0 && a.o_vbank1_addr_0 == b.o_vbank1_addr_0 && a.o_vbank1_wmask_0 == b.o_vbank1_wmask_0 &&
           a.o_vbank1_en_1 == b.o_vbank1_en_1 && a.o_vbank1_wen_1 == b.o_vbank1_wen_1 && a.o_vbank1_addr_1 == b.o_vbank1_addr_1 && a.o_vbank1_wmask_1 == b.o_vbank1_wmask_1 &&
           a.o_vbank1_en_2 == b.o_vbank1_en_2 && a.o_vbank1_wen_2 == b.o_vbank1_wen_2 && a.o_vbank1_addr_2 == b.o_vbank1_addr_2 && a.o_vbank1_wmask_2 == b.o_vbank1_wmask_2 &&
           a.o_vbank2_port_sel == b.o_vbank2_port_sel &&
           a.o_vbank2_en_0 == b.o_vbank2_en_0 && a.o_vbank2_wen_0 == b.o_vbank2_wen_0 && a.o_vbank2_addr_0 == b.o_vbank2_addr_0 && a.o_vbank2_wmask_0 == b.o_vbank2_wmask_0 &&
           a.o_vbank2_en_1 == b.o_vbank2_en_1 && a.o_vbank2_wen_1 == b.o_vbank2_wen_1 && a.o_vbank2_addr_1 == b.o_vbank2_addr_1 && a.o_vbank2_wmask_1 == b.o_vbank2_wmask_1 &&
           a.o_pooling_window_valid == b.o_pooling_window_valid && a.o_pooling_window_data == b.o_pooling_window_data &&
           a.o_feeder_first == b.o_feeder_first && a.o_feeder_last == b.o_feeder_last && a.o_pooling_last_wr == b.o_pooling_last_wr;
}

static int t8()
{
    // (a) pass-through
    uint64_t cyc = 0, bad_a = 0;
    {
        GvuVlsu ref;
        GvuVlsuNdPort a, b;
        for (int c = 0; c < 200000; c++, cyc++)
        {
            if (c % 5000 == 0)
            {
                AguNdCtrl off;
                off.src_port = urand(0, 4); off.dst_port = urand(0, 4); off.idle_reload = coin(0.5); off.stop = coin(0.5);
                AguNdDesc ds, dd;
                for (int k = 0; k < 4; k++) { ds.n[k] = urand(1, 9); ds.s[k] = irand(-50, 50); dd.n[k] = urand(1, 9); dd.s[k] = irand(-50, 50); }
                b.bind(off, ds, dd, {{static_cast<uint8_t>(urand(0, 2)), 0, static_cast<uint8_t>(urand(0, 2)), 0, 0}});
            }
            GvuVlsu::In v;
            v.i_agu_addr_reset = coin(0.01);
            v.i_sbank_rd_0 = coin(0.5); v.i_sbank_wr_0 = coin(0.2); v.i_sbank_agu_mode_0 = irand(0, 3); v.i_sbank_start_addr_0 = urand(0, 4095); v.i_sbank_wmask_0 = urand(0, 15);
            v.i_sbank_rd_1 = coin(0.5); v.i_sbank_wr_1 = coin(0.2); v.i_sbank_agu_mode_1 = irand(0, 3); v.i_sbank_start_addr_1 = urand(0, 16383); v.i_sbank_wmask_1 = coin(0.5);
            v.i_vbank0_rd = coin(0.5); v.i_vbank0_wr = coin(0.2); v.i_vbank0_agu_mode = irand(0, 3); v.i_vbank0_start_addr = urand(0, 1023); v.i_vbank0_lane_wmask = urand(0, 0xFFFFFFFFu);
            v.i_vbank1_port_sel = urand(0, 2); v.i_vbank1_rd = coin(0.5); v.i_vbank1_wr = coin(0.2); v.i_vbank1_agu_mode = irand(0, 3); v.i_vbank1_start_addr = urand(0, 127); v.i_vbank1_lane_wmask = urand(0, 0xFFFFFFFFu);
            v.i_vbank2_port_sel = coin(0.5); v.i_vbank2_rd = coin(0.5); v.i_vbank2_wr = coin(0.2); v.i_vbank2_agu_mode = irand(0, 3); v.i_vbank2_start_addr = urand(0, 1023); v.i_vbank2_lane_wmask = urand(0, 0xFFFFFFFFu);
            v.i_pooling_enable = coin(0.3); v.i_pooling_mode = irand(0, 3); v.i_pooling_stride = urand(1, 4); v.i_pooling_num_w_tile = urand(1, 4);
            v.i_pooling_c = urand(1, 4); v.i_pooling_oh = urand(1, 4); v.i_pooling_c_move = urand(0, 200); v.i_pooling_h_move = urand(0, 20);
            v.i_pooling_rdata = SpBytes(32);
            for (auto &x : v.i_pooling_rdata) x = static_cast<uint8_t>(urand(0, 255));
            v.i_pooling_valid = coin(0.3) ? urand(0, 0xFFFFFFFFu) : 0u;
            const GvuVlsu::Out o = ref.eval(v), oa = a.eval(v), ob = b.eval(v);
            if (!same(o, oa) || !same(o, ob))
            {
                if (bad_a < 5) std::printf("  T8a bad: cycle %d\n", c);
                bad_a++;
            }
            ref.tick(v); a.tick(v); b.tick(v);
        }
    }
    // (b) vbank0: source in the even phases (reads), destination in the odd phases (writes)
    const ScratchpadParams P;
    const int aw = P.VBANK0_AW();
    uint64_t acc = 0, bad_b = 0, switches = 0, bad_base = 0;
    for (int t = 0; t < 2000; t++)
    {
        GvuVlsuNdPort u;
        AguNdCtrl c;
        c.src_port = AguNdCtrl::VBANK0; c.src_on = true; c.dst_port = AguNdCtrl::VBANK0; c.dst_on = true;
        AguNdDesc d[2];
        for (int j = 0; j < 2; j++)
        {
            d[j].base = urand(0, (1u << aw) - 1u);
            for (int k = 0; k < 4; k++) { d[j].n[k] = urand(1, 6); d[j].s[k] = irand(-60, 60); }
        }
        const int phases = static_cast<int>(urand(2, 6));
        auto cycle = [&](bool reset, int pick, bool rd, bool wr) {
            GvuVlsu::In v;
            v.i_agu_addr_reset = reset;
            v.i_vbank0_agu_mode = AGU_ND; v.i_vbank0_rd = rd; v.i_vbank0_wr = wr; v.i_vbank0_lane_wmask = 0xFFFFFFFFu;
            u.bind(c, d[0], d[1], {{0, 0, static_cast<uint8_t>(pick + 1), 0, 0}});
            const unsigned a = u.eval(v).o_vbank0_addr;
            u.tick(v);
            return a;
        };
        // first cycle: i_agu_addr_reset with the pick of phase 0 (IDLE -> RESIDUAL_ST1), no access
        cycle(true, 0, false, false);
        for (int ph = 0; ph < phases; ph++)
        {
            const int j = ph & 1;
            const uint64_t L = d[j].length();
            for (uint64_t k = 0; k < L;)
            {
                const bool go = !coin(0.3);
                const bool last = go && k + 1 == L;
                // the switch to the next phase: in the cycle of the last access (odd programs) or in an idle cycle after it (even)
                const bool sw_now = last && ph + 1 < phases && (t & 1);
                const unsigned a = cycle(sw_now, sw_now ? (j ^ 1) : j, go && j == 0, go && j == 1);
                if (go)
                {
                    const unsigned want = d[j].addr_at(k, aw);
                    if (a != want)
                    {
                        if (bad_b < 5) std::printf("  T8b bad: prog %d phase %d access %llu addr %u want %u\n", t, ph, (unsigned long long)k, a, want);
                        bad_b++;
                        if (k == 0) bad_base++;
                    }
                    k++; acc++;
                }
                if (sw_now) switches++;
            }
            if (ph + 1 < phases && !(t & 1)) { cycle(true, j ^ 1, false, false); switches++; }
        }
    }
    std::printf("T8 GvuVlsuNdPort: (a) unbound / bound with AGU_ND_CTRL off = GvuVlsu: %llu cycles, %llu differ; (b) vbank0 source / destination by sel: %llu accesses, %llu phase switches, %llu differ (%llu at the first access of a phase)\n",
                (unsigned long long)cyc, (unsigned long long)bad_a, (unsigned long long)acc, (unsigned long long)switches, (unsigned long long)bad_b, (unsigned long long)bad_base);
    return (bad_a || bad_b) ? 1 : 0;
}

int main()
{
    const int f = t5() + t6() + t7() + t8();
    std::printf("RESULT: %s (%d of 4 tests failed)\n", f ? "FAIL" : "PASS", f);
    return f ? 1 : 0;
}
