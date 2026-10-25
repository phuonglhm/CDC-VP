// tb_gvu_agu_nd.cpp -- unit tests of the AGU_ND address mode (has/gvu_agu_nd.h, has/gvu_agu_nd_vlsu.h). Pure C++17.
//   T1 random descriptors: address of every access = nested-loop reference (mod 2^AW), o_last on the last access of each
//      program, back to base (stop = 0) or kept (stop = 1), idle cycles keep the address
//   T2 degenerate programs through GvuVlsuNd = the frozen GvuVlsu in AGU_INCR / AGU_FIXED, every port, every cycle
//   T3 the pooling program = the vbank0 address of the frozen GvuVlsu in AGU_POOLING at every read of the pooling feeder
//   T4 register packing of the proposal (Reg 16..24): unpack(pack(x)) = x
//   T1b the closed form AguNdDesc::addr_at(k) = the address of access k of GvuAguNd
// Build (repository root): g++ -std=c++17 -O2 -Wall -Wextra -I. tools/has/tb_gvu_agu_nd.cpp -o tb_gvu_agu_nd
#include <algorithm>
#include <cstdio>
#include <random>
#include <vector>
#include "has/gvu_agu_nd.h"
#include "has/gvu_agu_nd_vlsu.h"

using namespace rtl_ref_gvu;

static std::mt19937 rng(20261021u);
static unsigned urand(unsigned lo, unsigned hi) { return std::uniform_int_distribution<unsigned>(lo, hi)(rng); }
static int irand(int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); }
static bool coin(double p) { return std::bernoulli_distribution(p)(rng); }

static std::vector<unsigned> reference(const AguNdDesc &d, int aw)
{
    std::vector<unsigned> a;
    const unsigned m = (1u << aw) - 1u;
    for (unsigned i3 = 0; i3 < d.n[3]; i3++)
        for (unsigned i2 = 0; i2 < d.n[2]; i2++)
            for (unsigned i1 = 0; i1 < d.n[1]; i1++)
                for (unsigned i0 = 0; i0 < d.n[0]; i0++)
                    a.push_back(static_cast<unsigned>(static_cast<int64_t>(d.base) + int64_t(i0) * d.s[0] + int64_t(i1) * d.s[1] +
                                                      int64_t(i2) * d.s[2] + int64_t(i3) * d.s[3]) & m);
    return a;
}

static int t1()
{
    uint64_t acc = 0, bad = 0, progs = 0;
    const int aws[3] = {7, 10, 12};
    for (int t = 0; t < 2000; t++)
    {
        const int aw = aws[t % 3];
        AguNdDesc d;
        d.base = urand(0, (1u << aw) - 1u);
        for (int k = 0; k < 4; k++) { d.n[k] = urand(1, 12); d.s[k] = irand(-300, 300); }
        const bool stop = (t % 5) == 4;
        const std::vector<unsigned> ref = reference(d, aw);
        const uint64_t L = ref.size(), total = stop ? L + 7 : 2 * L + urand(0, unsigned(L));
        GvuAguNd u(aw);
        GvuAguNd::In in;
        in.d = d; in.stop = stop; in.i_reset = true;
        u.tick(in);
        in.i_reset = false;
        uint64_t k = 0;
        while (k < total)
        {
            in.i_req = coin(0.7);
            const GvuAguNd::Out o = u.eval(in);
            if (in.i_req)
            {
                const uint64_t j = stop ? std::min<uint64_t>(k, L - 1) : k % L;
                const bool last = stop ? k >= L - 1 : j == L - 1;
                if (o.o_addr != ref[j] || o.o_last != last)
                {
                    if (bad < 5) std::printf("  T1 bad: prog %d access %llu addr %u ref %u last %d/%d\n", t, (unsigned long long)k, o.o_addr, ref[j], o.o_last, last);
                    bad++;
                }
                k++; acc++;
            }
            u.tick(in);
        }
        progs++;
    }
    std::printf("T1 random descriptors: %llu programs, %llu accesses, %llu differ\n", (unsigned long long)progs, (unsigned long long)acc, (unsigned long long)bad);
    return bad ? 1 : 0;
}

// T2: frozen GvuVlsu (AGU_INCR / AGU_FIXED) against GvuVlsuNd with the degenerate programs, two ports bound per round.
static int t2()
{
    const ScratchpadParams P;
    const int aw[5] = {P.SBANK_AW(), P.SBANK_AW() + 2, P.VBANK0_AW(), P.VBANK1_AW() + 2, P.VBANK2_AW() + 2};
    uint64_t cyc = 0, bad = 0;
    for (int round = 0; round < 400; round++)
    {
        GvuVlsu a(P);
        GvuVlsuNd b(P);
        const unsigned pa = urand(0, 4), pb = (pa + urand(1, 4)) % 5;
        const bool incr_a = coin(0.5), incr_b = coin(0.5);
        GvuVlsuNd::In ib;
        ib.ctrl.src_port = pa; ib.ctrl.src_on = true; ib.ctrl.dst_port = pb; ib.ctrl.dst_on = true; ib.ctrl.idle_reload = true;
        auto prog = [&](bool incr, unsigned port, unsigned base) {
            AguNdDesc d;
            d.base = base;
            if (incr)
            {
                const unsigned len = 1u << aw[port];
                d.n = {{std::min(len, 1024u), std::max(len / 1024u, 1u), 1, 1}};
                d.s = {{1, 1024, 0, 0}};
            }
            return d;
        };
        unsigned st[5] = {0, 0, 0, 0, 0};
        for (int c = 0; c < 500; c++, cyc++)
        {
            GvuVlsu::In v;
            for (unsigned p = 0; p < 5; p++) if (coin(0.1) || c == 0) st[p] = urand(0, (1u << aw[p]) - 1u);
            v.i_agu_addr_reset = coin(0.02);
            v.i_sbank_rd_0 = coin(0.6); v.i_sbank_start_addr_0 = st[0];
            v.i_sbank_rd_1 = coin(0.6); v.i_sbank_start_addr_1 = st[1];
            v.i_vbank0_rd = coin(0.6); v.i_vbank0_start_addr = st[2];
            v.i_vbank1_wr = coin(0.6); v.i_vbank1_port_sel = urand(0, 2); v.i_vbank1_start_addr = st[3];
            v.i_vbank2_wr = coin(0.6); v.i_vbank2_port_sel = coin(0.5); v.i_vbank2_start_addr = st[4];
            int *ma[5] = {&v.i_sbank_agu_mode_0, &v.i_sbank_agu_mode_1, &v.i_vbank0_agu_mode, &v.i_vbank1_agu_mode, &v.i_vbank2_agu_mode};
            *ma[pa] = incr_a ? AGU_INCR : AGU_FIXED;
            *ma[pb] = incr_b ? AGU_INCR : AGU_FIXED;
            ib.v = v;
            int *mb[5] = {&ib.v.i_sbank_agu_mode_0, &ib.v.i_sbank_agu_mode_1, &ib.v.i_vbank0_agu_mode, &ib.v.i_vbank1_agu_mode, &ib.v.i_vbank2_agu_mode};
            *mb[pa] = AGU_ND; *mb[pb] = AGU_ND;
            ib.src = prog(incr_a, pa, st[pa]);
            ib.dst = prog(incr_b, pb, st[pb]);
            const GvuVlsu::Out oa = a.eval(v), ob = b.eval(ib);
            const bool same = oa.o_sbank_addr_0 == ob.o_sbank_addr_0 && oa.o_sbank_addr_1 == ob.o_sbank_addr_1 && oa.o_vbank0_addr == ob.o_vbank0_addr &&
                              oa.o_vbank1_addr_0 == ob.o_vbank1_addr_0 && oa.o_vbank1_addr_1 == ob.o_vbank1_addr_1 && oa.o_vbank1_addr_2 == ob.o_vbank1_addr_2 &&
                              oa.o_vbank2_addr_0 == ob.o_vbank2_addr_0 && oa.o_vbank2_addr_1 == ob.o_vbank2_addr_1 &&
                              oa.o_sbank_en_0 == ob.o_sbank_en_0 && oa.o_vbank0_en == ob.o_vbank0_en && oa.o_vbank1_en_2 == ob.o_vbank1_en_2 && oa.o_vbank2_en_1 == ob.o_vbank2_en_1;
            if (!same)
            {
                if (bad < 5) std::printf("  T2 bad: round %d cycle %d ports %u/%u incr %d/%d\n", round, c, pa, pb, incr_a, incr_b);
                bad++;
            }
            a.tick(v);
            b.tick(ib);
        }
    }
    // one long INCR burst with a constant start on every port width (wraps at 2^AW)
    for (unsigned p = 0; p < 5; p++)
    {
        GvuVlsu a(P);
        GvuVlsuNd b(P);
        GvuVlsuNd::In ib;
        ib.ctrl.src_port = p; ib.ctrl.src_on = true; ib.ctrl.idle_reload = true;
        const unsigned len = 1u << aw[p], base = urand(0, len - 1u);
        ib.src.base = base; ib.src.n = {{std::min(len, 1024u), std::max(len / 1024u, 1u), 1, 1}}; ib.src.s = {{1, 1024, 0, 0}};
        for (unsigned c = 0; c < 3 * len; c++, cyc++)
        {
            GvuVlsu::In v;
            v.i_agu_addr_reset = c == 0;
            if (p == 0) { v.i_sbank_rd_0 = c > 0; v.i_sbank_start_addr_0 = base; }
            if (p == 1) { v.i_sbank_rd_1 = c > 0; v.i_sbank_start_addr_1 = base; }
            if (p == 2) { v.i_vbank0_rd = c > 0; v.i_vbank0_start_addr = base; }
            if (p == 3) { v.i_vbank1_rd = c > 0; v.i_vbank1_port_sel = 2; v.i_vbank1_start_addr = base; }
            if (p == 4) { v.i_vbank2_rd = c > 0; v.i_vbank2_port_sel = true; v.i_vbank2_start_addr = base; }
            ib.v = v;
            int *mb[5] = {&ib.v.i_sbank_agu_mode_0, &ib.v.i_sbank_agu_mode_1, &ib.v.i_vbank0_agu_mode, &ib.v.i_vbank1_agu_mode, &ib.v.i_vbank2_agu_mode};
            *mb[p] = AGU_ND;
            const GvuVlsu::Out oa = a.eval(v), ob = b.eval(ib);
            const unsigned xa[5] = {oa.o_sbank_addr_0, oa.o_sbank_addr_1, oa.o_vbank0_addr, oa.o_vbank1_addr_2, oa.o_vbank2_addr_1};
            const unsigned xb[5] = {ob.o_sbank_addr_0, ob.o_sbank_addr_1, ob.o_vbank0_addr, ob.o_vbank1_addr_2, ob.o_vbank2_addr_1};
            if (xa[p] != xb[p]) { if (bad < 5) std::printf("  T2 long burst bad: port %u cycle %u %u/%u\n", p, c, xa[p], xb[p]); bad++; }
            a.tick(v);
            b.tick(ib);
        }
    }
    std::printf("T2 INCR / FIXED through GvuVlsuNd = frozen GvuVlsu: %llu cycles, %llu differ\n", (unsigned long long)cyc, (unsigned long long)bad);
    return bad ? 1 : 0;
}

// T3: AGU_POOLING read address of vbank0 = AGU_ND program.
static int t3()
{
    const ScratchpadParams P;
    const struct { int mode, k, s; } ks[7] = {{POOLING_2X2, 2, 1}, {POOLING_2X2, 2, 2}, {POOLING_3X3, 3, 1}, {POOLING_4X4, 4, 1},
                                             {POOLING_4X4, 4, 2}, {POOLING_4X4, 4, 4}, {POOLING_5X5, 5, 1}};
    uint64_t reads = 0, want_reads = 0, bad = 0, runs = 0, unfinished = 0;
    for (const auto &q : ks)
        for (unsigned wt = 1; wt <= 3; wt++)
            for (unsigned oh : {1u, 3u, 6u})
                for (unsigned C : {1u, 2u, 5u})
                {
                    const unsigned hin = (oh - 1u) * unsigned(q.s) + unsigned(q.k);
                    GvuVlsu v(P);
                    v.reset_pooling();
                    GvuVlsu::In in;
                    in.i_pooling_enable = true;
                    in.i_pooling_mode = q.mode; in.i_pooling_stride = unsigned(q.s);
                    in.i_pooling_num_w_tile = wt; in.i_pooling_c = C; in.i_pooling_oh = oh;
                    in.i_pooling_h_move = wt * unsigned(q.s); in.i_pooling_c_move = wt * hin;
                    in.i_vbank0_agu_mode = AGU_POOLING; in.i_vbank2_agu_mode = AGU_POOLING; in.i_vbank2_port_sel = true;
                    in.i_pooling_rdata.assign(32, 0);
                    GvuAguNd nd(P.VBANK0_AW());
                    GvuAguNd::In ni;
                    ni.d.base = 0;
                    ni.d.n = {{unsigned(q.k), wt, oh, C}};
                    ni.d.s = {{int(wt), 1, int(wt) * q.s, int(wt * hin)}};
                    ni.i_reset = true; nd.tick(ni); ni.i_reset = false;
                    const uint64_t want = uint64_t(C) * oh * wt * unsigned(q.k);
                    uint64_t n = 0;
                    bool done = false;
                    for (int c = 0; c < 200000 && !done; c++)
                    {
                        for (auto &b : in.i_pooling_rdata) b = static_cast<uint8_t>(urand(0, 255));
                        const GvuVlsu::Out o = v.eval(in);
                        ni.i_req = o.o_vbank0_en;
                        if (o.o_vbank0_en)
                        {
                            const unsigned a = nd.eval(ni).o_addr;
                            if (a != o.o_vbank0_addr) { if (bad < 5) std::printf("  T3 bad: k%d s%d wt %u oh %u C %u read %llu: %u / %u\n", q.k, q.s, wt, oh, C, (unsigned long long)n, o.o_vbank0_addr, a); bad++; }
                            n++;
                        }
                        done = o.o_pooling_last_wr;
                        v.tick(in);
                        nd.tick(ni);
                    }
                    if (!done) unfinished++;
                    if (n != want) { if (bad < 5) std::printf("  T3 count: k%d s%d wt %u oh %u C %u reads %llu want %llu\n", q.k, q.s, wt, oh, C, (unsigned long long)n, (unsigned long long)want); bad++; }
                    reads += n; want_reads += want; runs++;
                }
    std::printf("T3 AGU_POOLING = AGU_ND program: %llu runs, %llu reads (want %llu), %llu runs without last write, %llu differ\n",
                (unsigned long long)runs, (unsigned long long)reads, (unsigned long long)want_reads, (unsigned long long)unfinished, (unsigned long long)bad);
    return (bad || unfinished) ? 1 : 0;
}

static int t4()
{
    uint64_t bad = 0;
    for (int t = 0; t < 10000; t++)
    {
        AguNdDesc d;
        d.base = urand(0, 4095);
        for (int k = 0; k < 4; k++) { d.n[k] = urand(1, 1024); d.s[k] = irand(-32768, 32767); }
        const AguNdDesc e = AguNdRegs::unpack(AguNdRegs::pack(d));
        if (e.base != d.base || e.n != d.n || e.s != d.s) bad++;
        AguNdCtrl c;
        c.src_port = urand(0, 4); c.dst_port = urand(0, 4); c.src_on = coin(0.5); c.dst_on = coin(0.5); c.idle_reload = coin(0.5); c.stop = coin(0.5);
        const AguNdCtrl f = AguNdRegs::ctrl(AguNdRegs::ctrl(c));
        if (f.src_port != c.src_port || f.dst_port != c.dst_port || f.src_on != c.src_on || f.dst_on != c.dst_on || f.idle_reload != c.idle_reload || f.stop != c.stop) bad++;
        if (AguNdRegs::ctrl(c) >> 10) bad++;
    }
    std::printf("T4 register packing (Reg %u..%u): 10000 descriptors, %llu differ\n", AguNdRegs::CTRL, AguNdRegs::DST + 3, (unsigned long long)bad);
    return bad ? 1 : 0;
}

// T1b: the closed form AguNdDesc::addr_at(k) = the address GvuAguNd gives at access k (requests every cycle, stop = 0).
static int t1b()
{
    uint64_t acc = 0, bad = 0;
    const int aws[3] = {7, 10, 12};
    for (int t = 0; t < 2000; t++)
    {
        const int aw = aws[t % 3];
        AguNdDesc d;
        d.base = urand(0, (1u << aw) - 1u);
        for (int k = 0; k < 4; k++) { d.n[k] = urand(1, 12); d.s[k] = irand(-300, 300); }
        const uint64_t total = std::min<uint64_t>(2ull * d.length(), 20000);
        GvuAguNd u(aw);
        GvuAguNd::In in;
        in.d = d; in.i_reset = true;
        u.tick(in);
        in.i_reset = false; in.i_req = true;
        for (uint64_t k = 0; k < total; k++, acc++)
        {
            const unsigned a = u.eval(in).o_addr, b = d.addr_at(k, aw);
            if (a != b)
            {
                if (bad < 5) std::printf("  T1b bad: prog %d access %llu addr %u addr_at %u\n", t, (unsigned long long)k, a, b);
                bad++;
            }
            u.tick(in);
        }
    }
    std::printf("T1b addr_at(k) = GvuAguNd: 2000 programs, %llu accesses, %llu differ\n", (unsigned long long)acc, (unsigned long long)bad);
    return bad ? 1 : 0;
}

int main()
{
    const int f = t1() + t2() + t3() + t4() + t1b();
    std::printf("RESULT: %s (%d of 5 tests failed)\n", f ? "FAIL" : "PASS", f);
    return f ? 1 : 0;
}
