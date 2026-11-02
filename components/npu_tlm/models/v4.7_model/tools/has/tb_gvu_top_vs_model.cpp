// tb_gvu_top_vs_model.cpp -- GEMM_FUSED and residual add of GvuTop (has/rtl_ref_gvu_top.h) against the model's own
// cycle-level units, on random data over the whole value ranges:
//   GEMM_FUSED    has::RtlObp::beat (GvuGemmEpilogue of has/rtl_ref_gvu_requant.h; equal to has::requant + lut_direct on
//                 the YOLOv8m program, HAS_OBP_RTL)
//   residual add  has::RtlElem::add (GvuResAddPipe of has/rtl_ref_gvu_elemwise.h; equal to the model on every ELEM_WISE
//                 instruction of YOLOv8m and ViT-B/16, tb_rtl_ref_gvu_replay)
// GvuTop is driven as its own test benches do (tb_gvu_top_gemm_fused.cpp, tb_gvu_top_residual_add.cpp): tile layout of
// 2 tiles x 32 lanes x 64 tokens, data placed in the Scratchpad, result read from vbank0. The GEMM_FUSED runs follow each
// other without a reset, each with a new activation table (i_gemm_lut_reload). Every run draws its parameters (a small
// scale, a quantized convolution, or the full ranges), a zero point and one of the seven rounding modes; psums are a mix of
// small, medium and full int32 values.
// Per-channel parameters through the sbank are not exercised (the tile mode reads them by cycle count, see the plan of the
// core connection). Then the core-path modes: GEMM_FUSED in stream mode (one beat per PSM write, scale and shift per beat,
// lane masks, bursts and gaps, depth GVU_GEMM_STREAM_DEPTH) and residual adds whose last row is partial (i_res_last_lane_mask).
// build: g++ -std=c++17 -O2 -Wall -I. tools/has/tb_gvu_top_vs_model.cpp -o tb_gvu_top_vs_model
// usage: tb_gvu_top_vs_model [runs per operation, default 12] [seed, default 1]
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>
#include "has/gvu_quant.h"
#include "has/gvu_rtl_elem.h"
#include "has/gvu_rtl_obp.h"
#include "has/rtl_ref_gvu_top.h"

using namespace rtl_ref_gvu;

static const RoundMode kRtl[7] = {RND_FLOOR, RND_NEAR_UP, RND_NEAR_AWAY, RND_NEAR_EVEN, RND_CEIL, RND_TRUNC, RND_AWAY};
static const char *kName[7] = {"floor", "half_up", "half_away", "half_even", "ceil", "trunc", "away"};
static const unsigned T = 64, TILES = 2, N = TILES * GVU_LANES * T;   // element (k, j, i) at k * 32 * T + j * T + i
static std::mt19937_64 g_rng;

static long long uni(long long lo, long long hi) { return std::uniform_int_distribution<long long>(lo, hi)(g_rng); }

// Runs the controller until the done output of the operation; returns the cycles from the start, 0 on a timeout.
template <class Done>
static uint64_t run(GvuTop &dut, GvuTop::In &in, Done done, bool reload)
{
    in.i_pipeline_en = true;
    for (uint64_t c = 1; c < 20000; c++)
    {
        const GvuTop::Out o = dut.tick(in, true);
        if (reload && dut.state() == LUT_ACT_INIT) in.i_gemm_lut_reload = false;
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

static std::vector<int8_t> read_v0(GvuTop &dut)
{
    std::vector<int8_t> r(N);
    for (unsigned k = 0; k < TILES; k++)
        for (unsigned i = 0; i < T; i++)
        {
            const SpBytes row = dut.scratchpad().sram(0, 0).word(k * T + i);
            for (unsigned j = 0; j < GVU_LANES; j++) r[k * GVU_LANES * T + j * T + i] = static_cast<int8_t>(row[j]);
        }
    return r;
}

static int gemm_runs(int runs)
{
    GvuTop dut;
    GvuTop::In in;
    for (int c = 0; c < 5; c++) dut.tick(in, false);
    dut.tick(in, true);
    int failed = 0;
    for (int r = 0; r < runs; r++)
    {
        std::vector<int8_t> lut(256);
        for (auto &v : lut) v = static_cast<int8_t>(uni(-128, 127));
        // three kinds of parameters: a small scale, a quantized convolution (scale in [2^30, 2^31), outputs mostly inside
        // int8, so the rounding shows), and the full ranges (saturation and wrap paths)
        const int kind = r % 3, m = static_cast<int>(uni(0, 6)), zp = static_cast<int>(uni(-128, 127));
        const int shift = static_cast<int>(kind == 0 ? uni(4, 12) : kind == 1 ? uni(34, 40) : uni(0, 63));
        const int32_t scale = static_cast<int32_t>(kind == 0 ? uni(1, 1024) : kind == 1 ? uni(1LL << 30, 2147483647LL) : uni(-2147483648LL, 2147483647LL));
        std::vector<int32_t> ps(N);
        for (unsigned e = 0; e < N; e++)
        {
            const int q = static_cast<int>(e % 4);
            const long long lim = q == 0 ? 200 : q == 1 ? 40000 : q == 2 ? 4000000 : 2147483647LL;
            ps[e] = static_cast<int32_t>(uni(-lim - (q == 3 ? 1 : 0), lim));
        }
        for (unsigned w = 0; w < 2; w++) dut.scratchpad().sram(1, 1).poke(w, SpBytes(lut.begin() + 128 * w, lut.begin() + 128 * (w + 1)));
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
        // reference
        has::RtlObp::Unit ep;
        ep.load_act(lut.data());
        std::vector<int8_t> ref(N);
        bool expr = true;
        for (unsigned b = 0; b < N; b += 32)
        {
            std::vector<int32_t> vi(ps.begin() + b, ps.begin() + b + 32), vo(32, 0);
            expr = has::RtlObp::beat(ep, vi, vo, 0xFFFFFFFFu, scale, shift, zp, m, true, true) && expr;
            for (unsigned l = 0; l < 32; l++) ref[b + l] = static_cast<int8_t>(vo[l]);
        }
        // GvuTop
        in.i_gvu_op = GVU_OP_GEMM_FUSED; in.i_num_tile = TILES; in.i_token_len = T;
        in.i_gemm_scale = scale; in.i_gemm_shift = static_cast<int8_t>(shift); in.i_gemm_zp = static_cast<int8_t>(zp);
        in.i_gemm_round = kRtl[m]; in.i_gemm_skip_act = false; in.i_gemm_skip_zp = false; in.i_gemm_per_channel = false;
        in.i_vbank1_buf_sel = true; in.i_vbank0_buf_sel = false; in.i_vbank2_buf_sel = false; in.i_sbank_buf_sel = false;
        in.i_gemm_lut_reload = r > 0;
        const uint64_t cyc = run(dut, in, [](const GvuTop::Out &o) { return o.o_gemm_fused_pipe_done; }, r > 0);
        const std::vector<int8_t> got = read_v0(dut);
        unsigned bad = 0, first = 0;
        for (unsigned e = 0; e < N; e++) if (got[e] != ref[e]) { if (!bad) first = e; bad++; }
        std::printf("gemm  run %2d: scale %11d shift %2d zp %4d %-9s %s | cycles %4llu | %u / %u differ", r, scale, shift, zp, kName[m],
                    r ? "reload" : "first ", static_cast<unsigned long long>(cyc), bad, N);
        if (bad) std::printf(" (first: psum %d model %d GvuTop %d)", ps[first], ref[first], got[first]);
        if (!expr) std::printf(" (parameters not expressible on the model unit)");
        std::printf("\n");
        if (bad || !cyc || !expr) failed++;
    }
    return failed;
}

static int res_runs(int runs)
{
    int failed = 0;
    for (int r = 0; r < runs; r++)
    {
        const int m = static_cast<int>(uni(0, 6));
        const int32_t sa = static_cast<int32_t>(uni(1, 2147483647LL)), sb = static_cast<int32_t>(uni(1, 2147483647LL)), so = static_cast<int32_t>(uni(1, 2147483647LL));
        const int sha = static_cast<int>(uni(20, 40)), shb = static_cast<int>(uni(20, 40)), sho = static_cast<int>(uni(20, 40));
        const int za = static_cast<int>(uni(-128, 127)), zb = static_cast<int>(uni(-128, 127)), zo = static_cast<int>(uni(-128, 127));
        std::vector<int8_t> A(N), B(N), ref(N, 0);
        for (unsigned e = 0; e < N; e++) { A[e] = static_cast<int8_t>(uni(-128, 127)); B[e] = static_cast<int8_t>(uni(-128, 127)); }
        uint64_t mc = 0;
        const bool expr = has::RtlElem::add(A.data(), B.data(), ref.data(), N, za, zb, zo, sa, sb, so, sha, shb, sho, m, true, true, 0, &mc);
        GvuTop dut;
        GvuTop::In in;
        for (int c = 0; c < 5; c++) dut.tick(in, false);
        dut.tick(in, true);
        std::vector<SpBytes> v1((TILES * T + 3) / 4, SpBytes(4 * GVU_LANES, 0));
        for (unsigned k = 0; k < TILES; k++)
            for (unsigned i = 0; i < T; i++)
            {
                const unsigned row = k * T + i;
                SpBytes ra(GVU_LANES, 0);
                for (unsigned j = 0; j < GVU_LANES; j++)
                {
                    const unsigned e = k * GVU_LANES * T + j * T + i;
                    ra[j] = static_cast<uint8_t>(A[e]);
                    v1[row / 4][(row % 4) * GVU_LANES + j] = static_cast<uint8_t>(B[e]);
                }
                dut.scratchpad().sram(0, 0).poke(row, ra);
            }
        for (unsigned l = 0; l < v1.size(); l++) dut.scratchpad().sram(1, 0).poke(l, v1[l]);
        in.i_gvu_op = GVU_OP_RESIDUAL; in.i_num_tile = TILES; in.i_token_len = T;
        in.i_res_scale_a = sa; in.i_res_shift_a = static_cast<uint8_t>(sha); in.i_res_zp_a = static_cast<int8_t>(za);
        in.i_res_scale_b = sb; in.i_res_shift_b = static_cast<uint8_t>(shb); in.i_res_zp_b = static_cast<int8_t>(zb);
        in.i_res_scale_out = so; in.i_res_shift_out = static_cast<uint8_t>(sho); in.i_res_zp_out = static_cast<int8_t>(zo);
        in.i_res_round = kRtl[m]; in.i_res_out_int16 = false;
        const uint64_t cyc = run(dut, in, [](const GvuTop::Out &o) { return o.o_residual_pipe_done; }, false);
        const std::vector<int8_t> got = read_v0(dut);
        unsigned bad = 0, first = 0;
        for (unsigned e = 0; e < N; e++) if (got[e] != ref[e]) { if (!bad) first = e; bad++; }
        std::printf("res   run %2d: shifts %2d %2d %2d zps %4d %4d %4d %-9s | cycles %4llu | %u / %u differ", r, sha, shb, sho, za, zb, zo, kName[m],
                    static_cast<unsigned long long>(cyc), bad, N);
        if (bad) std::printf(" (first: a %d b %d model %d GvuTop %d)", A[first], B[first], ref[first], got[first]);
        if (!expr) std::printf(" (parameters not expressible on the model unit)");
        std::printf("\n");
        if (bad || !cyc || !expr) failed++;
    }
    return failed;
}

// One residual add of n elements (row-major, element r * 32 + l) on a GvuTop already out of reset, as the core-path socket
// runs it: one tile of ceil(n / 32) rows (at most 128, the int8 rows of vbank1), the lanes past n in the last row masked
// (i_res_last_lane_mask). Checks the n results against RtlElem::add and that the masked lanes of vbank0 keep their contents.
static bool res_once(GvuTop &dut, unsigned n, const char *tag)
{
    const unsigned rows = (n + 31) / 32, tail = n - 32 * (rows - 1);
    const int m = static_cast<int>(uni(0, 6));
    const int32_t sa = static_cast<int32_t>(uni(1, 2147483647LL)), sb = static_cast<int32_t>(uni(1, 2147483647LL)), so = static_cast<int32_t>(uni(1, 2147483647LL));
    const int sha = static_cast<int>(uni(20, 40)), shb = static_cast<int>(uni(20, 40)), sho = static_cast<int>(uni(20, 40));
    const int za = static_cast<int>(uni(-128, 127)), zb = static_cast<int>(uni(-128, 127)), zo = static_cast<int>(uni(-128, 127));
    std::vector<int8_t> A(n), B(n), ref(n, 0);
    for (unsigned e = 0; e < n; e++) { A[e] = static_cast<int8_t>(uni(-128, 127)); B[e] = static_cast<int8_t>(uni(-128, 127)); }
    uint64_t mc = 0;
    const bool expr = has::RtlElem::add(A.data(), B.data(), ref.data(), n, za, zb, zo, sa, sb, so, sha, shb, sho, m, true, true, 0, &mc);
    const uint8_t kKeep = 0x5A;   // contents of the lanes past n, must survive
    std::vector<SpBytes> v1((rows + 3) / 4, SpBytes(4 * GVU_LANES, 0));
    for (unsigned r = 0; r < rows; r++)
    {
        SpBytes ra(GVU_LANES, kKeep);
        for (unsigned l = 0; l < GVU_LANES && 32 * r + l < n; l++)
        {
            ra[l] = static_cast<uint8_t>(A[32 * r + l]);
            v1[r / 4][(r % 4) * GVU_LANES + l] = static_cast<uint8_t>(B[32 * r + l]);
        }
        dut.scratchpad().sram(0, 0).poke(r, ra);
    }
    for (unsigned w = 0; w < v1.size(); w++) dut.scratchpad().sram(1, 0).poke(w, v1[w]);
    GvuTop::In in;
    in.i_gvu_op = GVU_OP_RESIDUAL; in.i_num_tile = 1; in.i_token_len = rows;
    in.i_res_scale_a = sa; in.i_res_shift_a = static_cast<uint8_t>(sha); in.i_res_zp_a = static_cast<int8_t>(za);
    in.i_res_scale_b = sb; in.i_res_shift_b = static_cast<uint8_t>(shb); in.i_res_zp_b = static_cast<int8_t>(zb);
    in.i_res_scale_out = so; in.i_res_shift_out = static_cast<uint8_t>(sho); in.i_res_zp_out = static_cast<int8_t>(zo);
    in.i_res_round = kRtl[m]; in.i_res_out_int16 = false;
    in.i_res_last_lane_mask = tail < 32 ? ((1u << tail) - 1u) : 0xFFFFFFFFu;
    const uint64_t cyc = run(dut, in, [](const GvuTop::Out &o) { return o.o_residual_pipe_done; }, false);
    unsigned bad = 0, kept_bad = 0, first = 0;
    for (unsigned r = 0; r < rows; r++)
    {
        const SpBytes row = dut.scratchpad().sram(0, 0).word(r);
        for (unsigned l = 0; l < GVU_LANES; l++)
        {
            const unsigned e = 32 * r + l;
            if (e < n) { if (static_cast<int8_t>(row[l]) != ref[e]) { if (!bad) first = e; bad++; } }
            else if (row[l] != kKeep) kept_bad++;
        }
    }
    std::printf("%s n %4u rows %3u tail %2u %-9s | cycles %4llu | %u / %u differ, %u masked lanes overwritten", tag, n, rows, tail, kName[m],
                static_cast<unsigned long long>(cyc), bad, n, kept_bad);
    if (bad) std::printf(" (first: a %d b %d model %d)", A[first], B[first], ref[first]);
    if (!expr) std::printf(" (parameters not expressible on the model unit)");
    std::printf("\n");
    return !bad && !kept_bad && cyc && expr;
}

// Residual add with a partial last row: lengths that are not a multiple of 32, up to the 128 rows of one run.
static int res_tail_runs(int runs)
{
    GvuTop dut;
    GvuTop::In in;
    for (int c = 0; c < 5; c++) dut.tick(in, false);
    dut.tick(in, true);
    int failed = 0;
    for (int r = 0; r < runs; r++)
    {
        const unsigned n = r == 0 ? 4096u : r == 1 ? 1u : static_cast<unsigned>(uni(2, 4095));
        if (!res_once(dut, n, "rtail")) failed++;
    }
    return failed;
}

// Stream mode of GEMM_FUSED (the core path): one GvuTop through every run, as the socket keeps it. A run loads a new
// activation table, except every fourth run, which keeps the table after a residual add that overwrote vbank1 (D7), and
// every fifth run skips the activation. Beats come in bursts of 1..64 back to back separated by 0..100 idle cycles (more
// than 64, as between two PSM write sequences); every beat has its own scale and shift (one output channel per beat) and a
// random lane mask, empty masks included. Checks: every masked lane equals RtlObp::beat (GvuGemmEpilogue, one beat at a
// time); every beat leaves exactly GVU_GEMM_STREAM_DEPTH cycles after it entered, with its address and mask, in order; nothing leaves in a
// gap; the run ends in IDLE after the stop.
static int stream_runs(int runs)
{
    static const int kBeats = 400, kDepth = GVU_GEMM_STREAM_DEPTH;   // 11 = RTL (default), 9 = MAS build
    GvuTop dut;
    GvuTop::In in;
    for (int c = 0; c < 5; c++) dut.tick(in, false);
    dut.tick(in, true);
    int failed = 0;
    std::vector<int8_t> lut(256);
    has::RtlObp::Unit ep;
    for (int r = 0; r < runs; r++)
    {
        const bool new_table = r == 0 || r % 4 != 3, act = r % 5 != 4;
        if (new_table)
        {
            for (auto &v : lut) v = static_cast<int8_t>(uni(-128, 127));
            for (unsigned w = 0; w < 2; w++) dut.scratchpad().sram(1, 1).poke(w, SpBytes(lut.begin() + 128 * w, lut.begin() + 128 * (w + 1)));
            ep.load_act(lut.data());
        }
        const int kind = r % 3, m = static_cast<int>(uni(0, 6));
        const int zp = static_cast<int>(r % 2 ? uni(-128, 127) : uni(-32768, 32767));
        struct Beat { std::vector<int32_t> ps; int32_t scale; int shift; uint32_t mask, addr; std::vector<int32_t> ref; uint64_t t_in; };
        std::vector<Beat> beats(kBeats);
        bool expr = true;
        for (int b = 0; b < kBeats; b++)
        {
            Beat &x = beats[b];
            x.ps.resize(32); x.ref.assign(32, 0); x.t_in = 0;
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
            expr = has::RtlObp::beat(ep, x.ps, x.ref, x.mask, x.scale, x.shift, zp, m, true, act) && expr;
        }
        // into GEMM_FUSED_ST1 (through LUT_ACT_INIT for a new table)
        in = GvuTop::In();
        in.i_gvu_op = GVU_OP_GEMM_FUSED; in.i_gemm_stream = true; in.i_gemm_round = kRtl[m]; in.i_gemm_zp16 = zp;
        in.i_gemm_skip_act = !act; in.i_vbank1_buf_sel = true; in.i_gemm_lut_reload = new_table && r > 0; in.i_pipeline_en = true;
        int enter = 0;
        while (dut.state() != GEMM_FUSED_ST1 && enter < 100)
        {
            dut.tick(in, true);
            if (dut.state() == LUT_ACT_INIT) in.i_gemm_lut_reload = false;
            enter++;
        }
        in.i_gemm_lut_reload = false;
        // beats with bursts and gaps; outputs checked in the cycle they leave
        uint64_t cyc = 0;
        int sent = 0, got = 0, burst = 0, gap = 0;
        unsigned bad = 0, lat_bad = 0, meta_bad = 0, extra = 0;
        int first = -1;
        while ((got < kBeats || sent < kBeats) && cyc < 100000)
        {
            bool send = false;
            if (sent < kBeats)
            {
                if (burst == 0 && gap == 0) { burst = static_cast<int>(uni(1, 64)); gap = static_cast<int>(uni(0, 3) == 0 ? uni(65, 100) : uni(0, 20)); }
                if (burst > 0) { send = true; burst--; }
                else gap--;
            }
            in.i_psum_valid = send;
            if (send)
            {
                Beat &x = beats[sent];
                for (unsigned l = 0; l < 32; l++) in.i_psum_vec[l] = x.ps[l];
                in.i_psum_scale = x.scale; in.i_psum_shift = static_cast<uint8_t>(x.shift); in.i_psum_mask = x.mask; in.i_psum_addr = x.addr;
                x.t_in = cyc;
            }
            const GvuTop::Out o = dut.tick(in, true);
            if (o.o_gemm_fused_valid)
            {
                if (got >= sent) extra++;
                else
                {
                    const Beat &x = beats[got];
                    if (cyc != x.t_in + kDepth) lat_bad++;
                    if (o.o_gemm_fused_addr != x.addr || o.o_gemm_fused_mask != x.mask) meta_bad++;
                    for (unsigned l = 0; l < 32; l++)
                        if (((x.mask >> l) & 1u) && o.o_gemm_fused_data[l] != static_cast<int8_t>(x.ref[l])) { if (first < 0) first = got; bad++; }
                    got++;
                }
            }
            if (send) sent++;
            cyc++;
        }
        // stop: back to IDLE once the pipeline is empty
        in.i_psum_valid = false; in.i_gemm_stream_stop = true;
        int stop = 0;
        bool done = false;
        while (dut.state() != IDLE && stop < 50) { const GvuTop::Out o = dut.tick(in, true); done = done || o.o_gemm_fused_pipe_done; if (o.o_gemm_fused_valid) extra++; stop++; }
        in.i_pipeline_en = false; in.i_gemm_stream_stop = false;
        for (int k = 0; k < 3; k++) dut.tick(in, true);
        const bool ok = !bad && !lat_bad && !meta_bad && !extra && got == kBeats && done && dut.state() == IDLE && expr;
        std::printf("stream run %2d: %s act %d zp %6d %-9s kind %d | enter %2d cycles, %5llu cycles for %d beats | %u lanes differ, latency other than %d: %u, addr/mask wrong %u, beats out of nothing %u, stop %s",
                    r, new_table ? (r ? "reload" : "first ") : "keep  ", int(act), zp, kName[m], kind, enter, static_cast<unsigned long long>(cyc), kBeats, bad,
                    kDepth, lat_bad, meta_bad, extra, done && dut.state() == IDLE ? "ok" : "FAILED");
        if (bad) std::printf(" (first beat %d)", first);
        if (!expr) std::printf(" (parameters not expressible on the model unit)");
        std::printf("\n");
        if (!ok) failed++;
        // D7: a residual add on the same unit before the run that keeps its table (it overwrites vbank1)
        if (r % 4 == 2 && !res_once(dut, static_cast<unsigned>(uni(1, 4096)), "  shared res")) failed++;
    }
    return failed;
}

int main(int argc, char **argv)
{
    const int runs = argc > 1 ? std::atoi(argv[1]) : 12;
    g_rng.seed(argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 1);
    int f = gemm_runs(runs) + res_runs(runs);    // tile mode, as the colleague's test benches drive it
    f += stream_runs(runs) + res_tail_runs(runs); // stream mode (core path) and residual adds with a partial last row
    std::printf("RESULT: %s (runs that differ or did not finish: %d of %d)\n", f ? "FAIL" : "PASS", f, 4 * runs + (runs + 1) / 4);
    return f ? 1 : 0;
}
