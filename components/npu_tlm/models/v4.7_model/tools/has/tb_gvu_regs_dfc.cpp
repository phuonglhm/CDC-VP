// tb_gvu_regs_dfc.cpp -- HAS_GVU_REGS=1: the DFC drives GvuTop through the register file of gvu_core (has/gvu_regs_dfc.h, has/gvu_reg_master.h,
// has/rtl_ref_gvu_config_regs.h, GvuTopStream::begin_layer_regs / end_layer_regs), without the core. Checks:
//   1. GvuRegMaster: write k of a sequence lands at edge k * L + 2 after the request, the sequence is over at edge n * L
//   2. the sequences of has/gvu_regs_dfc.h decoded by the register file (zero point, skip activation, opcode, enable; pooling fields)
//   3. five GEMM_FUSED layers by registers (a table or none, a table after an earlier one = the LUT reset pair, two calls in one layer,
//      a bypassed beat that carries the flags): values equal to the call-style connection (HAS_GVU_TOP=1) beat by beat, latency E -> E+1 ->
//      E+1+kDepth, the layer ends by itself on the beat of the last call (oc_last & oc_tile_last, F1' in the tap), no fallback, the enable
//      bit clears itself and the done flag is set and cleared, the vbank1 buffer bit toggles once per table load, the number of register
//      writes and of edges the DFC waits are as the sequences say
//   4. max pool by registers (GvuTopStream::start_maxpool with the register file on) against the call style: values, GvuTop cycles, runs,
//      and the elapsed edges = 1 + runs * (3 L + 2) + GvuTop cycles
//   5. residual add by registers (start_add with the register file on, gvu_regs_dfc::add_run) against the call style, with the vbank1 buffer
//      bit at 0 and at 1: values, GvuTop cycles, runs, the configuration decoded from the register file = the call style's, the number of
//      writes (9 on the first run of a job, later runs only the start plus Reg 4 / Reg 15 when rows / mask change, one clear per run) and
//      the elapsed edges = 1 + sum over runs (n_w * L + 2) + GvuTop cycles
// build: g++ -std=c++17 -O2 -Wall -I. tools/has/tb_gvu_regs_dfc.cpp -lsystemc -o tb_gvu_regs_dfc
// usage: tb_gvu_regs_dfc [L, default 4]
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

static std::mt19937_64 g_rng(11);
static long long uni(long long lo, long long hi) { return std::uniform_int_distribution<long long>(lo, hi)(g_rng); }
static int g_errors = 0;
static uint64_t g_add_writes = 0;
static unsigned g_lat = 4;
static void check(bool ok, const char *what)
{
    if (!ok) { g_errors++; std::printf("  FAIL: %s\n", what); }
}
static uint64_t edge_now() { return uint64_t(sc_time_stamp().value() / 10000); }   // 10 ns clock, 1 ps resolution

struct Exp { uint32_t addr, mask, ch; psum_vector_t<32, int32_t> out; };
struct Got { uint64_t edge; uint32_t addr, mask; psum_vector_t<32, int32_t> v; };
struct Call { std::vector<StreamPacket> pk; uint32_t nctx; bool tile_last; };
struct Layer
{
    bool has_lut; int8_t lut[256]; int zp; int round_mode; bool act;
    std::vector<Call> calls;
    unsigned begin_writes, total_writes;
};
static std::vector<Layer> g_layers;
static std::vector<Exp> g_exp;
static std::vector<Got> g_got;

struct Driver : sc_module
{
    sc_in<bool> clk{"clk"};
    PsmStreamTap *tap{nullptr};
    std::vector<std::vector<StreamPacket>> plan;   // plan[e] = packets pushed at edge e
    uint64_t edge{0};
    SC_CTOR(Driver) { SC_METHOD(proc); sensitive << clk.pos(); dont_initialize(); }
    void proc()
    {
        edge++;
        if (edge < plan.size())
            for (const StreamPacket &p : plan[edge]) tap->push(p);
    }
};

// ---- part 3: the layers
static void make_layers()
{
    // dup: the row of the last channel of the first context straddles two SRAM-C words: two packets with oc_last in that context, so the
    // call has one more oc_last packet than contexts (the layer must still leave on the very last packet of the call)
    struct Spec { bool lut; int n; int nch; int bypass_at; int calls; bool bypass_last; bool dup; };
    static const Spec spec[7] = {{true, 64, 32, -1, 1, false, false}, {true, 20, 5, 10, 1, false, false}, {false, 12, 12, -1, 1, false, false},
                                 {true, 10, 5, -1, 2, false, false}, {true, 15, 5, -1, 1, true, false}, {true, 8, 4, -1, 1, false, false},
                                 {true, 10, 5, -1, 1, false, true}};
    uint32_t addr = 3000;
    unsigned tables = 0;
    for (int li = 0; li < 7; li++)
    {
        const Spec &sp = spec[li];
        Layer ly{};
        ly.has_lut = sp.lut; ly.act = sp.lut;
        for (int e = 0; e < 256; e++) ly.lut[e] = static_cast<int8_t>(sp.lut ? (li % 2 ? std::max(e - 128, 0) : (e - 128) / 2 + li) : 0);
        ly.zp = li == 1 ? -37 : static_cast<int>(uni(-20, 20));
        ly.round_mode = (li * 3) % 7;
        const bool table = sp.lut || tables == 0;                       // the first layer loads a table even without an activation
        ly.begin_writes = (table && tables > 0) ? 4u : 2u;               // Reg 3, [Reg 1 = 1, Reg 1 = 0], Reg 0
        if (li == 5) ly.begin_writes = 2u;                               // the max pool before the adds left the unit reset: its table loads as a first one (layer 6 follows adds that do not reset: the LUT reset pair is written)
        ly.total_writes = ly.begin_writes + 1u;                          // + the clear of the done flag
        if (table) tables++;
        for (int c = 0; c < sp.calls; c++)
        {
            Call cl;
            cl.nctx = uint32_t(sp.n / sp.nch);
            cl.tile_last = (c + 1 == sp.calls);
            for (int k = 0; k < sp.n; k++)
            {
                StreamPacket p;
                const int ch = k % sp.nch;
                p.ch = uint32_t(ch); p.addr = addr++;
                p.mask = (k % 7 == 3) ? 0x0000FFFFu : ((k % 11 == 5) ? 0x00F00F00u : 0xFFFFFFFFu);
                p.S = uni(1, 1 << 14); p.s = int(uni(8, 24));
                for (int l = 0; l < 32; l++) p.data[size_t(l)] = ((p.mask >> l) & 1u) ? int32_t(uni(-300000, 300000)) : 0;
                p.oc_last = (ch == sp.nch - 1);
                p.bypass = (c == 0 && k == sp.bypass_at) || (sp.bypass_last && cl.tile_last && k == sp.n - 1);
                cl.pk.push_back(p);
                if (sp.dup && c == 0 && k == sp.nch - 1)
                {
                    StreamPacket q = p;
                    q.addr = addr++;
                    for (int l = 0; l < 32; l++) q.data[size_t(l)] = ((q.mask >> l) & 1u) ? int32_t(uni(-300000, 300000)) : 0;
                    cl.pk.push_back(q);
                }
            }
            cl.pk.back().call_last = true;                              // the last write of the call (the tile's last element)
            ly.calls.push_back(cl);
        }
        g_layers.push_back(ly);
    }
    for (size_t li = 0; li < g_layers.size(); li++)
    {
        Layer &ly = g_layers[li];
        bool table_new = true;
        for (Call &cl : ly.calls)
            for (StreamPacket &p : cl.pk)
            {
                psum_vector_t<32, int32_t> out;
                if (p.bypass)
                {
                    for (int l = 0; l < 32; l++) out[size_t(l)] = ((p.mask >> l) & 1u) ? int32_t(uni(-128, 127)) : 0;
                    p.model = out;
                    g_exp.push_back(Exp{p.addr, p.mask, p.ch, out});
                    continue;
                }
                const bool ok = GvuTopBridge::gemm_beat(p.data, out, p.mask, p.addr, p.S, p.s, ly.zp, ly.round_mode, true, ly.act, ly.lut, table_new);
                table_new = false;
                if (!ok) std::printf("  (setup: the call-style beat did not run)\n");
                p.model = out;
                g_exp.push_back(Exp{p.addr, p.mask, p.ch, out});
            }
    }
}

struct Dfc : sc_module
{
    sc_in<bool> clk{"clk"};
    sc_out<bool> rstn{"rstn"};
    Driver *drv{nullptr};
    GvuTopStream *gs{nullptr};
    PsmStreamTap *tap{nullptr};
    unsigned toggles_seen{0};
    bool vbank1_bit{false};
    struct PoolCase { int g, H, W, kk, s, p; };
    std::vector<PoolCase> pools;
    SC_CTOR(Dfc) { SC_THREAD(th); sensitive << clk.pos(); }

    void layers(size_t first, size_t last)
    {
        for (size_t layer_no = first; layer_no < last; layer_no++)
        {
            Layer &ly = g_layers[layer_no];
            const uint64_t t0 = edge_now();
            gs->begin_layer_regs(ly.has_lut ? ly.lut : nullptr, ly.zp, ly.round_mode, ly.act);
            gs->reg_wait();
            const uint64_t waited = edge_now() - t0;
            char what[160];
            std::snprintf(what, sizeof what, "layer %u: the DFC waited n * L = %u edges for %u writes (waited %llu)", unsigned(layer_no), ly.begin_writes * g_lat,
                          ly.begin_writes, (unsigned long long)waited);
            check(waited == uint64_t(ly.begin_writes) * g_lat, what);
            const rtl_ref_gvu::GvuConfigOut o = gs->regs().out();
            check(o.pipe_en && o.op == 2 && o.gemm_fused_z == ly.zp && o.gemm_fused_skip_act == !ly.act, "after the start: enable, opcode GEMM_FUSED, zero point, skip activation");
            int guard = 0;
            while (!gs->ready_for_beats() && guard++ < 100) wait();
            check(guard < 100, "the unit takes beats after the start");
            for (size_t ci = 0; ci < ly.calls.size(); ci++)
            {
                Call &cl = ly.calls[ci];
                tap->begin_call(cl.nctx, cl.tile_last);
                const uint64_t X = drv->edge;
                drv->plan.assign(X + cl.pk.size() + 12, std::vector<StreamPacket>());
                for (size_t k = 0; k < cl.pk.size(); k++) drv->plan[X + 2 + k].push_back(cl.pk[k]);
                guard = 0;
                while (!(drv->edge >= drv->plan.size() && gs->drained(*tap)) && guard++ < 3000) wait();
                tap->end_call();                              // as HasNpuTop::core_end: the packets of the call are all in
                if (!cl.tile_last)
                {
                    wait(12);
                    check(gs->ready_for_beats() && gs->regs().out().pipe_en, "a call that is not the last of the layer leaves the unit in GEMM_FUSED");
                }
            }
            guard = 0;
            while (!(gs->regs().reg(0) & 2u) && guard++ < 200) wait();
            check((gs->regs().reg(0) & 3u) == 2u, "the layer ended by itself: enable bit cleared, done flag set");
            check(gs->unit().state() == rtl_ref_gvu::IDLE, "the unit is IDLE after the exit");
            const uint64_t t1 = edge_now();
            gs->end_layer_regs();
            check(edge_now() - t1 == g_lat, "the clear of the done flag costs one write");
            check((gs->regs().reg(0) & 3u) == 0u, "done flag cleared");
            for (int k = 0; k < 20; k++) wait();
        }
    }

    void max_pools()
    {
        std::printf("[regs] max pool by registers against the call style on the same unit\n");
        for (const PoolCase &pc : pools)
        {
            const size_t n_in = size_t(pc.g) * pc.H * pc.W;
            const int Ho = (pc.H + 2 * pc.p - pc.kk) / pc.s + 1, Wo = (pc.W + 2 * pc.p - pc.kk) / pc.s + 1;
            const size_t n_out = size_t(pc.g) * Ho * Wo;
            std::vector<int8_t> in(n_in), o1(n_out, 0), o2(n_out, 0);
            for (auto &v : in) v = int8_t(uni(-128, 127));
            const StreamStats before = stream_stats();
            uint64_t c1 = 0;
            const bool fits = gs->maxpool_fits_now(pc.g, pc.H, pc.W, pc.kk, pc.s, pc.p);
            const bool r1 = fits && gs->maxpool(in.data(), o1.data(), pc.g, pc.H, pc.W, pc.kk, pc.s, pc.p, 0, &c1);
            const uint64_t runs1 = stream_stats().mp_runs - before.mp_runs;
            const StreamStats mid = stream_stats();
            const uint64_t t0 = edge_now();
            gs->start_maxpool(in.data(), o2.data(), pc.g, pc.H, pc.W, pc.kk, pc.s, pc.p, 0);
            for (auto &v : in) v = 0;
            while (!gs->job_done()) wait(gs->job_event());
            const uint64_t elapsed = edge_now() - t0, c2 = gs->job_cycles();
            const uint64_t runs2 = stream_stats().regs_pool_runs - mid.regs_pool_runs;
            size_t d = 0;
            for (size_t i = 0; i < n_out; i++) if (o1[i] != o2[i]) d++;
            const uint64_t want = 1 + runs2 * (3ull * g_lat + 2) + c2;
            const bool ok = fits && r1 && d == 0 && c2 == c1 && runs1 == runs2 && elapsed == want;
            std::printf("[regs] max pool k%d s%d p%d %3d x %2d x %2d: call style %llu cycles %llu runs, by registers %llu cycles %llu runs, elapsed %llu edges "
                        "(1 + runs * (3 L + 2) + cycles = %llu), %zu elements differ %s\n", pc.kk, pc.s, pc.p, pc.g, pc.H, pc.W,
                        (unsigned long long)c1, (unsigned long long)runs1, (unsigned long long)c2, (unsigned long long)runs2, (unsigned long long)elapsed,
                        (unsigned long long)want, d, ok ? "ok" : "FAILED");
            if (!ok) g_errors++;
        }
    }

    struct AddCase { size_t n; int zpA, zpB, zpO; int64_t SA, SB, SO; int sA, sB, sO, rm; };
    void adds(const char *tag)
    {
        std::printf("[regs] residual add by registers against the call style on the same unit (vbank1 buffer bit %d, %s)\n",
                    int(gs->regs().out().vbank1_buf_sel), tag);
        const size_t sizes[] = {2304, 11, 4000, 4096, 5600, 8200, 12000};
        for (size_t n : sizes)
        {
            AddCase c;
            c.n = n; c.zpA = int(uni(-20, 20)); c.zpB = int(uni(-20, 20)); c.zpO = int(uni(-20, 20));
            c.SA = uni(1, 1 << 20); c.SB = uni(1, 1 << 20); c.SO = uni(1, 1 << 20);
            c.sA = int(uni(8, 24)); c.sB = int(uni(8, 24)); c.sO = int(uni(8, 24)); c.rm = int(uni(0, 6));
            std::vector<int8_t> A(n), B(n), o1(n, 0), o2(n, 0), Ac(n), Bc(n);
            for (size_t i = 0; i < n; i++) { A[i] = int8_t(uni(-128, 127)); B[i] = int8_t(uni(-128, 127)); }
            Ac = A; Bc = B;
            uint64_t c1 = 0;
            const bool r1 = gs->add(A.data(), B.data(), o1.data(), n, c.zpA, c.zpB, c.zpO, c.SA, c.SB, c.SO, c.sA, c.sB, c.sO, c.rm, true, true, 0, &c1);
            const StreamStats mid = stream_stats();
            const uint64_t t0 = edge_now();
            gs->start_add(Ac.data(), Bc.data(), o2.data(), n, c.zpA, c.zpB, c.zpO, c.SA, c.SB, c.SO, c.sA, c.sB, c.sO, c.rm, 0);
            for (auto &v : Ac) v = 0;
            for (auto &v : Bc) v = 0;
            while (!gs->job_done()) wait(gs->job_event());
            const uint64_t elapsed = edge_now() - t0, c2 = gs->job_cycles();
            const StreamStats &now = stream_stats();
            const uint64_t runs = now.regs_add_runs - mid.regs_add_runs, writes = now.regs_writes - mid.regs_writes;
            const size_t rows_total = (n + 31) / 32;
            uint64_t want_runs = 0, want_nw = 0, want_seq_edges = 0;
            unsigned prev_rows = 0; uint32_t prev_mask = 0;
            for (size_t r0 = 0; r0 < rows_total; r0 += 128)
            {
                const unsigned rows = unsigned(std::min<size_t>(128, rows_total - r0));
                const size_t tail = n - (r0 + rows - 1) * 32;
                const uint32_t mask = (r0 + rows == rows_total && tail < 32) ? ((1u << tail) - 1u) : 0xFFFFFFFFu;
                const unsigned nw = r0 == 0 ? 9u : 1u + (rows != prev_rows) + (mask != prev_mask);
                want_runs++; want_nw += nw; want_seq_edges += uint64_t(nw) * g_lat + 2;
                prev_rows = rows; prev_mask = mask;
            }
            size_t d = 0;
            for (size_t i = 0; i < n; i++) if (o1[i] != o2[i]) d++;
            const uint64_t want = 1 + want_seq_edges + c2;
            const bool ok = r1 && d == 0 && c2 == c1 && runs == want_runs && writes == want_nw + want_runs && elapsed == want &&
                            now.regs_add_cfg_mismatch == mid.regs_add_cfg_mismatch && now.elem_jobs_failed == mid.elem_jobs_failed;
            std::printf("[regs] add %5zu elements (%llu runs): call style %llu cycles, by registers %llu cycles, %llu writes (expected %llu), elapsed %llu edges "
                        "(1 + sum (n_w L + 2) + cycles = %llu), %zu elements differ %s\n", n, (unsigned long long)want_runs, (unsigned long long)c1,
                        (unsigned long long)c2, (unsigned long long)writes, (unsigned long long)(want_nw + want_runs), (unsigned long long)elapsed,
                        (unsigned long long)want, d, ok ? "ok" : "FAILED");
            if (!ok) g_errors++;
            g_add_writes += want_nw + want_runs;
        }
    }

    void th()
    {
        rstn.write(false);
        wait(5);
        rstn.write(true);
        wait(3);
        layers(0, 5);
        max_pools();
        adds("after 4 table loads");
        layers(5, 6);
        adds("after 5 table loads");
        layers(6, 7);
        sc_stop();
    }
};

// ---- parts 1 and 2: no clock
static void master_and_sequences()
{
    using namespace gvu_regs_dfc;
    {
        GvuRegMaster m(g_lat);
        std::vector<GvuRegMaster::W> seq = {{12, 1, 0xF}, {4, 2, 0xF}, {0, 3, 0xF}};
        const uint64_t T0 = 100000;
        check(m.request(seq, T0), "request accepted");
        check(!m.request(seq, T0), "a second request while busy is refused");
        unsigned applied = 0, drained_at = 0;
        std::vector<unsigned> at;
        GvuRegMaster::Step s = m.edge(T0);                      // the edge of the request counts for nothing
        check(!s.apply && !s.drained && m.busy(), "the request edge counts for nothing");
        for (unsigned e = 1; e <= 3 * g_lat + 2; e++)
        {
            s = m.edge(T0 + e * 10000);
            if (s.apply) { applied++; at.push_back(e); }
            if (s.drained) { drained_at = e; break; }
        }
        bool ok = applied == 3 && drained_at == 3 * g_lat && at.size() == 3 && at[0] == 2 && at[1] == g_lat + 2 && at[2] == 2 * g_lat + 2;
        std::printf("[regs] master L %u: writes land at edges %u %u %u, sequence over at edge %u (expected 2, %u, %u, %u) %s\n", g_lat,
                    at.size() > 0 ? at[0] : 0, at.size() > 1 ? at[1] : 0, at.size() > 2 ? at[2] : 0, drained_at, g_lat + 2, 2 * g_lat + 2, 3 * g_lat, ok ? "ok" : "FAILED");
        check(ok, "write k lands at edge k * L + 2, the sequence is over at edge n * L");
    }
    {
        rtl_ref_gvu::GvuConfigParams cp; cp.b13_fixed = true; cp.word_addressed = true;
        rtl_ref_gvu::GvuConfigRegs r(cp);
        auto apply = [&](const std::vector<GvuRegMaster::W> &seq) { for (const auto &w : seq) r.write_axi(w.byte_addr, w.data, w.strb); };
        apply(layer_begin(-37, true, false));
        rtl_ref_gvu::GvuConfigOut o = r.out();
        bool ok = o.pipe_en && o.op == 2 && o.gemm_fused_z == -37 && o.gemm_fused_skip_act && !o.lut_act_reset;
        apply(layer_begin(5, false, true));                       // ends with the LUT reset at 0 again
        o = r.out();
        ok = ok && o.pipe_en && o.op == 2 && o.gemm_fused_z == 5 && !o.gemm_fused_skip_act && !o.lut_act_reset;
        r.write_axi(0, 0u);                                       // the unit would have cleared the enable bit
        r.tick([] { rtl_ref_gvu::GvuConfigIn c; c.gvu_pipe_done = true; return c; }());
        ok = ok && (r.reg(0) & 3u) == 2u;
        apply(clear_done());
        ok = ok && (r.reg(0) & 3u) == 0u;
        apply(pool_run(3, 1, 2, 70, 20, 48, 2));                  // k = 5 (mode 3), stride 1, 2 W tiles, 70 channels, OH 20, moves 48 / 2
        o = r.out();
        ok = ok && o.pipe_en && o.op == 7 && o.pooling_mode == 3 && o.pooling_stride == 1 && o.pooling_num_w_tile == 2 && o.pooling_c == 70 &&
             o.pooling_oh == 20 && o.pooling_c_move == 48 && o.pooling_h_move == 2;
        std::printf("[regs] sequences decoded by the register file: zero point -37 / 5, skip activation, LUT reset pair, enable, done flag, clear, pooling fields %s\n",
                    ok ? "ok" : "FAILED");
        check(ok, "the sequences of gvu_regs_dfc are decoded by the register file as intended");
        check(layer_begin(0, false, false).size() == 2 && layer_begin(0, false, true).size() == 4 && clear_done().size() == 1 && pool_run(0, 1, 1, 1, 1, 0, 0).size() == 3,
              "number of writes per sequence");
    }
}

int sc_main(int argc, char **argv)
{
    if (argc > 1) g_lat = unsigned(std::max(2, std::atoi(argv[1])));
#ifdef _WIN32
    _putenv("HAS_GVU_TOP=1");
#else
    setenv("HAS_GVU_TOP", "1", 1);
#endif
    std::printf("== GvuTopStream by registers (HAS_GVU_REGS=1): L %u edges per register write\n", g_lat);
    master_and_sequences();
    make_layers();

    sc_set_time_resolution(1, SC_PS);
    sc_clock clk("clk", 10, SC_NS);
    sc_signal<bool> rstn{"rstn"}, ready, valid, oc_last, oc_tile_last, empty, stall, bypass;
    sc_signal<psum_vector_t<32, int32_t>> data, model;
    sc_signal<uint32_t> s_addr, s_mask, s_ch;
    sc_signal<int64_t> s_scale;
    sc_signal<int32_t> s_shift;
    sc_signal<uint64_t> s_push;

    Driver *drv = new Driver("drv"); drv->clk(clk);
    PsmStreamTap *tap = new PsmStreamTap("tap");
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
    gs->enable_regs(g_lat);
    tap->enable_f1();
    gs->result_sink = [&](uint32_t addr, uint32_t mask, const psum_vector_t<32, int32_t> &v)
    {
        Got g; g.edge = edge_now(); g.addr = addr; g.mask = mask; g.v = v;
        g_got.push_back(g);
    };
    Dfc *dfc = new Dfc("dfc");
    dfc->clk(clk); dfc->rstn(rstn); dfc->drv = drv; dfc->gs = gs; dfc->tap = tap;
    // max pool: several runs (cmax channels per run), k5 s1 p2 as the SPPF, k3 s1 p1, k2 s2 p0
    dfc->pools = {{70, 20, 20, 5, 1, 2}, {20, 40, 40, 3, 1, 1}, {13, 64, 64, 2, 2, 0}, {3, 20, 20, 5, 1, 2}};

    sc_start();

    const StreamStats &st = stream_stats();
    check(g_got.size() == g_exp.size(), "number of results written back");
    const size_t n = std::min(g_got.size(), g_exp.size());
    size_t bad = 0, meta = 0;
    for (size_t k = 0; k < n; k++)
    {
        if (g_got[k].addr != g_exp[k].addr || g_got[k].mask != g_exp[k].mask) meta++;
        for (int l = 0; l < 32; l++)
            if (((g_exp[k].mask >> l) & 1u) && g_got[k].v[size_t(l)] != g_exp[k].out[size_t(l)]) bad++;
    }
    std::printf("[regs] results %zu / expected %zu; lanes different from the call-style connection %zu; address / mask not carried %zu\n",
                g_got.size(), g_exp.size(), bad, meta);
    check(bad == 0, "values equal to HAS_GVU_TOP=1 beat by beat");
    check(meta == 0, "address and mask carried");
    size_t lat_bad = 0;
    for (const GvuTopStream::Trace &t : gs->trace) if (t.accept - t.push != 1 || t.out - t.accept != uint64_t(GvuTopStream::kDepth)) lat_bad++;
    std::printf("[regs] packets off the timing push->latch 1, latch->result %d: %zu of %zu\n", GvuTopStream::kDepth, lat_bad, gs->trace.size());
    check(lat_bad == 0, "latency E -> E+1 -> E+1+kDepth for every packet, bypassed ones too");
    unsigned want_writes = 0;
    for (const Layer &ly : g_layers) want_writes += ly.total_writes;
    // the max pool runs write 3 + 1 registers each
    const uint64_t pool_writes = 4 * st.regs_pool_runs;
    std::printf("[regs] layers %llu, layer exits by the flags %llu (fallback %llu, timeouts %llu), register writes %llu (layers %u + max pool %llu), "
                "toggles of the vbank1 buffer bit %llu, bypassed beats that carried the flags %llu\n", (unsigned long long)st.regs_layers,
                (unsigned long long)st.regs_exit_flags, (unsigned long long)st.regs_exit_fallback, (unsigned long long)st.regs_exit_timeout,
                (unsigned long long)st.regs_writes, want_writes, (unsigned long long)pool_writes, (unsigned long long)st.regs_lut_toggles,
                (unsigned long long)st.regs_flag_bypass);
    check(st.regs_layers == g_layers.size() && st.regs_exit_flags == g_layers.size() && st.regs_exit_fallback == 0 && st.regs_exit_timeout == 0,
          "every layer ended by the flags, no fallback");
    check(st.regs_writes == want_writes + pool_writes + g_add_writes, "number of register writes (layers + max pool + residual add)");
    check(st.regs_lut_toggles == 6, "the vbank1 buffer bit toggled once per table load (6 tables)");
    std::printf("[regs] residual add by registers: runs %llu, configuration read back differs from the call style in %llu runs, B in the vbank1 buffer 1 in %llu runs\n",
                (unsigned long long)st.regs_add_runs, (unsigned long long)st.regs_add_cfg_mismatch, (unsigned long long)st.regs_add_b_buf1);
    check(st.regs_add_cfg_mismatch == 0 && st.regs_add_b_buf1 > 0 && st.regs_add_runs > st.regs_add_b_buf1,
          "residual add: configuration from the registers = call style, both buffers of vbank1 covered");
    check(st.regs_flag_bypass == 1, "one bypassed beat carried the flags");
    std::printf("[regs] F1' in the tap: calls %llu (expected 8), calls whose oc_last packets != contexts %llu (expected 1: the straddling row), packets with oc_tile_last %llu\n",
                (unsigned long long)st.f1_calls, (unsigned long long)st.f1_ctx_mismatch, (unsigned long long)st.f1_tile_last_packets);
    check(st.f1_calls == 8 && st.f1_ctx_mismatch == 1 && st.f1_flag_bad == 0,
          "F1': the flag sits on the last packet of the last call of every layer only; the call with a straddling row has one oc_last packet too many");
    check(st.differ == 0 && st.meta_bad == 0, "no difference against the model counted");
    GvuTopStream::report();
    std::printf("RESULT: %s (errors %d)\n", g_errors ? "FAIL" : "PASS", g_errors);
    return g_errors ? 1 : 0;
}
