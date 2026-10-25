// tb_gvu_top_params.cpp -- the two GEMM_FUSED parameters of GvuTop (has/rtl_ref_gvu_top.h) that stand for hardware choices
// still open, each checked against the model:
//   depth      GvuTopParams::gemm_stream_depth (default rtl_ref_gvu::GEMM_STREAM_DEPTH, 9 = the register cuts of the MAS; the
//              hardware RTL measures 11). A unit built with depth 11 and a unit built with depth 9 take the same beats in
//              stream mode (bursts of 1..64, gaps, lane masks with empty ones, scale and shift per beat, full value ranges):
//              every beat leaves exactly `depth` cycles after it entered, in order, with the same values, address and mask on
//              both units; the depth-9 results equal has::RtlObp::beat; nothing leaves in a gap; the stop ends in IDLE on both.
//              The accessor clamps a depth outside 9 .. GEMM_STREAM_DEPTH_MAX.
//   narrowing  In::i_gemm_narrow (NARROW_SATURATE, the default, or NARROW_WRAP), in the tile mode and in the stream mode,
//              against has::requant with the same knob (has::Knobs::req_narrow SAT16 / WRAP16), the unit's rounding mode and
//              its 16-bit zero-point adder that wraps (has::Knobs::zp_add_wrap16 = 1); activation skipped, so the int8 output
//              is the requant result. Values whose round-shift leaves int16 are drawn on purpose (counted); each run must
//              have such lanes, SAT16 and WRAP16 must differ on some of them, and every lane must equal has::requant bit for
//              bit. For information only: lanes where has::requant without the wrapping zero-point adder gives another value.
//   zp         (mode "zp", alone) the zero point of GEMM_FUSED on the call-style connection (has::GvuTopBridge::gemm_beat_narrow,
//              HAS_GVU_TOP=1 set by the program): the RTL holds 8 bits (GEMM_FUSED register [15:8]) sign-extended into the
//              16-bit adder. A zero point in int8 takes every beat and every lane equals has::RtlObp::beat; a zero point outside
//              int8 is not expressible on every beat (one count each) and the beat keeps the model result; the case
//              ZP_OUT = 200 with one beat gives exactly one. For information only: lanes where the sign-extended low byte (what
//              the RTL would add) gives another value than the model.
// build: g++ -std=c++17 -O2 -Wall -I. tools/has/tb_gvu_top_params.cpp -o tb_gvu_top_params
// usage: tb_gvu_top_params [runs per part, default 8] [seed, default 1]
//        tb_gvu_top_params zp [seed, default 1]
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>
#include "has/gvu_quant.h"
#include "has/gvu_rtl_obp.h"
#include "has/gvu_top_bridge.h"
#include "has/rtl_ref_gvu_top.h"

using namespace rtl_ref_gvu;

static const RoundMode kRtl[7] = {RND_FLOOR, RND_NEAR_UP, RND_NEAR_AWAY, RND_NEAR_EVEN, RND_CEIL, RND_TRUNC, RND_AWAY};
static const char *kName[7] = {"floor", "half_up", "half_away", "half_even", "ceil", "trunc", "away"};
static const unsigned T = 64, TILES = 2, N = TILES * GVU_LANES * T;   // tile mode: element (k, j, i) at k * 32 * T + j * T + i
static std::mt19937_64 g_rng;

static long long uni(long long lo, long long hi) { return std::uniform_int_distribution<long long>(lo, hi)(g_rng); }

static void reset(GvuTop &dut)
{
    GvuTop::In in;
    for (int c = 0; c < 5; c++) dut.tick(in, false);
    dut.tick(in, true);
}

static void poke_lut(GvuTop &dut, const std::vector<int8_t> &lut)
{
    for (unsigned w = 0; w < 2; w++) dut.scratchpad().sram(1, 1).poke(w, SpBytes(lut.begin() + 128 * w, lut.begin() + 128 * (w + 1)));
}

// Into GEMM_FUSED_ST1 in stream mode from a fresh unit (through LUT_ACT_INIT). Returns false on a timeout.
static bool enter_stream(GvuTop &dut, GvuTop::In &in, int m, int zp, bool act, NarrowMode nm)
{
    in = GvuTop::In();
    in.i_gvu_op = GVU_OP_GEMM_FUSED; in.i_gemm_stream = true; in.i_gemm_round = kRtl[m]; in.i_gemm_narrow = nm; in.i_gemm_zp16 = zp;
    in.i_gemm_skip_act = !act; in.i_vbank1_buf_sel = true; in.i_pipeline_en = true;
    for (int k = 0; k < 100 && dut.state() != GEMM_FUSED_ST1; k++) dut.tick(in, true);
    return dut.state() == GEMM_FUSED_ST1;
}

// Stops a stream run; returns the cycles until IDLE (0 on a timeout).
static int stop_stream(GvuTop &dut, GvuTop::In &in)
{
    in.i_psum_valid = false; in.i_gemm_stream_stop = true;
    for (int c = 1; c < 100; c++)
    {
        dut.tick(in, true);
        if (dut.state() == IDLE) { in.i_pipeline_en = false; in.i_gemm_stream_stop = false; return c; }
    }
    return 0;
}

struct Beat { std::vector<int32_t> ps; int32_t scale{1}; int shift{0}; uint32_t mask{0}, addr{0}; uint64_t t_in{0}; };
struct Res { uint64_t cyc; uint32_t addr, mask; std::array<int8_t, GVU_LANES> v; };

// Drives the beats through a stream-mode unit with bursts and gaps (schedule drawn from `sched`, the same for every unit
// given the same seed) and collects every output with its cycle.
static std::vector<Res> drive(GvuTop &dut, GvuTop::In &in, std::vector<Beat> &beats, uint64_t sched_seed, uint64_t *cycles)
{
    std::mt19937_64 rs(sched_seed);
    auto r = [&rs](long long lo, long long hi) { return std::uniform_int_distribution<long long>(lo, hi)(rs); };
    std::vector<Res> out;
    uint64_t cyc = 0;
    size_t sent = 0;
    int burst = 0, gap = 0;
    while ((out.size() < beats.size() || sent < beats.size()) && cyc < 200000)
    {
        bool send = false;
        if (sent < beats.size())
        {
            if (burst == 0 && gap == 0) { burst = static_cast<int>(r(1, 64)); gap = static_cast<int>(r(0, 3) == 0 ? r(65, 100) : r(0, 20)); }
            if (burst > 0) { send = true; burst--; }
            else gap--;
        }
        in.i_psum_valid = send;
        if (send)
        {
            Beat &x = beats[sent++];
            for (unsigned l = 0; l < GVU_LANES; l++) in.i_psum_vec[l] = x.ps[l];
            in.i_psum_scale = x.scale; in.i_psum_shift = static_cast<uint8_t>(x.shift); in.i_psum_mask = x.mask; in.i_psum_addr = x.addr;
            x.t_in = cyc;
        }
        const GvuTop::Out o = dut.tick(in, true);
        if (o.o_gemm_fused_valid) out.push_back(Res{cyc, o.o_gemm_fused_addr, o.o_gemm_fused_mask, o.o_gemm_fused_data});
        cyc++;
    }
    in.i_psum_valid = false;
    *cycles = cyc;
    return out;
}

// ---- part 1: depth 11 against depth 9
static int depth_runs(int runs)
{
    int failed = 0;
    {
        GvuTopParams p;
        const int d_def = GvuTop(p).gemm_stream_depth();
        p.gemm_stream_depth = 11; const int d11 = GvuTop(p).gemm_stream_depth();
        p.gemm_stream_depth = 5; const int d_lo = GvuTop(p).gemm_stream_depth();
        p.gemm_stream_depth = 99; const int d_hi = GvuTop(p).gemm_stream_depth();
        const bool ok = d_def == GEMM_STREAM_DEPTH && d11 == 11 && d_lo == GEMM_STREAM_DEPTH_MAS && d_hi == GEMM_STREAM_DEPTH_MAX;
        std::printf("depth parameter: default %d (GEMM_STREAM_DEPTH %d), set 11 -> %d, set 5 -> %d, set 99 -> %d %s\n", d_def, GEMM_STREAM_DEPTH,
                    d11, d_lo, d_hi, ok ? "ok" : "FAILED");
        if (!ok) failed++;
    }
    static const int kBeats = 400;
    std::vector<int8_t> lut(256);
    for (int r = 0; r < runs; r++)
    {
        for (auto &v : lut) v = static_cast<int8_t>(uni(-128, 127));
        const bool act = r % 2 == 0;
        const int kind = r % 3, m = static_cast<int>(uni(0, 6));
        const int zp = static_cast<int>(r % 2 ? uni(-128, 127) : uni(-32768, 32767));
        std::vector<Beat> beats(kBeats);
        std::vector<std::vector<int32_t>> ref(kBeats, std::vector<int32_t>(32, 0));
        has::RtlObp::Unit ep;
        ep.load_act(lut.data());
        bool expr = true;
        for (int b = 0; b < kBeats; b++)
        {
            Beat &x = beats[b];
            x.ps.resize(32);
            for (unsigned l = 0; l < 32; l++)
            {
                const int q = static_cast<int>(l % 4);
                const long long lim = q == 0 ? 200 : q == 1 ? 40000 : q == 2 ? 4000000 : 2147483647LL;
                x.ps[l] = static_cast<int32_t>(uni(-lim - (q == 3 ? 1 : 0), lim));
            }
            x.shift = static_cast<int>(kind == 0 ? uni(4, 12) : kind == 1 ? uni(34, 40) : uni(0, 63));
            x.scale = static_cast<int32_t>(kind == 0 ? uni(1, 1024) : kind == 1 ? uni(1LL << 30, 2147483647LL) : uni(-2147483648LL, 2147483647LL));
            const int mk = static_cast<int>(uni(0, 9));
            x.mask = mk == 0 ? 0u : mk < 6 ? 0xFFFFFFFFu : static_cast<uint32_t>(uni(0, 0xFFFFFFFFLL));
            x.addr = static_cast<uint32_t>(b * 7 + r);
            expr = has::RtlObp::beat(ep, x.ps, ref[b], x.mask, x.scale, x.shift, zp, m, true, act) && expr;
        }
        const uint64_t sched = static_cast<uint64_t>(uni(0, 1LL << 62));
        GvuTopParams p9, p11; p9.gemm_stream_depth = GEMM_STREAM_DEPTH_MAS; p11.gemm_stream_depth = 11;
        GvuTop u9(p9), u11(p11);
        GvuTop::In i9, i11;
        reset(u9); reset(u11);
        poke_lut(u9, lut); poke_lut(u11, lut);
        const bool in_ok = enter_stream(u9, i9, m, zp, act, NARROW_SATURATE) && enter_stream(u11, i11, m, zp, act, NARROW_SATURATE);
        std::vector<Beat> b9 = beats, b11 = beats;
        uint64_t c9 = 0, c11 = 0;
        const std::vector<Res> o9 = drive(u9, i9, b9, sched, &c9);
        const std::vector<Res> o11 = drive(u11, i11, b11, sched, &c11);
        const int s9 = stop_stream(u9, i9), s11 = stop_stream(u11, i11);
        unsigned lat9 = 0, lat11 = 0, meta = 0, val = 0, bad_ref = 0;
        const size_t n = std::min(o9.size(), o11.size());
        for (size_t k = 0; k < n; k++)
        {
            if (o9[k].cyc != b9[k].t_in + 9) lat9++;
            if (o11[k].cyc != b11[k].t_in + 11) lat11++;
            if (o9[k].addr != beats[k].addr || o9[k].mask != beats[k].mask || o11[k].addr != o9[k].addr || o11[k].mask != o9[k].mask) meta++;
            for (unsigned l = 0; l < 32; l++)
            {
                if (!((beats[k].mask >> l) & 1u)) continue;
                if (o11[k].v[l] != o9[k].v[l]) val++;
                if (o9[k].v[l] != static_cast<int8_t>(ref[k][l])) bad_ref++;
            }
        }
        const bool ok = in_ok && expr && o9.size() == size_t(kBeats) && o11.size() == size_t(kBeats) && !lat9 && !lat11 && !meta && !val && !bad_ref &&
                        s9 > 0 && s11 > 0 && u9.state() == IDLE && u11.state() == IDLE;
        std::printf("depth run %2d: %-9s act %d zp %6d | beats out %zu / %zu (depth 9) %zu / %zu (depth 11) | latency off 9: %u, off 11: %u | "
                    "address / mask %u | lanes depth 11 != depth 9: %u | depth 9 != model: %u | stop to IDLE %d / %d cycles | %s\n",
                    r, kName[m], int(act), zp, o9.size(), size_t(kBeats), o11.size(), size_t(kBeats), lat9, lat11, meta, val, bad_ref, s9, s11, ok ? "ok" : "FAILED");
        if (!ok) failed++;
    }
    return failed;
}

// ---- part 2: narrowing, tile mode and stream mode, against has::requant
struct NarrowCount { unsigned long long lanes{0}, over{0}, bad{0}, sat_vs_wrap{0}, info_nowrapzp{0}; };

static has::Knobs knobs(int m, int narrow, int zp_wrap)
{
    has::Knobs k;
    k.round_mode = m; k.req_narrow = narrow; k.zp_add_wrap16 = zp_wrap;
    return k;
}

// Draws one psum and its scale / shift so that the round-shift leaves int16 often. kind 0: |x * S >> s| up to about 2^30;
// kind 1: the full ranges, shift up to 40; kind 2: x = j * 65536 + small with S = 1, s = 0 (the low 16 bits fall inside
// int8, so WRAP16 and SAT16 give visibly different int8 results); kind 3: x * S >> s = x exactly, around +-32768 (both
// sides of the limit).
static void draw(int kind, int32_t *x, int32_t *S, int *s)
{
    if (kind == 0) { *x = static_cast<int32_t>(uni(-(1LL << 22), 1LL << 22)); *S = static_cast<int32_t>(uni(1, 1 << 12)); *s = static_cast<int>(uni(0, 6)); }
    else if (kind == 1) { *x = static_cast<int32_t>(uni(-2147483648LL, 2147483647LL)); *S = static_cast<int32_t>(uni(-2147483648LL, 2147483647LL)); *s = static_cast<int>(uni(0, 40)); }
    else if (kind == 2) { *x = static_cast<int32_t>(uni(-30000, 30000) * 65536LL + uni(-200, 200)); *S = 1; *s = 0; }
    else { *x = static_cast<int32_t>((uni(0, 1) ? 1 : -1) * (32768 + uni(-300, 300))); *s = static_cast<int>(uni(0, 2)); *S = 1 << *s; }
}

template <class Done>
static uint64_t run_tile(GvuTop &dut, GvuTop::In &in, Done done)
{
    in.i_pipeline_en = true;
    for (uint64_t c = 1; c < 20000; c++)
    {
        const GvuTop::Out o = dut.tick(in, true);
        if (o.o_gemm_fused_lut_init_done && in.i_vbank1_buf_sel && dut.state() == GEMM_FUSED_ST1) in.i_vbank1_buf_sel = false;
        if (done(o))
        {
            in.i_pipeline_en = false;
            for (int k = 0; k < 15; k++) dut.tick(in, true);
            return c;
        }
    }
    return 0;
}

static std::vector<int8_t> tile_once(const std::vector<int32_t> &ps, const std::vector<int8_t> &lut, int32_t S, int s, int zp, int m, NarrowMode nm, uint64_t *cyc)
{
    GvuTop dut;
    reset(dut);
    poke_lut(dut, lut);
    for (unsigned k = 0; k < TILES; k++)
        for (unsigned i = 0; i < T; i++)
        {
            SpBytes row(4 * GVU_LANES, 0);
            for (unsigned j = 0; j < GVU_LANES; j++)
            {
                const uint32_t u = static_cast<uint32_t>(ps[k * GVU_LANES * T + j * T + i]);
                for (int b = 0; b < 4; b++) row[4 * j + b] = static_cast<uint8_t>(u >> (8 * b));
            }
            dut.scratchpad().sram(2, 0).poke(k * T + i, row);
        }
    GvuTop::In in;
    in.i_gvu_op = GVU_OP_GEMM_FUSED; in.i_num_tile = TILES; in.i_token_len = T;
    in.i_gemm_scale = S; in.i_gemm_shift = static_cast<int8_t>(s); in.i_gemm_zp = static_cast<int8_t>(zp);
    in.i_gemm_round = kRtl[m]; in.i_gemm_narrow = nm; in.i_gemm_skip_act = true; in.i_gemm_skip_zp = false; in.i_gemm_per_channel = false;
    in.i_vbank1_buf_sel = true;
    *cyc = run_tile(dut, in, [](const GvuTop::Out &o) { return o.o_gemm_fused_pipe_done; });
    std::vector<int8_t> r(N);
    for (unsigned k = 0; k < TILES; k++)
        for (unsigned i = 0; i < T; i++)
        {
            const SpBytes row = dut.scratchpad().sram(0, 0).word(k * T + i);
            for (unsigned j = 0; j < GVU_LANES; j++) r[k * GVU_LANES * T + j * T + i] = static_cast<int8_t>(row[j]);
        }
    return r;
}

static int narrow_tile_runs(int runs, NarrowCount *tot)
{
    int failed = 0;
    std::vector<int8_t> lut(256);
    for (auto &v : lut) v = static_cast<int8_t>(uni(-128, 127));
    for (int r = 0; r < runs; r++)
    {
        // the tile mode has one scale / shift per run: draw them with the first element, then the psums of that kind
        const int kind = r % 4, m = static_cast<int>(uni(0, 6)), zp = static_cast<int>(uni(-128, 127));
        int32_t x0, S; int s;
        draw(kind, &x0, &S, &s);
        std::vector<int32_t> ps(N);
        for (unsigned e = 0; e < N; e++) { int32_t S2; int s2; draw(kind, &ps[e], &S2, &s2); }
        uint64_t c_sat = 0, c_wrap = 0;
        const std::vector<int8_t> g_sat = tile_once(ps, lut, S, s, zp, m, NARROW_SATURATE, &c_sat);
        const std::vector<int8_t> g_wrap = tile_once(ps, lut, S, s, zp, m, NARROW_WRAP, &c_wrap);
        NarrowCount c;
        unsigned bad_sat = 0, bad_wrap = 0;
        for (unsigned e = 0; e < N; e++)
        {
            has::QuantCounters q;
            const int8_t r_sat = has::requant(ps[e], S, s, zp, knobs(m, has::SAT16, 1), &q);
            const int8_t r_wrap = has::requant(ps[e], S, s, zp, knobs(m, has::WRAP16, 1));
            const int8_t r_wrap0 = has::requant(ps[e], S, s, zp, knobs(m, has::WRAP16, 0));
            const int8_t r_sat0 = has::requant(ps[e], S, s, zp, knobs(m, has::SAT16, 0));
            c.lanes++;
            if (q.sat16) c.over++;
            if (g_sat[e] != r_sat) bad_sat++;
            if (g_wrap[e] != r_wrap) bad_wrap++;
            if (g_sat[e] != g_wrap[e]) c.sat_vs_wrap++;
            if (r_wrap0 != r_wrap || r_sat0 != r_sat) c.info_nowrapzp++;
        }
        c.bad = bad_sat + bad_wrap;
        const bool ok = c_sat && c_wrap && c.bad == 0 && c.over > 0 && c.sat_vs_wrap > 0;
        std::printf("tile   run %2d: kind %d scale %11d shift %2d zp %4d %-9s | lanes %llu, round-shift outside int16 %llu | "
                    "SAT16 != has::requant %u, WRAP16 != has::requant %u | SAT16 vs WRAP16 differ %llu | cycles %llu / %llu | %s\n",
                    r, kind, S, s, zp, kName[m], c.lanes, c.over, bad_sat, bad_wrap, c.sat_vs_wrap,
                    static_cast<unsigned long long>(c_sat), static_cast<unsigned long long>(c_wrap), ok ? "ok" : "FAILED");
        tot->lanes += c.lanes; tot->over += c.over; tot->bad += c.bad; tot->sat_vs_wrap += c.sat_vs_wrap; tot->info_nowrapzp += c.info_nowrapzp;
        if (!ok) failed++;
    }
    return failed;
}

static int narrow_stream_runs(int runs, NarrowCount *tot)
{
    int failed = 0;
    static const int kBeats = 300;
    std::vector<int8_t> lut(256);
    for (auto &v : lut) v = static_cast<int8_t>(uni(-128, 127));
    for (int r = 0; r < runs; r++)
    {
        const int m = static_cast<int>(uni(0, 6));
        const int zp = static_cast<int>(r % 2 ? uni(-128, 127) : uni(-32768, 32767));
        std::vector<Beat> beats(kBeats);
        for (int b = 0; b < kBeats; b++)
        {
            Beat &x = beats[b];
            x.ps.resize(32);
            const int kind = static_cast<int>(uni(0, 3));
            int32_t S0; int s0;
            draw(kind, &x.ps[0], &S0, &s0);
            x.scale = S0; x.shift = s0;
            for (unsigned l = 1; l < 32; l++) { int32_t S2; int s2; draw(kind, &x.ps[l], &S2, &s2); }
            x.mask = uni(0, 4) == 0 ? static_cast<uint32_t>(uni(0, 0xFFFFFFFFLL)) : 0xFFFFFFFFu;
            x.addr = static_cast<uint32_t>(b * 5 + r);
        }
        const uint64_t sched = static_cast<uint64_t>(uni(0, 1LL << 62));
        std::vector<Res> out[2];
        bool run_ok = true;
        for (int w = 0; w < 2; w++)
        {
            GvuTop dut;
            GvuTop::In in;
            reset(dut);
            poke_lut(dut, lut);
            run_ok = enter_stream(dut, in, m, zp, false, w ? NARROW_WRAP : NARROW_SATURATE) && run_ok;
            std::vector<Beat> bw = beats;
            uint64_t cyc = 0;
            out[w] = drive(dut, in, bw, sched, &cyc);
            run_ok = stop_stream(dut, in) > 0 && dut.state() == IDLE && run_ok;
            unsigned lat = 0;
            for (size_t k = 0; k < out[w].size() && k < bw.size(); k++) if (out[w][k].cyc != bw[k].t_in + uint64_t(dut.gemm_stream_depth())) lat++;
            run_ok = lat == 0 && out[w].size() == size_t(kBeats) && run_ok;
        }
        NarrowCount c;
        unsigned bad_sat = 0, bad_wrap = 0, meta = 0;
        for (size_t k = 0; k < std::min(out[0].size(), out[1].size()); k++)
        {
            const Beat &x = beats[k];
            if (out[0][k].addr != x.addr || out[0][k].mask != x.mask || out[1][k].addr != x.addr || out[1][k].mask != x.mask) meta++;
            for (unsigned l = 0; l < 32; l++)
            {
                if (!((x.mask >> l) & 1u)) continue;
                has::QuantCounters q;
                const int8_t r_sat = has::requant(x.ps[l], x.scale, x.shift, zp, knobs(m, has::SAT16, 1), &q);
                const int8_t r_wrap = has::requant(x.ps[l], x.scale, x.shift, zp, knobs(m, has::WRAP16, 1));
                const int8_t r_wrap0 = has::requant(x.ps[l], x.scale, x.shift, zp, knobs(m, has::WRAP16, 0));
                const int8_t r_sat0 = has::requant(x.ps[l], x.scale, x.shift, zp, knobs(m, has::SAT16, 0));
                c.lanes++;
                if (q.sat16) c.over++;
                if (out[0][k].v[l] != r_sat) bad_sat++;
                if (out[1][k].v[l] != r_wrap) bad_wrap++;
                if (out[0][k].v[l] != out[1][k].v[l]) c.sat_vs_wrap++;
                if (r_wrap0 != r_wrap || r_sat0 != r_sat) c.info_nowrapzp++;
            }
        }
        c.bad = bad_sat + bad_wrap;
        const bool ok = run_ok && meta == 0 && c.bad == 0 && c.over > 0 && c.sat_vs_wrap > 0;
        std::printf("stream run %2d: zp %6d %-9s | lanes %llu, round-shift outside int16 %llu | SAT16 != has::requant %u, WRAP16 != has::requant %u | "
                    "SAT16 vs WRAP16 differ %llu | address / mask %u | %s\n",
                    r, zp, kName[m], c.lanes, c.over, bad_sat, bad_wrap, c.sat_vs_wrap, meta, ok ? "ok" : "FAILED");
        tot->lanes += c.lanes; tot->over += c.over; tot->bad += c.bad; tot->sat_vs_wrap += c.sat_vs_wrap; tot->info_nowrapzp += c.info_nowrapzp;
        if (!ok) failed++;
    }
    return failed;
}

// ---- mode zp: zero point through the call-style connection
// Draws one beat (psums over the full ranges, scale / shift of the three kinds of part 1, a lane mask) and its model result.
static void draw_beat(Beat &x, uint32_t addr)
{
    x.ps.resize(32);
    for (unsigned l = 0; l < 32; l++)
    {
        const int q = static_cast<int>(l % 4);
        const long long lim = q == 0 ? 200 : q == 1 ? 40000 : q == 2 ? 4000000 : 2147483647LL;
        x.ps[l] = static_cast<int32_t>(uni(-lim - (q == 3 ? 1 : 0), lim));
    }
    const int kind = static_cast<int>(uni(0, 2));
    x.shift = static_cast<int>(kind == 0 ? uni(4, 12) : kind == 1 ? uni(34, 40) : uni(0, 63));
    x.scale = static_cast<int32_t>(kind == 0 ? uni(1, 1024) : kind == 1 ? uni(1LL << 30, 2147483647LL) : uni(-2147483648LL, 2147483647LL));
    const int mk = static_cast<int>(uni(0, 9));
    x.mask = mk == 0 ? 0u : mk < 6 ? 0xFFFFFFFFu : static_cast<uint32_t>(uni(0, 0xFFFFFFFFLL));
    x.addr = addr;
}

static int zp_mode()
{
#ifdef _WIN32
    _putenv_s("HAS_GVU_TOP", "1");
#else
    setenv("HAS_GVU_TOP", "1", 1);
#endif
    using has::GvuTopBridge;
    if (!GvuTopBridge::on()) { std::printf("RESULT: FAIL (the connection did not start)\n"); return 1; }
    int failed = 0;
    {
        static const int v[6] = {-37, 127, -128, 200, -129, 255};
        static const int want[6] = {-37, 127, -128, -56, 127, -1};
        static const bool fits[6] = {true, true, true, false, false, false};
        bool ok = true;
        for (int i = 0; i < 6; i++) ok = ok && GvuTopBridge::zp_reg(v[i]) == want[i] && GvuTopBridge::zp_fits(v[i]) == fits[i];
        std::printf("zero point register: -37 -> %d, 127 -> %d, -128 -> %d, 200 -> %d, -129 -> %d, 255 -> %d (fits int8: %d %d %d %d %d %d) %s\n",
                    GvuTopBridge::zp_reg(-37), GvuTopBridge::zp_reg(127), GvuTopBridge::zp_reg(-128), GvuTopBridge::zp_reg(200), GvuTopBridge::zp_reg(-129),
                    GvuTopBridge::zp_reg(255), int(GvuTopBridge::zp_fits(-37)), int(GvuTopBridge::zp_fits(127)), int(GvuTopBridge::zp_fits(-128)),
                    int(GvuTopBridge::zp_fits(200)), int(GvuTopBridge::zp_fits(-129)), int(GvuTopBridge::zp_fits(255)), ok ? "ok" : "FAILED");
        if (!ok) failed++;
    }
    struct Case { int zp, beats; };
    static const Case cases[] = {{-128, 60}, {-37, 60}, {-2, 60}, {0, 60}, {3, 60}, {127, 60},
                                 {128, 60}, {200, 60}, {-129, 60}, {255, 60}, {-32768, 60}, {32767, 60}, {200, 1}};
    std::vector<int8_t> lut(256);
    uint64_t tot_beats = 0, tot_not = 0, want_not = 0;
    uint32_t addr = 0;
    for (const Case &c : cases)
    {
        for (auto &e : lut) e = static_cast<int8_t>(uni(-128, 127));
        const int m = static_cast<int>(uni(0, 6));
        const bool act = uni(0, 1) == 1, int8 = c.zp >= -128 && c.zp <= 127;
        has::RtlObp::Unit ep, ep_rtl;
        ep.load_act(lut.data()); ep_rtl.load_act(lut.data());
        unsigned taken = 0, not_expr = 0, lanes = 0, differ = 0, touched = 0, info_rtl = 0;
        bool ref_ok = true;
        for (int b = 0; b < c.beats; b++)
        {
            Beat x;
            draw_beat(x, addr++);
            if (c.beats == 1) x.mask = 0xFFFFFFFFu;   // the one-beat case carries every lane
            std::vector<int32_t> ref(32, 0), rtl(32, 0);
            ref_ok = has::RtlObp::beat(ep, x.ps, ref, x.mask, x.scale, x.shift, c.zp, m, true, act) && ref_ok;
            has::RtlObp::beat(ep_rtl, x.ps, rtl, x.mask, x.scale, x.shift, GvuTopBridge::zp_reg(c.zp), m, true, act);
            std::vector<int32_t> out = ref;
            const bool t = GvuTopBridge::gemm_beat_narrow(x.ps, out, x.mask, x.addr, x.scale, x.shift, c.zp, m, has::SAT16, act, lut.data(), b == 0);
            if (t) taken++; else not_expr++;
            for (unsigned l = 0; l < 32; l++)
            {
                if (!((x.mask >> l) & 1u)) continue;
                lanes++;
                if (out[l] != ref[l]) { if (t) differ++; else touched++; }
                if (rtl[l] != ref[l]) info_rtl++;
            }
        }
        const unsigned want = int8 ? 0u : unsigned(c.beats);
        const bool ok = ref_ok && not_expr == want && taken + not_expr == unsigned(c.beats) && !differ && !touched && (int8 ? info_rtl == 0 : true);
        std::printf("zp %6d %-9s act %d | beats %3d, taken %3u, not expressible %3u (expected %3u) | lanes %5u, differ from the model %u, "
                    "bypassed lanes changed %u | information: lanes where the sign-extended low byte (%d) gives another value %u | %s\n",
                    c.zp, kName[m], int(act), c.beats, taken, not_expr, want, lanes, differ, touched, GvuTopBridge::zp_reg(c.zp), info_rtl, ok ? "ok" : "FAILED");
        if (!ok) failed++;
        tot_beats += unsigned(c.beats); tot_not += not_expr; want_not += want;
    }
    std::printf("zero point: beats %llu, not expressible %llu (expected %llu; the case ZP_OUT = 200 with one beat counts 1)\n",
                (unsigned long long)tot_beats, (unsigned long long)tot_not, (unsigned long long)want_not);
    const int n = 1 + int(sizeof cases / sizeof cases[0]);
    std::printf("RESULT: %s (zero-point cases not as expected: %d of %d)\n", failed ? "FAIL" : "PASS", failed, n);
    return failed ? 1 : 0;
}

int main(int argc, char **argv)
{
    if (argc > 1 && std::strcmp(argv[1], "zp") == 0)
    {
        g_rng.seed(argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 1);
        std::printf("GvuTop GEMM_FUSED zero point on the call-style connection (8 bits as the RTL register, sign-extended)\n");
        return zp_mode();
    }
    const int runs = argc > 1 ? std::atoi(argv[1]) : 8;
    g_rng.seed(argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 1);
    std::printf("GvuTop GEMM_FUSED parameters: stream depth (build default %d) and narrowing after the round-shift\n", GEMM_STREAM_DEPTH);
    const int fd = depth_runs(runs);
    NarrowCount nt, ns;
    const int ft = narrow_tile_runs(runs, &nt);
    const int fs = narrow_stream_runs(runs, &ns);
    std::printf("narrowing, tile mode  : lanes %llu, outside int16 %llu, differ from has::requant %llu, SAT16 vs WRAP16 differ %llu; "
                "information: lanes where has::requant without the wrapping zero-point adder differs %llu\n", nt.lanes, nt.over, nt.bad, nt.sat_vs_wrap, nt.info_nowrapzp);
    std::printf("narrowing, stream mode: lanes %llu, outside int16 %llu, differ from has::requant %llu, SAT16 vs WRAP16 differ %llu; "
                "information: lanes where has::requant without the wrapping zero-point adder differs %llu\n", ns.lanes, ns.over, ns.bad, ns.sat_vs_wrap, ns.info_nowrapzp);
    const int failed = fd + ft + fs;
    std::printf("RESULT: %s (runs not as expected: %d of %d)\n", failed ? "FAIL" : "PASS", failed, 3 * runs + 1);
    return failed ? 1 : 0;
}
