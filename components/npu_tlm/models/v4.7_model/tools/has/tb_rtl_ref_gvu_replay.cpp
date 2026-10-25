// tb_rtl_ref_gvu_replay.cpp -- replay of the ELEM_WISE instructions of a compiled program on the cycle-level pipelines
// of has/rtl_ref_gvu_elemwise.h (max pool, residual add), one instruction at a time and without the rest of the model.
// The program directory is the one the instruction-path testbench reads: mmio.txt (register writes of the compiler)
// and dram_golden.bin (the memory image with every tensor at its reference value). For every ELEM_WISE instruction the
// operands are taken from the reference image, the pipeline runs with the ported load / store unit and Scratchpad, and
// the result is compared byte by byte with the reference image at the output address.
// Not replayed: average pool (no cycle-level module yet) and an add whose operand lies at the output address (the
// reference image holds the result there, not the operand).
// --gvu-top: the residual adds and the max pools run on GvuTop through the socket of the instruction path
// (has::GvuTopBridge::add and ::maxpool of has/gvu_top_bridge.h, as with HAS_GVU_TOP=1); a max pool goes in the channel
// groups of has::HasElemwise::maxpool with the default Scratchpad (Knobs::spec_default), as on the instruction path.
// build: g++ -std=c++17 -O2 -Wall -I. tools/has/tb_rtl_ref_gvu_replay.cpp -o tb_rtl_ref_gvu_replay
// usage: tb_rtl_ref_gvu_replay <program dir> [max instructions, 0 = all] [--gvu-top]
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "has/gvu_quant.h"
#include "has/has_mmio_compat.h"
#include "has/rtl_ref_gvu_elemwise.h"
#include "has/gvu_top_bridge.h"

using namespace rtl_ref_gvu;

struct Totals
{
    uint64_t instr{0}, elems{0}, bad{0}, cycles{0}, runs{0}, sum_overflow{0};
};
static bool g_gvu_top = false;

// Residual add on GvuTop through the socket; returns the number of bytes that differ from want.
static uint64_t replay_add_top(const int8_t *A, const int8_t *B, const int8_t *want, size_t n, const has::AddQ &q, Totals *t, uint64_t *cyc)
{
    std::vector<int8_t> o(want, want + n);     // the socket compares with this and returns GvuTop's result in it
    uint64_t pc = 0;
    if (!has::GvuTopBridge::add(A, B, o.data(), n, q.zpA, q.zpB, q.zpO, q.SA, q.SB, q.SO, q.sA, q.sB, q.sO, has::HALF_UP, true, true, 0, &pc))
    {
        std::printf("  add: not expressible on GvuTop (or a run did not finish)\n");
        return n;
    }
    uint64_t bad = 0;
    for (size_t e = 0; e < n; e++)
        if (o[e] != want[e])
        {
            if (bad < 5) std::printf("  add element %zu: got %d, reference %d (a %d, b %d)\n", e, o[e], want[e], A[e], B[e]);
            bad++;
        }
    *cyc += pc; t->runs += ((n + 31) / 32 + 127) / 128;
    return bad;
}

static bool read_file(const std::string &path, std::vector<uint8_t> &d)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    d.resize(static_cast<size_t>(f.tellg()));
    f.seekg(0);
    f.read(reinterpret_cast<char *>(d.data()), static_cast<std::streamsize>(d.size()));
    return true;
}

// Residual add of n elements; returns the number of bytes that differ from want.
static uint64_t replay_add(const int8_t *A, const int8_t *B, const int8_t *want, size_t n, const has::AddQ &q, Totals *t, uint64_t *cyc)
{
    GvuResAddPipe::In cfg;
    cfg.i_zp_a = q.zpA; cfg.i_zp_b = q.zpB; cfg.i_zp_out = q.zpO;
    cfg.i_scale_a = static_cast<int32_t>(q.SA); cfg.i_scale_b = static_cast<int32_t>(q.SB); cfg.i_scale_out = static_cast<int32_t>(q.SO);
    cfg.i_shift_a = static_cast<unsigned>(q.sA); cfg.i_shift_b = static_cast<unsigned>(q.sB); cfg.i_shift_out = static_cast<unsigned>(q.sO);
    cfg.i_round = RND_NEAR_UP;
    const has::Knobs kn;                                                 // default arithmetic of the functional model
    const size_t rows_total = (n + 31) / 32, per_chunk = 128;            // vbank#1 holds 128 int8 rows
    uint64_t bad = 0;
    for (size_t r0 = 0; r0 < rows_total; r0 += per_chunk)
    {
        const size_t rows = std::min(per_chunk, rows_total - r0);
        ElemwiseBench b;
        GvuResAddPipe dut;
        std::vector<SpBytes> v1((rows + 3) / 4, SpBytes(128, 0));
        for (size_t r = 0; r < rows; r++)
        {
            SpBytes ra(32, 0);
            for (size_t l = 0; l < 32; l++)
            {
                const size_t e = (r0 + r) * 32 + l;
                if (e >= n) break;
                ra[l] = static_cast<uint8_t>(A[e]);
                v1[r / 4][32 * (r % 4) + l] = static_cast<uint8_t>(B[e]);
            }
            b.poke_v0(static_cast<unsigned>(r), ra);
        }
        for (size_t k = 0; k < v1.size(); k++) b.poke_v1(static_cast<unsigned>(k), v1[k]);
        cfg.i_rows = static_cast<unsigned>(rows); cfg.i_a_row = 0; cfg.i_b_row = 0; cfg.i_sum_row = 0;
        const size_t tail = n - (r0 + rows - 1) * 32;
        cfg.i_last_lane_mask = (r0 + rows == rows_total && tail < 32) ? ((1u << tail) - 1u) : 0xFFFFFFFFu;
        b.step(dut, cfg);
        cfg.i_start = true; b.step(dut, cfg); cfg.i_start = false;
        const uint64_t c0 = b.cycles;
        bool done = false;
        for (uint64_t guard = 0; guard < 2 * rows + 200 && !done; guard++) done = b.step(dut, cfg).o_done;
        if (!done) { std::printf("  add: no done in a chunk of %zu rows\n", rows); return n; }
        *cyc += b.cycles - c0; t->runs++;
        for (size_t r = 0; r < rows; r++)
        {
            const SpBytes orow = b.peek_v0(static_cast<unsigned>(r));
            for (size_t l = 0; l < 32; l++)
            {
                const size_t e = (r0 + r) * 32 + l;
                if (e >= n) break;
                if (static_cast<int8_t>(orow[l]) != want[e])
                {
                    if (bad < 5) std::printf("  add element %zu: got %d, reference %d (a %d, b %d)\n", e, static_cast<int8_t>(orow[l]), want[e], A[e], B[e]);
                    bad++;
                }
                // sum outside 32 bits: the stage 1 adder wraps where the functional model saturates
                const int64_t s = static_cast<int64_t>(has::dequant(A[e], q.zpA, static_cast<int32_t>(q.SA), q.sA, kn)) + has::dequant(B[e], q.zpB, static_cast<int32_t>(q.SB), q.sB, kn);
                if (s > INT32_MAX || s < INT32_MIN) t->sum_overflow++;
            }
        }
    }
    return bad;
}

// Max pool on GvuTop through the socket, channel groups as has::HasElemwise::maxpool with the default Scratchpad (banked:
// the padded input in vbank#0, the result in vbank#1); returns the number of bytes that differ from want ([C, Ho, Wo]).
static uint64_t replay_maxpool_top(const int8_t *in, const int8_t *want, int C, int H, int W, int kk, int s, int p, Totals *t, uint64_t *cyc, uint64_t *elems)
{
    const int Hp = H + 2 * p, Wp = W + 2 * p, Ho = (Hp - kk) / s + 1, Wo = (Wp - kk) / s + 1;
    has::Knobs k;
    k.spec_default();
    int group = k.sp_banked ? std::min(static_cast<int>(k.sp_v0 / static_cast<uint32_t>(Hp * Wp)), static_cast<int>(k.sp_v1 / static_cast<uint32_t>(Ho * Wo)))
                            : static_cast<int>(k.scratch_bytes / static_cast<uint32_t>(Hp * Wp));
    if (group < 1) group = 1;
    std::vector<int8_t> o(want, want + static_cast<size_t>(C) * Ho * Wo);   // the socket compares with this and returns GvuTop's result in it
    for (int c0 = 0; c0 < C; c0 += group)
    {
        const int g = std::min(group, C - c0);
        uint64_t pc = 0;
        if (!has::GvuTopBridge::maxpool(in + static_cast<size_t>(c0) * H * W, o.data() + static_cast<size_t>(c0) * Ho * Wo, g, H, W, kk, s, p, 0, &pc))
        {
            std::printf("  max pool: not expressible on GvuTop (or a run did not finish), channels %d..%d\n", c0, c0 + g - 1);
            return static_cast<uint64_t>(C) * Ho * Wo;
        }
        *cyc += pc; t->runs++;
    }
    uint64_t bad = 0;
    for (size_t e = 0; e < o.size(); e++)
    {
        if (o[e] != want[e])
        {
            if (bad < 5) std::printf("  max pool element %zu: got %d, reference %d\n", e, o[e], want[e]);
            bad++;
        }
        (*elems)++;
    }
    return bad;
}

// Max pool on [C, H, W]; returns the number of bytes that differ from want ([C, Ho, Wo]).
static uint64_t replay_maxpool(const int8_t *in, const int8_t *want, int C, int H, int W, int kk, int s, int p, Totals *t, uint64_t *cyc, uint64_t *elems)
{
    const int Hp = H + 2 * p, Wp = W + 2 * p, Ho = (Hp - kk) / s + 1, Wo = (Wp - kk) / s + 1, vpr = (Wo + 31) / 32, win = kk * kk;
    auto at = [&](int c, int y, int x) -> int8_t {                       // padded tensor, padding value -128
        y -= p; x -= p;
        return (y < 0 || y >= H || x < 0 || x >= W) ? static_cast<int8_t>(-128) : in[(static_cast<size_t>(c) * H + static_cast<size_t>(y)) * W + static_cast<size_t>(x)];
    };
    struct OutVec { int c, y, x0; };
    std::vector<OutVec> ov;
    for (int c = 0; c < C; c++) for (int y = 0; y < Ho; y++) for (int v = 0; v < vpr; v++) ov.push_back({c, y, 32 * v});
    const size_t per_chunk = std::min<size_t>(800 / static_cast<size_t>(win), 128);     // rows of vbank#0, int8 rows of vbank#1
    if (per_chunk == 0) { std::printf("  max pool: a window of %d vectors does not fit vbank#0\n", win); return static_cast<uint64_t>(C) * Ho * Wo; }
    uint64_t bad = 0;
    for (size_t base = 0; base < ov.size(); base += per_chunk)
    {
        const size_t n = std::min(per_chunk, ov.size() - base);
        ElemwiseBench b;
        GvuMaxPoolPipe dut;
        for (size_t r = 0; r < n; r++)
            for (int ky = 0; ky < kk; ky++)
                for (int kx = 0; kx < kk; kx++)
                {
                    SpBytes row(32, static_cast<uint8_t>(0x80));
                    for (int l = 0; l < 32 && ov[base + r].x0 + l < Wo; l++)
                        row[static_cast<size_t>(l)] = static_cast<uint8_t>(at(ov[base + r].c, ov[base + r].y * s + ky, (ov[base + r].x0 + l) * s + kx));
                    b.poke_v0(static_cast<unsigned>(r * static_cast<size_t>(win) + static_cast<size_t>(ky * kk + kx)), row);
                }
        GvuMaxPoolPipe::In cfg;
        cfg.i_out_rows = static_cast<unsigned>(n); cfg.i_win = static_cast<unsigned>(win); cfg.i_src_row = 0; cfg.i_dst_row = 0;
        b.step(dut, cfg);
        cfg.i_start = true; b.step(dut, cfg); cfg.i_start = false;
        const uint64_t c0 = b.cycles;
        bool done = false;
        for (uint64_t guard = 0; guard < n * static_cast<uint64_t>(win) + 100 && !done; guard++) done = b.step(dut, cfg).o_done;
        if (!done) { std::printf("  max pool: no done in a chunk of %zu rows\n", n); return static_cast<uint64_t>(C) * Ho * Wo; }
        *cyc += b.cycles - c0; t->runs++;
        for (size_t r = 0; r < n; r++)
        {
            const SpBytes row = b.peek_v1(static_cast<unsigned>(r / 4));
            const OutVec &o = ov[base + r];
            for (int l = 0; l < 32 && o.x0 + l < Wo; l++)
            {
                const int8_t got = static_cast<int8_t>(row[32 * (r % 4) + static_cast<size_t>(l)]);
                const int8_t ref = want[(static_cast<size_t>(o.c) * Ho + static_cast<size_t>(o.y)) * Wo + static_cast<size_t>(o.x0 + l)];
                if (got != ref)
                {
                    if (bad < 5) std::printf("  max pool c %d y %d x %d: got %d, reference %d\n", o.c, o.y, o.x0 + l, got, ref);
                    bad++;
                }
                (*elems)++;
            }
        }
    }
    return bad;
}

int main(int argc, char **argv)
{
    if (argc < 2) { std::printf("usage: %s <program dir> [max instructions, 0 = all] [--gvu-top]\n", argv[0]); return 2; }
    const std::string dir = argv[1];
    const long max_instr = argc > 2 && std::strcmp(argv[2], "--gvu-top") != 0 ? std::atol(argv[2]) : 0;
    for (int a = 2; a < argc; a++) if (std::strcmp(argv[a], "--gvu-top") == 0) g_gvu_top = true;
    // the socket reads HAS_GVU_TOP once, at its first use (after this line)
    if (g_gvu_top) { setenv("HAS_GVU_TOP", "1", 1); std::printf("[replay] residual adds and max pools on GvuTop (has::GvuTopBridge)\n"); }
    std::vector<uint8_t> gold;
    std::ifstream f(dir + "/mmio.txt");
    if (!f || !read_file(dir + "/dram_golden.bin", gold)) { std::printf("cannot read %s/mmio.txt or dram_golden.bin\n", dir.c_str()); return 2; }
    const int8_t *g = reinterpret_cast<const int8_t *>(gold.data());

    has::MmioCompat mm;
    Totals add, mp;
    uint64_t skipped_avg = 0, skipped_inplace = 0, skipped_range = 0, index = 0;
    std::string line, label;
    long done_instr = 0;
    while (std::getline(f, line) && (max_instr == 0 || done_instr < max_instr))
    {
        if (line.rfind("# [", 0) == 0) { label = line.substr(2); continue; }
        if (line.empty() || line[0] == '#' || line[0] == 'H') continue;
        std::istringstream is(line);
        std::string tag;
        uint32_t addr = 0, val = 0;
        is >> tag >> std::hex >> addr >> val;
        mm.write(addr, val);
        has::LayerInstr li;
        while (mm.pop(li))
        {
            const uint64_t idx = index;
            if (li.last_of_host) index++;
            mm.retire(li);
            if (li.opcode != 0x15) continue;
            if (li.mode == 5) { skipped_avg++; continue; }
            uint64_t cyc = 0, bad = 0, elems = 0;
            if (li.mode == 0)
            {
                if (li.a_addr == li.out_addr || li.b_addr == li.out_addr) { skipped_inplace++; continue; }
                if (static_cast<uint64_t>(std::max({li.a_addr, li.b_addr, li.out_addr})) + li.n > gold.size()) { skipped_range++; continue; }
                bad = (g_gvu_top ? replay_add_top : replay_add)(g + li.a_addr, g + li.b_addr, g + li.out_addr, li.n, li.add, &add, &cyc);
                elems = li.n;
                add.instr++; add.elems += elems; add.bad += bad; add.cycles += cyc;
            }
            else
            {
                const int Ho = (li.h + 2 * li.pool_p - li.pool_k) / li.pool_s + 1, Wo = (li.w + 2 * li.pool_p - li.pool_k) / li.pool_s + 1;
                if (static_cast<uint64_t>(li.a_addr) + static_cast<uint64_t>(li.c) * li.h * li.w > gold.size() ||
                    static_cast<uint64_t>(li.out_addr) + static_cast<uint64_t>(li.c) * Ho * Wo > gold.size()) { skipped_range++; continue; }
                bad = (g_gvu_top ? replay_maxpool_top : replay_maxpool)(g + li.a_addr, g + li.out_addr, li.c, li.h, li.w, li.pool_k, li.pool_s, li.pool_p, &mp, &cyc, &elems);
                mp.instr++; mp.elems += elems; mp.bad += bad; mp.cycles += cyc;
            }
            std::printf("[replay] %" PRIu64 " %s elements %" PRIu64 " bad %" PRIu64 " cycles %" PRIu64 " %s  # %s\n", idx, li.mode == 0 ? "ADD" : "MAX_POOL", elems, bad, cyc,
                        bad == 0 ? "PASS" : "FAIL", label.c_str());
            std::fflush(stdout);
            done_instr++;
        }
    }
    std::printf("[replay] residual add: %" PRIu64 " instructions, %" PRIu64 " elements, %" PRIu64 " differ from the reference, %" PRIu64 " pipeline cycles in %" PRIu64 " chunks, sums outside 32 bits %" PRIu64 "\n",
                add.instr, add.elems, add.bad, add.cycles, add.runs, add.sum_overflow);
    std::printf("[replay] max pool: %" PRIu64 " instructions, %" PRIu64 " elements, %" PRIu64 " differ from the reference, %" PRIu64 " pipeline cycles in %" PRIu64 " chunks\n",
                mp.instr, mp.elems, mp.bad, mp.cycles, mp.runs);
    std::printf("[replay] not replayed: average pool %" PRIu64 ", add with an operand at the output address %" PRIu64 ", address outside the image %" PRIu64 "\n",
                skipped_avg, skipped_inplace, skipped_range);
    const bool ok = add.bad == 0 && mp.bad == 0 && add.instr + mp.instr > 0;
    std::printf("RESULT: %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
