// tb_gvu_top_stream.cpp -- unit test of the clocked connection: PsmStreamTap + GvuTopStream (has/gvu_top_stream.h)
// without the core. A driver plays the PSM write hook (calls PsmStreamTap::push in the clock edge) with packets of random psums,
// lane masks, channels with their own Out_Scale / Out_Shift, over four layers (a new activation table each, one without
// activation). Expected values: the call-style connection (HAS_GVU_TOP=1, has::GvuTopBridge::gemm_beat). Checks:
//   - every result equal to the call-style result, written back with the right address and mask
//   - timing along the RTL: packet in the FIFO at edge E (valid in the cycle after E), latched by GvuTop at E+1, result at
//     E+1+kDepth (E+10 at the default depth 9; build with -DGVU_GEMM_STREAM_DEPTH=11 for the depth of the hardware RTL)
//   - the same trace when the driver process is created before / after the tap (argv[1] = before | after; the caller compares)
//   - FIFO: layer 3 holds i_psum_ready low for 8 edges while 8 packets arrive one per edge -> stall_needed 4, queue max 8
//   - negative check: one packet carries a wrong model lane -> exactly one difference counted
//   - a beat that GvuTop cannot express (bypass) comes back as the model result at the same latency
//   - residual add on the same unit against has::GvuTopBridge::add (3 sizes), values and cycles
// build: g++ -std=c++17 -O2 -I. tools/has/tb_gvu_top_stream.cpp -lsystemc -o tb_gvu_top_stream
#include <systemc.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>
#include "has/gvu_top_bridge.h"
#include "has/gvu_top_stream.h"

using namespace has;
using sauria::psum_vector_t;

static std::mt19937_64 g_rng(7);
static long long uni(long long lo, long long hi) { return std::uniform_int_distribution<long long>(lo, hi)(g_rng); }
static int g_errors = 0;
static void check(bool ok, const char *what)
{
    if (!ok) { g_errors++; std::printf("  FAIL: %s\n", what); }
}

struct Exp { uint32_t addr, mask, ch; psum_vector_t<32, int32_t> out; bool bypass; };
struct Got { uint64_t edge; uint32_t addr, mask; psum_vector_t<32, int32_t> v; };
struct Layer
{
    bool has_lut; int8_t lut[256]; int zp; int round_mode; bool act;
    std::vector<StreamPacket> pk;
    int hold_edges;
};

struct Driver : sc_module
{
    sc_in<bool> clk{"clk"};
    PsmStreamTap *tap{nullptr};
    std::vector<std::vector<StreamPacket>> plan;   // plan[e] = packets pushed at edge e
    uint64_t edge{0};
    bool seen{false}, ran_after_tap{false};
    SC_CTOR(Driver) { SC_METHOD(proc); sensitive << clk.pos(); dont_initialize(); }
    void proc()
    {
        edge++;
        if (!seen) { seen = true; ran_after_tap = tap->edge() == edge; }   // tap->edge() counts the same edges: equal = the tap ran first
        if (edge < plan.size())
            for (const StreamPacket &p : plan[edge]) tap->push(p);
    }
};

struct Dfc : sc_module
{
    sc_in<bool> clk{"clk"};
    sc_out<bool> rstn{"rstn"};
    std::vector<Layer> *L{nullptr};
    Driver *drv{nullptr};
    GvuTopStream *gs{nullptr};
    PsmStreamTap *tap{nullptr};
    SC_CTOR(Dfc) { SC_THREAD(th); sensitive << clk.pos(); }
    void th()
    {
        rstn.write(false);
        wait(5);
        rstn.write(true);
        wait(3);
        for (Layer &ly : *L)
        {
            gs->begin_layer(ly.has_lut ? ly.lut : nullptr, ly.zp, ly.round_mode, ly.act);
            int guard = 0;
            while (!gs->ready_for_beats() && guard++ < 100) wait();
            const uint64_t X = drv->edge;
            drv->plan.assign(X + ly.pk.size() + 12, std::vector<StreamPacket>());
            if (ly.hold_edges)
            {
                // ready low in the edges [H, H + hold); the packets reach the FIFO at the edges H+1 .. H+n (all before a pop can happen)
                const uint64_t H = X + 3;
                gs->hold_from = H; gs->hold_until = H + uint64_t(ly.hold_edges);
                for (size_t k = 0; k < ly.pk.size(); k++) drv->plan[H + 1 + k].push_back(ly.pk[k]);
            }
            else
                for (size_t k = 0; k < ly.pk.size(); k++) drv->plan[X + 2 + k].push_back(ly.pk[k]);
            guard = 0;
            while (!(drv->edge >= drv->plan.size() && gs->drained(*tap)) && guard++ < 3000) wait();
            gs->end_layer();
            guard = 0;
            while (!gs->layer_closed() && guard++ < 200) wait();
            for (int k = 0; k < 70; k++) wait();      // the gap between two chains of writes is more than 64 cycles (U3)
        }
        sc_stop();
    }
};

static std::vector<Layer> g_layers;
static std::vector<Exp> g_exp;
static std::vector<Got> g_got;

static void make_layers()
{
    for (int li = 0; li < 4; li++)
    {
        Layer ly{};
        ly.has_lut = (li != 2);
        ly.act = ly.has_lut;
        for (int e = 0; e < 256; e++) ly.lut[e] = static_cast<int8_t>(ly.has_lut ? (li % 2 ? std::max(e - 128, 0) : (e - 128) / 2 + li) : 0);
        ly.zp = static_cast<int>(uni(-20, 20));
        ly.round_mode = (li * 3) % 7;
        const int n = li == 0 ? 64 : (li == 3 ? 8 : 20);
        for (int k = 0; k < n; k++)
        {
            StreamPacket p;
            const int ch = k % 5;
            p.ch = uint32_t(ch); p.addr = uint32_t(1000 + li * 200 + k);
            p.mask = (k % 7 == 3) ? 0x0000FFFFu : ((k % 11 == 5) ? 0x00F00F00u : 0xFFFFFFFFu);
            p.S = uni(1, 1 << 14); p.s = int(uni(8, 24));
            for (int l = 0; l < 32; l++) p.data[size_t(l)] = ((p.mask >> l) & 1u) ? int32_t(uni(-300000, 300000)) : 0;
            p.oc_last = (ch == 4); p.oc_tile_last = (li == 3);
            p.bypass = (li == 1 && k == 10);
            ly.pk.push_back(p);
        }
        ly.hold_edges = (li == 3) ? 8 : 0;
        g_layers.push_back(ly);
    }
    for (size_t li = 0; li < g_layers.size(); li++)
    {
        Layer &ly = g_layers[li];
        bool table_new = true;
        for (size_t k = 0; k < ly.pk.size(); k++)
        {
            StreamPacket &p = ly.pk[k];
            psum_vector_t<32, int32_t> out;
            if (p.bypass)
            {
                for (int l = 0; l < 32; l++) out[size_t(l)] = ((p.mask >> l) & 1u) ? int32_t(uni(-128, 127)) : 0;
                p.model = out;
                g_exp.push_back(Exp{p.addr, p.mask, p.ch, out, true});
                continue;
            }
            const bool ok = GvuTopBridge::gemm_beat(p.data, out, p.mask, p.addr, p.S, p.s, ly.zp, ly.round_mode, true, ly.act, ly.lut, table_new);
            table_new = false;
            if (!ok) std::printf("  (setup: the call-style beat did not run)\n");
            p.model = out;
            g_exp.push_back(Exp{p.addr, p.mask, p.ch, out, false});
            if (li == 0 && k == 2) p.model[0] += 1;      // negative check: a wrong model lane, expected result unchanged
        }
    }
}

static int add_checks(GvuTopStream &gs)
{
    int bad = 0;
    static const size_t sizes[3] = {3000, 5600, 4097};
    for (size_t t = 0; t < 3; t++)
    {
        const size_t n = sizes[t];
        std::vector<int8_t> A(n), B(n), o1(n), o3(n);
        for (size_t i = 0; i < n; i++) { A[i] = int8_t(uni(-128, 127)); B[i] = int8_t(uni(-128, 127)); o1[i] = 0; o3[i] = 0; }
        const int zpA = int(uni(-8, 8)), zpB = int(uni(-8, 8)), zpO = int(uni(-8, 8));
        const int64_t SA = uni(1 << 12, 1 << 16), SB = uni(1 << 12, 1 << 16), SO = uni(1 << 12, 1 << 16);
        const int sA = 14, sB = 14, sO = 16;
        uint64_t c1 = 0, c3 = 0;
        const bool r1 = GvuTopBridge::add(A.data(), B.data(), o1.data(), n, zpA, zpB, zpO, SA, SB, SO, sA, sB, sO, 1, true, true, 0, &c1);
        const bool r3 = gs.add(A.data(), B.data(), o3.data(), n, zpA, zpB, zpO, SA, SB, SO, sA, sB, sO, 1, true, true, 0, &c3);
        size_t d = 0;
        for (size_t i = 0; i < n; i++) if (o1[i] != o3[i]) d++;
        std::printf("[stream-add] add %zu elements: call-style %s %llu cycles, on the stream unit %s %llu cycles, %zu elements differ\n", n,
                    r1 ? "ran" : "refused", (unsigned long long)c1, r3 ? "ran" : "refused", (unsigned long long)c3, d);
        if (!r1 || !r3 || d || c1 != c3) bad++;
    }
    return bad;
}

// ---- mode "elem": residual add and max pool as jobs on the clock (HAS_GVU_ELEM_CLK=1) against the call style on the same unit
struct ElemCase { bool add; size_t n; int g, H, W, kk, s, p; uint64_t side; };
struct ElemTester : sc_module
{
    sc_in<bool> clk;
    sc_signal<bool> *rstn{nullptr};
    GvuTopStream *gs{nullptr};
    std::vector<ElemCase> cases;
    int bad{0};
    SC_HAS_PROCESS(ElemTester);
    explicit ElemTester(sc_module_name n) : sc_module(n) { SC_THREAD(run); sensitive << clk.pos(); }
    static uint64_t edge_now() { return uint64_t(sc_time_stamp().value() / 10000); }   // 10 ns clock, 1 ps resolution
    void run()
    {
        rstn->write(false);
        wait(3);
        rstn->write(true);
        wait(2);
        for (size_t t = 0; t < cases.size(); t++)
        {
            const ElemCase &e = cases[t];
            std::vector<int8_t> A, B, o1, o2;
            int zpA = 0, zpB = 0, zpO = 0, sA = 14, sB = 14, sO = 16;
            int64_t SA = 1, SB = 1, SO = 1;
            size_t nout = e.n;
            if (e.add)
            {
                A.resize(e.n); B.resize(e.n);
                for (size_t i = 0; i < e.n; i++) { A[i] = int8_t(uni(-128, 127)); B[i] = int8_t(uni(-128, 127)); }
                zpA = int(uni(-8, 8)); zpB = int(uni(-8, 8)); zpO = int(uni(-8, 8));
                SA = uni(1 << 12, 1 << 16); SB = uni(1 << 12, 1 << 16); SO = uni(1 << 12, 1 << 16);
            }
            else
            {
                A.resize(size_t(e.g) * e.H * e.W);
                for (auto &v : A) v = int8_t(uni(-128, 127));
                nout = size_t(e.g) * size_t((e.H + 2 * e.p - e.kk) / e.s + 1) * size_t((e.W + 2 * e.p - e.kk) / e.s + 1);
            }
            o1.assign(nout, 0); o2.assign(nout, 0);
            const StreamStats before = stream_stats();
            uint64_t c1 = 0;
            const bool fits = e.add ? GvuTopStream::add_fits(zpA, zpB, zpO, SA, SB, SO, sA, sB, sO, 1, true, true)
                                    : GvuTopStream::maxpool_fits(e.g, e.H, e.W, e.kk, e.s, e.p);
            const bool r1 = e.add ? gs->add(A.data(), B.data(), o1.data(), e.n, zpA, zpB, zpO, SA, SB, SO, sA, sB, sO, 1, true, true, 0, &c1)
                                  : gs->maxpool(A.data(), o1.data(), e.g, e.H, e.W, e.kk, e.s, e.p, 0, &c1);
            const uint64_t runs1 = stream_stats().mp_runs - before.mp_runs, resets1 = stream_stats().resets - before.resets;
            // the job: requested at this edge, the operands overwritten right after the request (the job must use its copies)
            const StreamStats mid = stream_stats();
            const uint64_t t0 = edge_now();
            if (e.add) gs->start_add(A.data(), B.data(), o2.data(), e.n, zpA, zpB, zpO, SA, SB, SO, sA, sB, sO, 1, 0);
            else gs->start_maxpool(A.data(), o2.data(), e.g, e.H, e.W, e.kk, e.s, e.p, 0);
            for (auto &v : A) v = 0;
            for (auto &v : B) v = 0;
            if (e.side) wait(int(e.side));
            while (!gs->job_done()) wait(gs->job_event());
            const uint64_t elapsed = edge_now() - t0, c2 = gs->job_cycles();
            const uint64_t runs2 = stream_stats().mp_runs - mid.mp_runs, resets2 = stream_stats().resets - mid.resets;
            size_t d = 0;
            for (size_t i = 0; i < nout; i++) if (o1[i] != o2[i]) d++;
            const uint64_t want = std::max(c1, e.side);
            const bool ok = fits && r1 && d == 0 && c2 == c1 && elapsed == want && runs1 == runs2 && resets1 == resets2;
            if (e.add)
                std::printf("[elem] add %5zu elements (%3zu rows): call style %llu cycles, job %llu cycles, thread waited %llu edges (side %llu, expected %llu), "
                            "%zu elements differ %s\n", e.n, (e.n + 31) / 32, (unsigned long long)c1, (unsigned long long)c2, (unsigned long long)elapsed,
                            (unsigned long long)e.side, (unsigned long long)want, d, ok ? "ok" : "FAILED");
            else
                std::printf("[elem] max pool k%d s%d p%d %3d x %2d x %2d: call style %llu cycles %llu runs %llu resets, job %llu cycles %llu runs %llu resets, "
                            "thread waited %llu edges (side %llu, expected %llu), %zu elements differ %s\n", e.kk, e.s, e.p, e.g, e.H, e.W,
                            (unsigned long long)c1, (unsigned long long)runs1, (unsigned long long)resets1, (unsigned long long)c2, (unsigned long long)runs2,
                            (unsigned long long)resets2, (unsigned long long)elapsed, (unsigned long long)e.side, (unsigned long long)want, d, ok ? "ok" : "FAILED");
            if (!ok) bad++;
        }
        sc_stop();
    }
};

static int elem_main()
{
    std::printf("== residual add and max pool as jobs on the clock of GvuTopStream (HAS_GVU_ELEM_CLK=1) against the call style on the same unit\n");
    sc_set_time_resolution(1, SC_PS);
    sc_clock clk("clk", 10, SC_NS);
    sc_signal<bool> rstn{"rstn"}, ready, valid, oc_last, oc_tile_last, empty, stall, bypass;
    sc_signal<psum_vector_t<32, int32_t>> data, model;
    sc_signal<uint32_t> s_addr, s_mask, s_ch;
    sc_signal<int64_t> s_scale;
    sc_signal<int32_t> s_shift;
    sc_signal<uint64_t> s_push;
    GvuTopStream *gs = new GvuTopStream("gvu");
    gs->i_clk(clk); gs->i_rstn(rstn); gs->i_psum_valid(valid); gs->i_psum_data(data); gs->i_oc_last(oc_last); gs->i_oc_tile_last(oc_tile_last);
    gs->i_psum_empty(empty); gs->i_stream_stall(stall); gs->o_psum_ready(ready);
    gs->i_side_addr(s_addr); gs->i_side_mask(s_mask); gs->i_side_ch(s_ch); gs->i_side_scale(s_scale); gs->i_side_shift(s_shift);
    gs->i_side_bypass(bypass); gs->i_side_model(model); gs->i_side_push(s_push);
    ElemTester *t = new ElemTester("tester");
    t->clk(clk); t->rstn = &rstn; t->gs = gs;
    // residual add: one run, a partial last row, 128 rows (one full run), several runs; side DMA shorter and longer than the job
    t->cases = {{true, 27, 0, 0, 0, 0, 0, 0, 0},       {true, 7 * 32, 0, 0, 0, 0, 0, 0, 0},  {true, 128 * 32, 0, 0, 0, 0, 0, 0, 0},
                {true, 300 * 32 - 5, 0, 0, 0, 0, 0, 0, 0}, {true, 1000 * 32, 0, 0, 0, 0, 0, 0, 0},
                {true, 300 * 32 - 5, 0, 0, 0, 0, 0, 0, 100}, {true, 300 * 32 - 5, 0, 0, 0, 0, 0, 0, 5000},
    // max pool (several runs of cmax channels, a reset before each): k5 s1 p2 as SPPF, k3 s1 p1, k2 s2 p0; side shorter and longer
                {false, 0, 70, 20, 20, 5, 1, 2, 0}, {false, 0, 20, 40, 40, 3, 1, 1, 0}, {false, 0, 13, 64, 64, 2, 2, 0, 0},
                {false, 0, 70, 20, 20, 5, 1, 2, 1000}, {false, 0, 13, 64, 64, 2, 2, 0, 200000}};
    sc_start();
    const StreamStats &st = stream_stats();
    std::printf("[elem] jobs %llu (failed %llu), ticks %llu, job cycles %llu, ticks after done to IDLE %llu, jobs not started at the edge after the request %llu\n",
                (unsigned long long)st.elem_jobs, (unsigned long long)st.elem_jobs_failed, (unsigned long long)st.elem_ticks,
                (unsigned long long)st.elem_job_cycles, (unsigned long long)st.elem_idle_ticks, (unsigned long long)st.elem_start_late);
    const bool ok = t->bad == 0 && st.elem_jobs == t->cases.size() && !st.elem_jobs_failed && st.elem_ticks == st.elem_job_cycles &&
                    !st.elem_idle_ticks && !st.elem_start_late;
    std::printf("RESULT: %s (cases not as expected: %d of %zu)\n", ok ? "PASS" : "FAIL", t->bad, t->cases.size());
    return ok ? 0 : 1;
}

int sc_main(int argc, char **argv)
{
    if (argc > 1 && std::strcmp(argv[1], "elem") == 0) return elem_main();
#ifdef _WIN32
    _putenv("HAS_GVU_TOP=1");
#else
    setenv("HAS_GVU_TOP", "1", 1);
#endif
    const bool driver_first = !(argc > 1 && std::strcmp(argv[1], "after") == 0);
    std::printf("== stream unit test: PsmStreamTap + GvuTopStream, no core (driver created %s the tap)\n", driver_first ? "before" : "after");
    make_layers();

    sc_set_time_resolution(1, SC_PS);
    sc_clock clk("clk", 10, SC_NS);
    sc_signal<bool> rstn{"rstn"}, ready, valid, oc_last, oc_tile_last, empty, stall, bypass;
    sc_signal<psum_vector_t<32, int32_t>> data, model;
    sc_signal<uint32_t> s_addr, s_mask, s_ch;
    sc_signal<int64_t> s_scale;
    sc_signal<int32_t> s_shift;
    sc_signal<uint64_t> s_push;

    Driver *drv = nullptr;
    auto make_driver = [&]() { drv = new Driver("drv"); drv->clk(clk); };
    if (driver_first) make_driver();
    PsmStreamTap *tap = new PsmStreamTap("tap");
    if (!driver_first) make_driver();
    GvuTopStream *gs = new GvuTopStream("gvu");
    drv->tap = tap;
    tap->i_clk(clk); tap->i_rstn(rstn); tap->i_psum_ready(ready);
    tap->o_psum_valid(valid); tap->o_psum_data(data); tap->o_oc_last(oc_last); tap->o_oc_tile_last(oc_tile_last);
    tap->o_psum_empty(empty); tap->o_stream_stall(stall);
    tap->o_side_addr(s_addr); tap->o_side_mask(s_mask); tap->o_side_ch(s_ch); tap->o_side_scale(s_scale); tap->o_side_shift(s_shift);
    tap->o_side_bypass(bypass); tap->o_side_model(model); tap->o_side_push(s_push);
    gs->i_clk(clk); gs->i_rstn(rstn); gs->i_psum_valid(valid); gs->i_psum_data(data); gs->i_oc_last(oc_last); gs->i_oc_tile_last(oc_tile_last);
    gs->i_psum_empty(empty); gs->i_stream_stall(stall); gs->o_psum_ready(ready);
    gs->i_side_addr(s_addr); gs->i_side_mask(s_mask); gs->i_side_ch(s_ch); gs->i_side_scale(s_scale); gs->i_side_shift(s_shift);
    gs->i_side_bypass(bypass); gs->i_side_model(model); gs->i_side_push(s_push);
    gs->tracing = true;
    gs->result_sink = [&](uint32_t addr, uint32_t mask, const psum_vector_t<32, int32_t> &v)
    {
        Got g; g.edge = uint64_t(sc_time_stamp().value() / 10000); g.addr = addr; g.mask = mask; g.v = v;
        g_got.push_back(g);
    };
    Dfc *dfc = new Dfc("dfc");
    dfc->clk(clk); dfc->rstn(rstn); dfc->L = &g_layers; dfc->drv = drv; dfc->gs = gs; dfc->tap = tap;

    sc_start();

    const StreamStats &st = stream_stats();
    std::printf("[stream] order of the processes at the first edge: the driver (PSM hook) ran %s the tap's edge process\n", drv->ran_after_tap ? "AFTER" : "BEFORE");
    check(g_got.size() == g_exp.size(), "number of results written back");
    const size_t n = std::min(g_got.size(), g_exp.size());
    size_t bad = 0, meta = 0;
    for (size_t k = 0; k < n; k++)
    {
        if (g_got[k].addr != g_exp[k].addr || g_got[k].mask != g_exp[k].mask) meta++;
        for (int l = 0; l < 32; l++)
            if (((g_exp[k].mask >> l) & 1u) && g_got[k].v[size_t(l)] != g_exp[k].out[size_t(l)]) bad++;
    }
    std::printf("[stream] results %zu / expected %zu; lanes different from the call-style connection %zu; address / mask not carried %zu\n",
                g_got.size(), g_exp.size(), bad, meta);
    check(bad == 0, "values equal to HAS_GVU_TOP=1 beat by beat");
    check(meta == 0, "address and mask carried");
    // timing of every packet that went through the unit (the bypassed one is checked as well)
    // layers 0-2 (104 packets): push->latch 1 and latch->result kDepth; layer 3 (the last 8, ready held low): push->latch 8 (waited in the FIFO)
    size_t lat_bad = 0;
    for (size_t k = 0; k < gs->trace.size(); k++)
    {
        const GvuTopStream::Trace &t = gs->trace[k];
        const uint64_t want = (k + 8 >= gs->trace.size()) ? 8 : 1;
        if (t.accept - t.push != want || t.out - t.accept != uint64_t(GvuTopStream::kDepth)) lat_bad++;
    }
    std::printf("[stream] packets off the expected timing (push->latch 1, or 8 for the 8 held ones; latch->result %d): %zu of %zu\n", GvuTopStream::kDepth, lat_bad, gs->trace.size());
    check(lat_bad == 0, "latency E -> E+1 -> E+1+kDepth for every packet");
    for (size_t k = 0; k < std::min<size_t>(gs->trace.size(), 3); k++)
        std::printf("  packet %zu: in the FIFO at edge %llu, latched by GvuTop at edge %llu, result written back at edge %llu "
                    "(push->latch %llu, latch->result %llu)\n", k, (unsigned long long)gs->trace[k].push, (unsigned long long)gs->trace[k].accept,
                    (unsigned long long)gs->trace[k].out, (unsigned long long)(gs->trace[k].accept - gs->trace[k].push),
                    (unsigned long long)(gs->trace[k].out - gs->trace[k].accept));
    // a canonical line for the comparison between the two driver orders
    uint64_t h = 1469598103934665603ull;
    for (const GvuTopStream::Trace &t : gs->trace) { h ^= t.push * 1000003ull + t.accept * 31ull + t.out; h *= 1099511628211ull; }
    std::printf("[stream] trace checksum (push, latch, result of every packet) %016llx over %zu packets\n", (unsigned long long)h, gs->trace.size());
    // FIFO and negative checks
    std::printf("[stream] FIFO: queue max %llu, stall_needed %llu (expected 8 and 4); differences counted against the model %llu (expected 1: the wrong lane)\n",
                (unsigned long long)st.fifo_max, (unsigned long long)st.stall_needed, (unsigned long long)st.differ);
    check(st.stall_needed == 4, "stall_needed == 4 with ready held 8 edges and 8 packets");
    check(st.fifo_max == 8, "queue max == 8");
    check(st.differ == 1, "exactly one difference counted against the wrong model lane");
    check(st.bypassed == 1, "one bypassed packet");
    check(st.meta_bad == 0, "address / mask carried through GvuTop");
    // RTL observer: the 8 packets of layer 3 have oc_tile_last = 1 and oc_last on channel 4 (k = 4)
    std::printf("[stream] observer: exits of GEMM_FUSED_OBP (oc_last & oc_tile_last) %llu (expected 1), packets within %d beats after an exit %llu\n",
                (unsigned long long)st.rtl_exit_events, GvuTopStream::kRtlExitDelay, (unsigned long long)st.rtl_at_risk);
    check(st.rtl_exit_events == 1, "observer: one exit event");
    check(add_checks(*gs) == 0, "residual add equal to the call-style connection, same cycles");
    GvuTopStream::report();
    std::printf("RESULT: %s (errors %d)\n", g_errors ? "FAIL" : "PASS", g_errors);
    return g_errors ? 1 : 0;
}
