// tb_rtl_ref_gvu_g2.cpp -- checks the staged / stateful blocks ported from the RTL of the vector unit:
// has/rtl_ref_gvu_lut.h (LUT unit), has/rtl_ref_gvu_vrf.h (register file, stream registers), has/rtl_ref_gvu_scratchpad.h,
// has/rtl_ref_gvu_vlsu.h (load / store unit).
// Kinds of check, printed with each line:
//   [arith]  against an independent computation (random + boundary vectors)
//   [hw-tb]  the configuration of an RTL testbench (tb_gvu_lut_unit, whose golden files are not part of the RTL
//            delivered), replayed on the ported block
//   [cycle]  latency and ordering of the clocked models
//   [note]   behaviour of the RTL as written that is counted and printed, not judged
// None of these is a comparison with an RTL simulation.
// build: g++ -std=c++17 -O2 -Wall -I. tools/has/tb_rtl_ref_gvu_g2.cpp -o tb_rtl_ref_gvu_g2
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <deque>
#include <random>
#include "has/rtl_ref_gvu_lut.h"
#include "has/rtl_ref_gvu_scratchpad.h"
#include "has/rtl_ref_gvu_vlsu.h"
#include "has/rtl_ref_gvu_vrf.h"

using namespace rtl_ref_gvu;

static int g_fail = 0;
static uint64_t g_checks = 0;
static std::mt19937_64 g_rng(0x47565532u);
static void section(const char *kind, const char *name, uint64_t n, int fail_before)
{
    printf("%-7s %-66s %8" PRIu64 "  %s\n", kind, name, n, g_fail == fail_before ? "ok" : "MISMATCH");
}
#define CHECK(cond, ...) do { g_checks++; if (!(cond)) { if (g_fail < 20) { printf("  mismatch: "); printf(__VA_ARGS__); printf("\n"); } g_fail++; } } while (0)

static uint32_t rnd_x()
{
    switch (g_rng() % 8)
    {
    case 0: return 1u + static_cast<uint32_t>(g_rng() % 300);
    case 1: return 0xFFFFFFFFu - static_cast<uint32_t>(g_rng() % 300);
    case 2: { const int e = static_cast<int>(g_rng() % 32); return 1u << e; }
    case 3: { const int e = 1 + static_cast<int>(g_rng() % 31); return (1u << e) - 1u; }
    case 4: { const int e = 8 + static_cast<int>(g_rng() % 24); return (0x1FFu << (e - 8)) | static_cast<uint32_t>(g_rng() & ((1u << (e - 8)) - 1)); } // index 255
    default: { const uint32_t v = static_cast<uint32_t>(g_rng()) >> (g_rng() % 32); return v ? v : 1u; }
    }
}

// Host-side tables in the format the functional model uses: Q14, recip 256 entries, rsqrt 512 entries ({E[0], index}).
static std::vector<uint16_t> table_recip()
{
    std::vector<uint16_t> t(256);
    for (int i = 0; i < 256; i++) t[static_cast<size_t>(i)] = static_cast<uint16_t>(std::lround(std::ldexp(1.0 / (1.0 + i / 256.0), 14)));
    return t;
}
static std::vector<uint16_t> table_rsqrt()
{
    std::vector<uint16_t> t(512);
    for (int p = 0; p < 2; p++)
        for (int i = 0; i < 256; i++) t[static_cast<size_t>(p * 256 + i)] = static_cast<uint16_t>(std::lround(std::ldexp(1.0 / std::sqrt((1.0 + i / 256.0) * (1 << p)), 14)));
    return t;
}

// Independent PWL lookup: mantissa with 16 bits below the leading one, table index = its top 8 bits, fraction = low 8.
static uint16_t ref_pwl(const std::vector<uint16_t> &T, bool rsqrt, unsigned W, unsigned sh, uint32_t x, uint32_t *e_shift)
{
    int E = 0;
    for (int b = 31; b >= 0; b--) if ((x >> b) & 1) { E = b; break; }
    const uint64_t mant = E >= static_cast<int>(W) ? (static_cast<uint64_t>(x) >> (E - static_cast<int>(W))) : ((static_cast<uint64_t>(x) << (static_cast<int>(W) - E)) & 0xFFFFFFFFull);
    const size_t idx = static_cast<size_t>(((rsqrt ? (E & 1) : 0) << 8) | ((mant >> 8) & 0xFF));
    const size_t nxt = idx + 1 < T.size() ? idx + 1 : idx;
    const int frac = static_cast<int>(mant & 0xFF);
    int delta = static_cast<int>(T[nxt]) - static_cast<int>(T[idx]);
    delta = ((delta + 128) & 0xFF) - 128;                          // 8 signed bits
    const int prod = delta * frac;
    const int add = prod >= 0 ? (prod >> sh) : -((-prod + (1 << sh) - 1) >> sh);   // floor division by 2^sh
    *e_shift = static_cast<uint32_t>(rsqrt ? E / 2 : E);
    return static_cast<uint16_t>(T[idx] + add);
}

static void test_lut_value()
{
    int f0 = g_fail; uint64_t n = 0;
    LutMem mem;
    const std::vector<uint16_t> TR = table_recip(), TS = table_rsqrt();
    mem.load16(LUT_RECIP, TR);
    mem.load16(LUT_RSQRT, TS);
    for (unsigned a = 0; a < 512; a++)
    {
        CHECK(mem.rdata16(LUT_RSQRT, a) == TS[a] && mem.rdata16_nxt(LUT_RSQRT, a) == TS[a < 511 ? a + 1 : a], "rsqrt table readback %u", a);
        if (a < 256) CHECK(mem.rdata16(LUT_RECIP, a) == TR[a] && mem.rdata16_nxt(LUT_RECIP, a) == TR[a < 255 ? a + 1 : a], "recip table readback %u", a);
        n++;
    }
    {   // the write port: internal address, 3-bit write counter (8 writes of 16 bytes reach 128 of the 256 act entries)
        LutMem m2; std::vector<uint8_t> d(16);
        for (int wr = 0; wr < 9; wr++)
        {
            for (int i = 0; i < 16; i++) d[static_cast<size_t>(i)] = static_cast<uint8_t>(0x10 * (wr + 1) + i);
            CHECK(m2.lut_waddr() == static_cast<unsigned>(wr < 8 ? 16 * wr : 0) && m2.o_init_done(LUT_ACT) == (wr == 7), "act write %d: address %u done %d", wr, m2.lut_waddr(), m2.o_init_done(LUT_ACT) ? 1 : 0);
            m2.tick(true, true, LUT_ACT, d.data());
        }
        CHECK(m2.rdata8(LUT_ACT, 0) == 0x90 && m2.rdata8(LUT_ACT, 15) == 0x9F && m2.rdata8(LUT_ACT, 16) == 0x20 && m2.rdata8(LUT_ACT, 127) == 0x8F, "act: the ninth write lands at the start of the table, the rest of the lower half keeps writes 2..8");
        CHECK(m2.rdata8(LUT_ACT, 128) == 0 && m2.rdata8(LUT_ACT, 255) == 0 && m2.rdata8(LUT_EXP, 0) == 0, "act: the upper half of the table is not reachable through the port");
        LutMem m3;
        for (int wr = 0; wr < 8; wr++) { for (int i = 0; i < 16; i++) d[static_cast<size_t>(i)] = static_cast<uint8_t>(wr * 8 + i / 2); m3.tick(true, true, LUT_RSQRT, d.data()); }
        CHECK(m3.lut_waddr() == 0 && m3.rdata16(LUT_RSQRT, 63) == 0x3F3F && m3.rdata16(LUT_RSQRT, 64) == 0 && m3.rdata16(LUT_RECIP, 0) == 0, "rsqrt: 8 writes of 8 words reach the first 64 of 512 words");
    }
    section("[arith]", "gvu_lut_mem: tables, write port (own address, 3-bit counter), lut[i+1] held at the end", n, f0);

    f0 = g_fail; n = 0;
    for (int it = 0; it < 600000; it++)
    {
        const bool rs = it & 1;
        const uint32_t x = it < 4 ? static_cast<uint32_t>(it / 2) : rnd_x();
        const unsigned sh = (it % 97 == 0) ? static_cast<unsigned>(g_rng() % 12) : 8u;
        const LutValue v = lut_value(mem, rs ? LUT_RSQRT : LUT_RECIP, LUT_PWL, 7, 16, sh, x);
        if (x == 0) { CHECK(v.input_zero && v.data == 0, "PWL zero input"); n++; continue; }
        uint32_t es = 0;
        const uint16_t e = ref_pwl(rs ? TS : TR, rs, 16, sh, x, &es);
        CHECK(v.data == e && v.e_shift == es && !v.input_zero, "PWL %s x=%08x sh=%u got=%04x/%u exp=%04x/%u", rs ? "rsqrt" : "recip", x, sh, v.data, v.e_shift, e, es);
        n++;
    }
    section("[arith]", "LUT PWL value (recip, rsqrt; W 16; Q14 tables 256 / 512)", n, f0);

    f0 = g_fail; n = 0;
    std::vector<uint16_t> ramp(512);
    for (int i = 0; i < 512; i++) ramp[static_cast<size_t>(i)] = static_cast<uint16_t>(0x4000 + i);      // entry = its address
    LutMem lm; lm.load16(LUT_RECIP, std::vector<uint16_t>(ramp.begin(), ramp.begin() + 256)); lm.load16(LUT_RSQRT, ramp);
    for (int it = 0; it < 300000; it++)
    {
        const bool rs = it & 1; const unsigned base = static_cast<unsigned>(g_rng() % 32);
        const uint32_t x = rnd_x();
        const LutValue v = lut_value(lm, rs ? LUT_RSQRT : LUT_RECIP, LUT_LOG_SCALE, base, 16, 8, x);
        const int E = 31 - __builtin_clz(x);
        const unsigned fine = E >= 4 ? (x >> (E - 4)) & 15u : 0u;
        const unsigned idx = ((static_cast<unsigned>(E - static_cast<int>(base)) & 31u) << 4) | fine;
        CHECK(v.data == static_cast<uint16_t>(0x4000 + (rs ? idx : (idx & 0xFF))) && v.e_shift == static_cast<uint32_t>(rs ? E / 2 : E), "log-scale x=%08x base=%u got=%04x idx=%x", x, base, v.data, idx);
        n++;
    }
    section("[arith]", "LUT log-scale index {E - base, 4 fine bits} (fine bits 0 below 16)", n, f0);
}

// Streams of lookups through the unit with a random ready, 32 lanes with the same input (as tb_gvu_lut_unit drives it:
// recip, PWL, i_base 7, i_W 16, i_lerp_shift 8, lane 0 checked in order). Returns the minimum latency seen.
static int run_stream(LutType sel, LutIdxMode mode, const LutParams &p, bool with_zeros, uint64_t *n_ok, uint64_t *n_bad, uint64_t *n_missing, bool always_ready = false)
{
    LutUnit dut(p);
    const std::vector<uint16_t> TR = table_recip(), TS = table_rsqrt();
    dut.mem.load16(LUT_RECIP, TR); dut.mem.load16(LUT_RSQRT, TS);
    std::vector<uint8_t> act(256);
    for (int i = 0; i < 256; i++) act[static_cast<size_t>(i)] = static_cast<uint8_t>(255 - i);
    dut.mem.load8(LUT_ACT, act); dut.mem.load8(LUT_EXP, act);
    std::deque<std::pair<uint32_t, int>> exp;                      // expected lane-0 data, cycle accepted
    int lat_min = 1 << 30;
    bool v = false; uint32_t x = 1; uint64_t accepted = 0;
    *n_ok = *n_bad = 0;
    for (int cyc = 0; cyc < 40000; cyc++)
    {
        if (!v) { v = (g_rng() % 4) != 0; x = (with_zeros && g_rng() % 4 == 0) ? 0u : rnd_x(); }
        LutUnit::In in;
        in.i_lookup_valid = v; in.i_lut_rsel = sel; in.i_lut_idx_mode = mode; in.i_base = 7; in.i_W = 16; in.i_lerp_shift = 8;
        in.i_lookup_ready = always_ready || (g_rng() % 4) != 0;
        in.i_32b_lookup_idx.fill(x); in.i_8b_lookup_idx.fill(x & 0xFF);
        const LutUnit::Out o = dut.eval(in);
        const bool acc = v && o.o_lookup_ready;
        if (acc)
        {
            const uint32_t e = lut_is_direct(sel) ? act[x & 0xFF] : lut_value(dut.mem, sel, mode, 7, 16, 8, x).data;
            exp.push_back({e, cyc}); accepted++;
        }
        if (o.o_lookup_valid && in.i_lookup_ready)
        {
            const uint32_t got = lut_is_direct(sel) ? o.o_8b_lookup_data[0] : o.o_16b_lookup_data[0];
            if (!exp.empty() && got == exp.front().first)
            {
                const int lat = cyc - exp.front().second;
                lat_min = lat < lat_min ? lat : lat_min;
                (*n_ok)++; exp.pop_front();
            }
            else { (*n_bad)++; if (!exp.empty()) exp.pop_front(); }
        }
        dut.tick(in);
        if (acc) v = false;
    }
    *n_missing = exp.size() > 8 ? exp.size() : 0;                  // more than the pipeline can still hold
    return lat_min;
}

static void test_lut_stream()
{
    const struct { LutType sel; LutIdxMode mode; bool mul_pipe, shift_pipe; const char *name; } cfg[] = {
        {LUT_RECIP, LUT_PWL, true, false, "recip PWL, RTL defaults (tb_gvu_lut_unit configuration)"},
        {LUT_RSQRT, LUT_PWL, true, false, "rsqrt PWL, RTL defaults"},
        {LUT_RECIP, LUT_PWL, false, false, "recip PWL, LERP_MUL_PIPE 0"},
        {LUT_RECIP, LUT_PWL, true, true, "recip PWL, LERP_SHIFT_PIPE 1"},
        {LUT_RECIP, LUT_LOG_SCALE, true, false, "recip log-scale"},
        {LUT_EXP, LUT_PWL, true, false, "exp direct"},
    };
    for (const auto &c : cfg)
    {
        const int f0 = g_fail;
        LutParams p; p.LERP_MUL_PIPE = c.mul_pipe; p.LERP_SHIFT_PIPE = c.shift_pipe;
        uint64_t ok = 0, bad = 0, miss = 0;
        // Log-scale: the result is taken after pipe1 while the beat goes on into pipe2 .. pipe4, so it is visible for one
        // cycle only; the stream is checked with the consumer always ready and the back-pressure case is a [note] below.
        const int lat = run_stream(c.sel, c.mode, p, false, &ok, &bad, &miss, c.mode == LUT_LOG_SCALE && !lut_is_direct(c.sel));
        CHECK(bad == 0 && miss == 0, "LUT stream %s: %" PRIu64 " wrong, %" PRIu64 " missing", c.name, bad, miss);
        CHECK(lat == lat_lut(c.sel, c.mode, p), "LUT stream %s: latency %d, constant %d", c.name, lat, lat_lut(c.sel, c.mode, p));
        char nm[120]; snprintf(nm, sizeof nm, "LutUnit %s: latency %d", c.name, lat);
        section(c.sel == LUT_RECIP && c.mode == LUT_PWL && c.mul_pipe && !c.shift_pipe ? "[hw-tb]" : "[cycle]", nm, ok, f0);
    }
    {
        uint64_t ok = 0, bad = 0, miss = 0;
        run_stream(LUT_RECIP, LUT_LOG_SCALE, LutParams(), false, &ok, &bad, &miss);
        printf("[note]  LutUnit recip log-scale, consumer ready 3 cycles in 4, no zero input: %" PRIu64 " beats in order with the right value, %" PRIu64 " wrong, %" PRIu64 " never delivered\n", ok, bad, miss);
    }
    // Zero inputs mixed into the stream: the zero flag travels with the beat, so a zero beat takes the same number of
    // cycles as any other, keeps its place in the order and comes out with data 0. Log-scale with the consumer always
    // ready (its result is visible for one cycle only, see above).
    for (int m = 0; m < 2; m++)
    {
        const int f0 = g_fail;
        const LutIdxMode mode = m ? LUT_LOG_SCALE : LUT_PWL;
        uint64_t ok = 0, bad = 0, miss = 0;
        const int lat = run_stream(LUT_RECIP, mode, LutParams(), true, &ok, &bad, &miss, m == 1);
        CHECK(bad == 0 && miss == 0, "LUT stream with zero inputs, %s: %" PRIu64 " wrong, %" PRIu64 " missing", m ? "log-scale" : "PWL", bad, miss);
        CHECK(lat == lat_lut(LUT_RECIP, mode), "LUT stream with zero inputs, %s: latency %d, constant %d", m ? "log-scale" : "PWL", lat, lat_lut(LUT_RECIP, mode));
        char nm[120]; snprintf(nm, sizeof nm, "LutUnit recip %s, one beat in four is 0: every beat in order, zero beats give 0, latency %d", m ? "log-scale" : "PWL", lat);
        section("[cycle]", nm, ok, f0);
    }
    {   // log-scale: the data leaves after pipe1, o_e_shift after pipe4
        LutLookupLane lane; LutMem mem; mem.load16(LUT_RECIP, table_recip());
        LutLookupLane::In in; in.i_lut_idx_mode = LUT_LOG_SCALE; in.i_lut_sel = LUT_RECIP; in.i_base = 0;
        in.i_lookup_valid = true; in.i_32b_lookup_idx = 1u << 20; lane.tick(in, mem);
        in.i_32b_lookup_idx = 1u << 3; lane.tick(in, mem);
        const LutLookupLane::Out o = lane.eval(in, mem);
        printf("[note]  log-scale: first result valid=%d (input 2^20, E 20) while o_e_shift=%u in that cycle (it follows pipe4, two registers later)\n", o.o_lookup_valid ? 1 : 0, o.o_e_shift);
    }
}

static void test_vrf()
{
    int f0 = g_fail; uint64_t n = 0;
    Vrf dut;
    std::vector<uint8_t> ref(1024, 0);
    for (int cyc = 0; cyc < 20000; cyc++)
    {
        Vrf::Writes w;
        std::vector<uint8_t> next = ref;
        const int rows[4] = {1, 2, 4, 8}, ports[4] = {2, 2, 3, 2};
        std::vector<Vrf::Write> *dst[4] = {&w.int8, &w.int16, &w.int32, &w.int64};
        for (int k = 0; k < 4; k++)
            for (int j = 0; j < ports[k]; j++)
            {
                Vrf::Write q; q.wen = g_rng() % 6 == 0; q.waddr = static_cast<unsigned>(g_rng() % (32 / rows[k]));
                q.wdata.resize(static_cast<size_t>(rows[k] * 32));
                for (auto &b : q.wdata) b = static_cast<uint8_t>(g_rng());
                if (q.wen) for (int i = 0; i < rows[k] * 32; i++) next[static_cast<size_t>(static_cast<int>(q.waddr) * rows[k] * 32 + i)] = q.wdata[static_cast<size_t>(i)];
                dst[k]->push_back(q);
            }
        dut.tick(w);
        ref = next;
        for (int k = 0; k < 4; k++)
        {
            const unsigned a = static_cast<unsigned>(g_rng() % (32 / rows[k]));
            const Bytes d = dut.read(rows[k], a);
            bool same = d.size() == static_cast<size_t>(rows[k] * 32);
            for (size_t i = 0; same && i < d.size(); i++) same = d[i] == ref[static_cast<size_t>(a) * static_cast<size_t>(rows[k]) * 32 + i];
            CHECK(same, "vrf read rows=%d addr=%u", rows[k], a);
            n++;
        }
    }
    section("[arith]", "vrf.sv: aliased 1 KB, ports 2/2/3/2, write order int8 < int16 < int32 < int64", n, f0);

    // Stream registers: full-width mode and split mode keep order; each sub-register is an independent stream.
    f0 = g_fail; n = 0;
    const VstreamReg::Kind kinds[3] = {VstreamReg::INT32, VstreamReg::INT64_T0, VstreamReg::INT64_T1};
    for (int kk = 0; kk < 3; kk++)
        for (int split = 0; split < 2; split++)
        {
            VstreamReg dut2(kinds[kk]);
            const int N = dut2.subregs(), SB = dut2.sub_bytes();
            const unsigned mode = split ? (kinds[kk] == VstreamReg::INT32 ? 2u : 1u) : 0u;
            std::deque<Bytes> q[4];
            bool v[4] = {false, false, false, false}; Bytes d[4];
            for (int cyc = 0; cyc < 6000; cyc++)
            {
                VstreamReg::In in; in.i_cfg_mode = mode;
                const int streams = split ? N : 1;
                for (int s = 0; s < streams; s++)
                    if (!v[s] && (g_rng() & 1)) { v[s] = true; d[s].resize(static_cast<size_t>(split ? SB : N * SB)); for (auto &b : d[s]) b = static_cast<uint8_t>(g_rng()); }
                bool rdy[4];
                for (int s = 0; s < streams; s++) rdy[s] = (g_rng() % 3) != 0;
                if (split) for (int s = 0; s < N; s++) { in.sub[static_cast<size_t>(s)].valid = v[s]; in.sub[static_cast<size_t>(s)].data = d[s]; in.sub[static_cast<size_t>(s)].ready = rdy[s]; }
                else { in.full.valid = v[0]; in.full.data = d[0]; in.full.ready = rdy[0]; }
                const VstreamReg::Out o = dut2.eval(in);
                for (int s = 0; s < streams; s++)
                {
                    const VstreamReg::PortOut &po = split ? o.sub[static_cast<size_t>(s)] : o.full;
                    if (po.o_valid && rdy[s]) { CHECK(!q[s].empty() && po.o_data == q[s].front(), "stream reg kind %d split %d stream %d", kk, split, s); if (!q[s].empty()) q[s].pop_front(); n++; }
                    if (v[s] && po.o_ready) q[s].push_back(d[s]);
                }
                dut2.tick(in);
                for (int s = 0; s < streams; s++) { const VstreamReg::PortOut &po = split ? o.sub[static_cast<size_t>(s)] : o.full; if (v[s] && po.o_ready) v[s] = false; }
            }
        }
    section("[cycle]", "vstream_reg_int32 / int64_type0 / type1: full and split modes, order kept", n, f0);
    {   // v32int8 mode of vstream_reg_int32: the registers follow i_vint8_ready, the v32int32 view stays silent
        const int f1 = g_fail;
        VstreamReg w(VstreamReg::INT32);
        VstreamReg::In in; in.i_cfg_mode = 2;
        for (int s = 0; s < 4; s++) { in.sub[static_cast<size_t>(s)].valid = true; in.sub[static_cast<size_t>(s)].ready = true; in.sub[static_cast<size_t>(s)].data.assign(32, static_cast<uint8_t>(s + 1)); }
        in.full.ready = false;
        w.tick(in);
        VstreamReg::In idle; idle.i_cfg_mode = 2;
        const VstreamReg::Out o = w.eval(idle);
        CHECK(o.sub[0].o_valid && o.sub[3].o_valid && !o.full.o_valid && !o.half[0].o_valid, "v32int8 mode: the registers load with i_vint8_ready, only the v32int8 view is valid");
        section("[cycle]", "vstream_reg_int32, v32int8 mode: ready taken from the v32int8 ports", 1, f1);
    }
}

// Reference of one memory word write as the memory wrappers are written: mask bit i moves bits [i, i + 8) of the data.
static void ref_write(std::vector<uint8_t> &word, const SpBytes &mask, const SpBytes &data, bool as_written)
{
    const int wb = static_cast<int>(word.size());
    auto mbit = [&](int i) { return static_cast<size_t>(i / 8) < mask.size() && ((mask[static_cast<size_t>(i / 8)] >> (i % 8)) & 1); };
    for (int i = 0; i < wb; i++)
    {
        if (!mbit(i)) continue;
        if (!as_written) { word[static_cast<size_t>(i)] = static_cast<size_t>(i) < data.size() ? data[static_cast<size_t>(i)] : 0; continue; }
        for (int k = i; k < i + 8; k++)
        {
            const bool d = static_cast<size_t>(k / 8) < data.size() && ((data[static_cast<size_t>(k / 8)] >> (k % 8)) & 1);
            word[static_cast<size_t>(k / 8)] = static_cast<uint8_t>((word[static_cast<size_t>(k / 8)] & ~(1u << (k % 8))) | (d ? (1u << (k % 8)) : 0u));
        }
    }
}
static SpBytes ones(int nbits) { SpBytes m(static_cast<size_t>((nbits + 7) / 8), 0); for (int k = 0; k < nbits; k++) m[static_cast<size_t>(k / 8)] = static_cast<uint8_t>(m[static_cast<size_t>(k / 8)] | (1u << (k % 8))); return m; }
static SpBytes rnd_b(size_t n) { SpBytes r(n); for (auto &x : r) x = static_cast<uint8_t>(g_rng()); return r; }
static SpBytes shifted(const SpBytes &d, int off, int total) { SpBytes r(static_cast<size_t>(total), 0); for (size_t k = 0; k < d.size() && off + static_cast<int>(k) < total; k++) r[static_cast<size_t>(off) + k] = d[k]; return r; }

static void test_scratchpad_directed()
{
    const int f0 = g_fail; uint64_t n = 0;
    for (int as_written = 1; as_written >= 0; as_written--)
    {
        ScratchpadParams p; p.wmask_as_written = as_written != 0;
        GvuScratchpad dut(p);
        typedef GvuScratchpad::In In;
        // sbank port 0: write a word with the full mask, read it back two clocks after the read access
        In w; w.i_sbank_en_0 = true; w.i_sbank_wen_0 = true; w.i_sbank_addr_0 = 5; w.i_sbank_wmask_0 = 0xF; w.i_sbank_wdata_0 = 0xFFFFFFFFu; dut.tick(w);
        In r; r.i_sbank_en_0 = true; r.i_sbank_addr_0 = 5;
        In idle;
        dut.tick(r);
        const uint32_t after1 = dut.out(idle).o_sbank_rdata_0;
        dut.tick(idle);
        const uint32_t after2 = dut.out(idle).o_sbank_rdata_0;
        CHECK(after1 == 0 && after2 == (as_written ? 0x000007FFu : 0xFFFFFFFFu), "sbank port 0 (as_written %d): after 1 clock %08x, after 2 clocks %08x", as_written, after1, after2);
        // sbank port 1: byte 1 of the word
        In wb; wb.i_sbank_en_1 = true; wb.i_sbank_wen_1 = true; wb.i_sbank_addr_1 = 4 * 9 + 1; wb.i_sbank_wmask_1 = true; wb.i_sbank_wdata_1 = 0xFF; dut.tick(wb);
        In rb; rb.i_sbank_en_0 = true; rb.i_sbank_addr_0 = 9; dut.tick(rb); dut.tick(idle);
        CHECK(dut.out(idle).o_sbank_rdata_0 == (as_written ? 0x00000100u : 0x0000FF00u), "sbank port 1 byte write (as_written %d): word %08x", as_written, dut.out(idle).o_sbank_rdata_0);
        // vbank1 port 0: a v32int32 row with the full mask
        In wv; wv.i_vbank1_en_0 = true; wv.i_vbank1_wen_0 = true; wv.i_vbank1_addr_0 = 3; wv.i_vbank1_wmask_0 = ones(128); wv.i_vbank1_wdata_0 = SpBytes(128, 0xFF); dut.tick(wv);
        In rv; rv.i_vbank1_en_0 = true; rv.i_vbank1_addr_0 = 3; dut.tick(rv); dut.tick(idle);
        const SpBytes row = dut.out(idle).o_vbank1_rdata_0;
        int set = 0; for (uint8_t x : row) for (int k = 0; k < 8; k++) set += (x >> k) & 1;
        CHECK(set == (as_written ? 135 : 1024), "vbank1 port 0 full-mask write (as_written %d): %d bits changed", as_written, set);
        // ping-pong: the other buffer of vbank1 does not hold the row; the narrow ports slice the same word
        In ro = rv; ro.i_vbank1_buf_sel = true; dut.tick(ro); In io; io.i_vbank1_buf_sel = true; dut.tick(io);
        int set_o = 0; for (uint8_t x : dut.out(io).o_vbank1_rdata_0) set_o += x != 0;
        CHECK(set_o == 0, "vbank1 buffer 1 is separate from buffer 0");
        In r2; r2.i_vbank1_port_sel = 2; r2.i_vbank1_en_2 = true; r2.i_vbank1_addr_2 = 4 * 3 + 0; dut.tick(r2);
        In i2; i2.i_vbank1_port_sel = 2; i2.i_vbank1_addr_2 = 4 * 3 + 0; dut.tick(i2);
        In i3 = i2; i3.i_vbank1_addr_2 = 4 * 3 + 3;
        const SpBytes q0 = dut.out(i2).o_vbank1_rdata_2, q0b = dut.out(i3).o_vbank1_rdata_2;
        In r3 = r2; r3.i_vbank1_addr_2 = 4 * 3 + 3; dut.tick(r3); dut.tick(i2);
        const SpBytes q3 = dut.out(i2).o_vbank1_rdata_2;
        CHECK(q0.size() == 32 && q0[0] == row[0] && q0[31] == row[31] && q0b == q0 && q3[0] == row[96] && q3[31] == row[127],
              "vbank1 port 2 reads the quarter of the word chosen by the address of the read access, whatever the address of the output cycle");
        n += 6;
    }
    section("[cycle]", "gvu_scratchpad: read data two clocks after the address, mask as written, ping-pong, narrow ports", n, f0);
}

// Random accesses on every accelerator port against reference memories kept by this test bench (two buffers per bank),
// with the two-clock read path modelled separately (memory output register, then the pipeline register).
static void test_scratchpad_random()
{
    const int f0 = g_fail; uint64_t n = 0;
    const ScratchpadParams P;
    GvuScratchpad dut(P);
    const int depth[4] = {P.VBANK0_DEPTH, P.VBANK1_DEPTH, P.VBANK2_DEPTH, P.SBANK_DEPTH}, wb[4] = {32, 128, 128, 4};
    std::vector<std::vector<uint8_t>> mem[4][2];
    for (int k = 0; k < 4; k++) for (int b = 0; b < 2; b++) mem[k][b].assign(static_cast<size_t>(depth[k]), std::vector<uint8_t>(static_cast<size_t>(wb[k]), 0));
    // memory output registers per buffer (sbank: two ports) and the pipeline registers
    SpBytes q[4][2][2], pr[4][2];
    unsigned dq[4][2] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};   // part selects delayed by two clocks: sbank port 1, vbank1 port 1, vbank1 port 2, vbank2 port 1
    for (int k = 0; k < 4; k++) { for (int b = 0; b < 2; b++) for (int pt = 0; pt < 2; pt++) q[k][b][pt].assign(static_cast<size_t>(wb[k]), 0); for (int pt = 0; pt < 2; pt++) pr[k][pt].assign(static_cast<size_t>(wb[k]), 0); }
    auto ra = [&](int d) { return static_cast<unsigned>(g_rng() % 4 ? g_rng() % 6 : g_rng() % static_cast<uint64_t>(d)); };
    auto rm = [&](int bits) { SpBytes m = (g_rng() & 1) ? ones(bits) : rnd_b(static_cast<size_t>(bits / 8)); return m; };
    GvuScratchpad::In i;
    for (int cyc = 0; cyc < 60000; cyc++)
    {
        if (g_rng() % 50 == 0) i.i_sbank_buf_sel = !i.i_sbank_buf_sel;
        if (g_rng() % 50 == 0) i.i_vbank0_buf_sel = !i.i_vbank0_buf_sel;
        if (g_rng() % 50 == 0) i.i_vbank1_buf_sel = !i.i_vbank1_buf_sel;
        if (g_rng() % 50 == 0) i.i_vbank2_buf_sel = !i.i_vbank2_buf_sel;
        i.i_sbank_en_0 = g_rng() % 3; i.i_sbank_wen_0 = g_rng() % 3 == 0; i.i_sbank_addr_0 = ra(depth[3]); i.i_sbank_wmask_0 = static_cast<unsigned>(g_rng() & 0xF); i.i_sbank_wdata_0 = static_cast<uint32_t>(g_rng());
        i.i_sbank_en_1 = g_rng() % 3; i.i_sbank_wen_1 = g_rng() % 3 == 0; i.i_sbank_addr_1 = (ra(depth[3]) << 2) | static_cast<unsigned>(g_rng() & 3); i.i_sbank_wmask_1 = g_rng() & 1; i.i_sbank_wdata_1 = static_cast<uint8_t>(g_rng());
        i.i_vbank0_en = g_rng() % 3; i.i_vbank0_wen = g_rng() % 3 == 0; i.i_vbank0_addr = ra(depth[0]); i.i_vbank0_wmask = rm(32); i.i_vbank0_wdata = rnd_b(32);
        i.i_vbank1_port_sel = static_cast<unsigned>(g_rng() % 4);
        i.i_vbank1_en_0 = g_rng() % 3; i.i_vbank1_wen_0 = g_rng() % 3 == 0; i.i_vbank1_addr_0 = ra(depth[1]); i.i_vbank1_wmask_0 = rm(128); i.i_vbank1_wdata_0 = rnd_b(128);
        i.i_vbank1_en_1 = g_rng() % 3; i.i_vbank1_wen_1 = g_rng() % 3 == 0; i.i_vbank1_addr_1 = (ra(depth[1]) << 1) | static_cast<unsigned>(g_rng() & 1); i.i_vbank1_wmask_1 = rm(64); i.i_vbank1_wdata_1 = rnd_b(64);
        i.i_vbank1_en_2 = g_rng() % 3; i.i_vbank1_wen_2 = g_rng() % 3 == 0; i.i_vbank1_addr_2 = (ra(depth[1]) << 2) | static_cast<unsigned>(g_rng() & 3); i.i_vbank1_wmask_2 = rm(32); i.i_vbank1_wdata_2 = rnd_b(32);
        i.i_vbank2_port_sel = g_rng() & 1;
        i.i_vbank2_en_0 = g_rng() % 3; i.i_vbank2_wen_0 = g_rng() % 3 == 0; i.i_vbank2_addr_0 = ra(depth[2]); i.i_vbank2_wmask_0 = rm(128); i.i_vbank2_wdata_0 = rnd_b(128);
        i.i_vbank2_en_1 = g_rng() % 3; i.i_vbank2_wen_1 = g_rng() % 3 == 0; i.i_vbank2_addr_1 = (ra(depth[2]) << 2) | static_cast<unsigned>(g_rng() & 3); i.i_vbank2_wmask_1 = rm(32); i.i_vbank2_wdata_1 = rnd_b(32);

        // expected outputs of this cycle, from the registers of the reference
        const GvuScratchpad::Out o = dut.out(i);
        const uint32_t e_s0 = static_cast<uint32_t>(pr[3][0][0]) | (static_cast<uint32_t>(pr[3][0][1]) << 8) | (static_cast<uint32_t>(pr[3][0][2]) << 16) | (static_cast<uint32_t>(pr[3][0][3]) << 24);
        bool ok = o.o_sbank_rdata_0 == e_s0 && o.o_sbank_rdata_1 == pr[3][1][dq[0][1]] && o.o_vbank0_rdata == pr[0][0] && o.o_vbank1_rdata_0 == pr[1][0] && o.o_vbank2_rdata_0 == pr[2][0];
        for (int k = 0; k < 64; k++) ok = ok && o.o_vbank1_rdata_1[static_cast<size_t>(k)] == pr[1][0][static_cast<size_t>(64 * dq[1][1] + k)];
        for (int k = 0; k < 32; k++) ok = ok && o.o_vbank1_rdata_2[static_cast<size_t>(k)] == pr[1][0][static_cast<size_t>(32 * dq[2][1] + k)] && o.o_vbank2_rdata_1[static_cast<size_t>(k)] == pr[2][0][static_cast<size_t>(32 * dq[3][1] + k)];
        CHECK(ok, "scratchpad outputs at cycle %d", cyc);
        n++;

        // one clock of the reference: pipeline registers first, then the memories
        const int bs[4] = {i.i_vbank0_buf_sel ? 1 : 0, i.i_vbank1_buf_sel ? 1 : 0, i.i_vbank2_buf_sel ? 1 : 0, i.i_sbank_buf_sel ? 1 : 0};
        for (int k = 0; k < 4; k++) for (int pt = 0; pt < 2; pt++) pr[k][pt] = q[k][bs[k]][pt];
        for (int k = 0; k < 4; k++) dq[k][1] = dq[k][0];
        dq[0][0] = i.i_sbank_addr_1 & 3; dq[1][0] = i.i_vbank1_addr_1 & 1; dq[2][0] = i.i_vbank1_addr_2 & 3; dq[3][0] = i.i_vbank2_addr_1 & 3;
        struct Acc { bool en, wen; unsigned addr; SpBytes mask, data; };
        auto apply = [&](int k, int pt, const Acc &a, std::vector<std::vector<uint8_t>> &next) {
            if (!a.en) return;
            if (a.wen) ref_write(next[a.addr], a.mask, a.data, P.wmask_as_written);
            else q[k][bs[k]][pt] = mem[k][bs[k]][a.addr];
        };
        {   // vbank0
            std::vector<std::vector<uint8_t>> next = mem[0][bs[0]];
            apply(0, 0, Acc{i.i_vbank0_en, i.i_vbank0_wen, i.i_vbank0_addr, i.i_vbank0_wmask, i.i_vbank0_wdata}, next);
            mem[0][bs[0]] = next;
        }
        {   // vbank1: one of three ports
            Acc a;
            if (i.i_vbank1_port_sel == 0) a = Acc{i.i_vbank1_en_0, i.i_vbank1_wen_0, i.i_vbank1_addr_0, i.i_vbank1_wmask_0, i.i_vbank1_wdata_0};
            else if (i.i_vbank1_port_sel == 1) a = Acc{i.i_vbank1_en_1, i.i_vbank1_wen_1, i.i_vbank1_addr_1 >> 1, shifted(i.i_vbank1_wmask_1, 8 * static_cast<int>(i.i_vbank1_addr_1 & 1), 16), shifted(i.i_vbank1_wdata_1, 64 * static_cast<int>(i.i_vbank1_addr_1 & 1), 128)};
            else a = Acc{i.i_vbank1_en_2, i.i_vbank1_wen_2, i.i_vbank1_addr_2 >> 2, shifted(i.i_vbank1_wmask_2, 4 * static_cast<int>(i.i_vbank1_addr_2 & 3), 16), shifted(i.i_vbank1_wdata_2, 32 * static_cast<int>(i.i_vbank1_addr_2 & 3), 128)};
            std::vector<std::vector<uint8_t>> next = mem[1][bs[1]];
            apply(1, 0, a, next);
            mem[1][bs[1]] = next;
        }
        {   // vbank2: one of two ports
            Acc a;
            if (!i.i_vbank2_port_sel) a = Acc{i.i_vbank2_en_0, i.i_vbank2_wen_0, i.i_vbank2_addr_0, i.i_vbank2_wmask_0, i.i_vbank2_wdata_0};
            else a = Acc{i.i_vbank2_en_1, i.i_vbank2_wen_1, i.i_vbank2_addr_1 >> 2, shifted(i.i_vbank2_wmask_1, 4 * static_cast<int>(i.i_vbank2_addr_1 & 3), 16), shifted(i.i_vbank2_wdata_1, 32 * static_cast<int>(i.i_vbank2_addr_1 & 3), 128)};
            std::vector<std::vector<uint8_t>> next = mem[2][bs[2]];
            apply(2, 0, a, next);
            mem[2][bs[2]] = next;
        }
        {   // sbank: two ports in the same clock, port 1 written last
            std::vector<std::vector<uint8_t>> next = mem[3][bs[3]];
            const SpBytes d0{static_cast<uint8_t>(i.i_sbank_wdata_0), static_cast<uint8_t>(i.i_sbank_wdata_0 >> 8), static_cast<uint8_t>(i.i_sbank_wdata_0 >> 16), static_cast<uint8_t>(i.i_sbank_wdata_0 >> 24)};
            apply(3, 0, Acc{i.i_sbank_en_0, i.i_sbank_wen_0, i.i_sbank_addr_0, SpBytes{static_cast<uint8_t>(i.i_sbank_wmask_0)}, d0}, next);
            const unsigned wa = i.i_sbank_addr_1 & 3;
            SpBytes d1(4, 0); d1[wa] = i.i_sbank_wdata_1;
            apply(3, 1, Acc{i.i_sbank_en_1, i.i_sbank_wen_1, i.i_sbank_addr_1 >> 2, SpBytes{static_cast<uint8_t>((i.i_sbank_wmask_1 ? 1u : 0u) << wa)}, d1}, next);
            mem[3][bs[3]] = next;
        }
        dut.tick(i);
    }
    section("[arith]", "gvu_scratchpad: random accesses on every accelerator port against reference memories", n, f0);
    printf("        capacity per buffer (bank depths of gvu_pkg.sv): vbank0 %u B, vbank1 %u B, vbank2 %u B, sbank %u B = %u B; %u B with both buffers\n",
           P.vbank_bytes(0), P.vbank_bytes(1), P.vbank_bytes(2), P.sbank_bytes(), P.vbank_bytes(0) + P.vbank_bytes(1) + P.vbank_bytes(2) + P.sbank_bytes(),
           2 * (P.vbank_bytes(0) + P.vbank_bytes(1) + P.vbank_bytes(2) + P.sbank_bytes()));
}

// gvu_vlsu: a burst reads the addresses start, start + 1, ... where start is the value of the start-address input in the
// clock before the first request; the counters wrap on their own width; the lane mask is widened per element.
static void test_vlsu()
{
    const int f0 = g_fail; uint64_t n = 0;
    const ScratchpadParams P;
    GvuVlsu dut(P);
    GvuVlsu::In idle; idle.i_vbank0_start_addr = 798; idle.i_vbank2_start_addr = 1022; idle.i_vbank1_start_addr = 31;
    dut.tick(idle);
    GvuVlsu::In rd = idle; rd.i_vbank0_rd = true; rd.i_vbank0_start_addr = 5; rd.i_vbank2_rd = true; rd.i_vbank2_port_sel = true; rd.i_vbank1_wr = true; rd.i_vbank1_port_sel = 0; rd.i_vbank1_lane_wmask = 0x80000005u;
    const unsigned exp0[4] = {798, 799, 800, 801}, exp2[4] = {1022, 1023, 0, 1}, exp1[4] = {31, 0, 1, 2};
    for (int k = 0; k < 4; k++)
    {
        const GvuVlsu::Out o = dut.eval(rd);
        CHECK(o.o_vbank0_en && !o.o_vbank0_wen && o.o_vbank0_addr == exp0[k], "vlsu vbank0 burst beat %d: address %u", k, o.o_vbank0_addr);
        CHECK(o.o_vbank2_en_1 && !o.o_vbank2_en_0 && o.o_vbank2_addr_1 == exp2[k], "vlsu vbank2 port 1 burst beat %d: address %u", k, o.o_vbank2_addr_1);
        CHECK(o.o_vbank1_en_0 && o.o_vbank1_wen_0 && !o.o_vbank1_en_1 && !o.o_vbank1_en_2 && o.o_vbank1_addr_0 == exp1[k], "vlsu vbank1 port 0 burst beat %d: address %u", k, o.o_vbank1_addr_0);
        CHECK(o.o_vbank1_wmask_0[0] == 0x0F && o.o_vbank1_wmask_0[1] == 0x0F && o.o_vbank1_wmask_0[15] == 0xF0 && o.o_vbank1_wmask_1[0] == 0x33 && o.o_vbank1_wmask_1[7] == 0xC0, "vlsu mask expansion");
        dut.tick(rd); n += 4;
    }
    CHECK(dut.eval(idle).o_vbank0_addr == 802, "vlsu: the address register keeps counting until the request ends");
    dut.tick(idle);
    CHECK(dut.eval(idle).o_vbank0_addr == 798, "vlsu: without a request the register follows the start address");
    n += 2;
    {   // address modes: AGU_FIXED follows the start address input during the burst, AGU_POOLING (code 2) keeps the
        // register; in AGU_POOLING the vbank0 address output is the pooling address, so the register is read in AGU_FIXED
        GvuVlsu d2(P);
        GvuVlsu::In q; q.i_vbank0_start_addr = 100; d2.tick(q);
        q.i_vbank0_rd = true; q.i_vbank0_agu_mode = AGU_FIXED; q.i_vbank0_start_addr = 7;
        CHECK(d2.eval(q).o_vbank0_addr == 100, "vlsu AGU_FIXED: first beat at the address loaded before the request");
        d2.tick(q); q.i_vbank0_start_addr = 9;
        CHECK(d2.eval(q).o_vbank0_addr == 7, "vlsu AGU_FIXED: second beat at the start address of the previous clock");
        d2.tick(q);
        CHECK(d2.eval(q).o_vbank0_addr == 9, "vlsu AGU_FIXED: third beat");
        q.i_vbank0_agu_mode = AGU_POOLING; q.i_vbank0_start_addr = 300;
        d2.tick(q); d2.tick(q);
        q.i_vbank0_agu_mode = AGU_FIXED;
        CHECK(d2.eval(q).o_vbank0_addr == 9, "vlsu AGU_POOLING: the register keeps its value");
        // vbank1: one register for the three ports; it advances with the request even when no port is selected
        GvuVlsu::In r; r.i_vbank1_start_addr = 0x7E; d2.tick(r);
        r.i_vbank1_rd = true; r.i_vbank1_port_sel = 3;
        const GvuVlsu::Out o3 = d2.eval(r);
        CHECK(!o3.o_vbank1_en_0 && !o3.o_vbank1_en_1 && !o3.o_vbank1_en_2 && o3.o_vbank1_addr_2 == 0x7E && o3.o_vbank1_addr_1 == 0x3E && o3.o_vbank1_addr_0 == 0x1E, "vlsu vbank1: port select 3 enables no port; the ports take the low bits of one register");
        d2.tick(r); d2.tick(r);
        CHECK(d2.eval(r).o_vbank1_addr_2 == 0x00, "vlsu vbank1: the register advances (and wraps on 7 bits) while no port is enabled");
        n += 6;
    }
    section("[cycle]", "gvu_vlsu: address registers (start latched one clock ahead, wrap, three address modes), port routing, mask expansion", n, f0);
    printf("[note]  gvu_vlsu vbank0: a start address of 798 with 4 beats gives the addresses 798, 799, 800, 801; the bank has 800 rows (addresses 0..799), the counter has 10 bits\n");
}

int main()
{
    printf("tb_rtl_ref_gvu_g2: LUT unit, register file, stream registers, Scratchpad, load / store unit. No RTL simulation is involved in these checks.\n");
    test_lut_value();
    test_lut_stream();
    test_vrf();
    test_scratchpad_directed();
    test_scratchpad_random();
    test_vlsu();
    printf("latency constants (RTL parameter defaults): LUT direct %d, log-scale %d, PWL %d (LERP_MUL_PIPE 0: %d, LERP_SHIFT_PIPE 1: %d); stream register 1; Scratchpad read 2\n",
           lat_lut(LUT_ACT, LUT_PWL), lat_lut(LUT_RECIP, LUT_LOG_SCALE), lat_lut(LUT_RECIP, LUT_PWL), 3, 5);
    printf("checks %" PRIu64 ", mismatches %d\n", g_checks, g_fail);
    printf(g_fail ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return g_fail ? 1 : 0;
}
