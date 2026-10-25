// tb_gvu_top_maxpool.cpp -- max pool of GvuTop (has/rtl_ref_gvu_top.h, POOLING_MAX, port of the latest hardware RTL drop) against
//   (a) the golden outputs of the hardware team's vectors (target/test/maxpool/{single,multi}/stride_*/k*/ of the RTL tree:
//       single = 288 channels of 20 x 20, multi = 40 channels of 80 x 80; plain max pool, no padding), and
//   (b) generated cases the vectors do not have, among them the SPPF max pool of YOLOv8m (288 channels of 20 x 20 padded
//       by 2 with -128 = 24 x 24, 5 x 5, stride 1, one W tile), against a plain max pool computed here.
// GvuTop is driven as the hardware test bench tb/gvu/tb_gvu_top.sv does: the tensor is written into vbank0 buffer 0 as rows
// of 32 bytes (row c * H * Wt + h * Wt + w_tile, bytes past W left 0), the result is read from vbank2 port 1 (v32int8)
// buffer 0 (row c * OH * Wt + oh * Wt + w_tile, lane = ow mod the output lanes of a tile). The channels go in runs that fit
// vbank0 (800 rows) and vbank2 port 1 (1 024 rows): the hardware bench processes one run only. Every run starts from a
// reset of the unit: the pooling feeder keeps first_load_cnt between runs (it is cleared only by the reset), and a run
// started right after another reads extra rows and gives wrong results (the last line of the table shows it
// on two back-to-back runs, as a diagnostic, outside the verdict).
// Measured against the latest hardware RTL drop (tools/has/gvu_oracle/run_verilator_maxpool.sh): the port writes the same
// vbank2 rows in the same cycles, and one run of the RTL equals the golden on every set, several W tiles included: the
// 5-bit lane index that wraps (B3 of the review 1051) does not give wrong values. The table prints, per set, the output
// positions (ow mod lanes per tile) that differ; the verdict expects none.
// build: g++ -std=c++17 -O2 -Wall -I. tools/has/tb_gvu_top_maxpool.cpp -o tb_gvu_top_maxpool
// usage: tb_gvu_top_maxpool <vector directory target/test/maxpool, or - to skip (a)> [seed, default 1]
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>
#include <set>
#include <string>
#include <vector>
#include "has/gvu_top_bridge.h"
#include "has/rtl_ref_gvu_top.h"

using namespace rtl_ref_gvu;

struct Case
{
    std::string name;
    int C, H, W, k, s;
    std::vector<int8_t> in;      // [C][H][W]
    std::vector<int8_t> gold;    // [C][OH][OW]
    std::set<int> expect_pos;    // output positions (ow mod lanes per tile) predicted to differ (B3 and the k4 s2 crossing)
};

static int out_lanes(int k, int s) { return s == 1 ? 32 : (k == s ? (32 - k) / s + 1 : (32 - k) / s + 2); }

static std::vector<int8_t> plain_maxpool(const std::vector<int8_t> &in, int C, int H, int W, int k, int s)
{
    const int OH = (H - k) / s + 1, OW = (W - k) / s + 1;
    std::vector<int8_t> o(static_cast<size_t>(C) * OH * OW);
    for (int c = 0; c < C; c++)
        for (int y = 0; y < OH; y++)
            for (int x = 0; x < OW; x++)
            {
                int m = -128;
                for (int dy = 0; dy < k; dy++)
                    for (int dx = 0; dx < k; dx++) m = std::max(m, static_cast<int>(in[(static_cast<size_t>(c) * H + y * s + dy) * W + x * s + dx]));
                o[(static_cast<size_t>(c) * OH + y) * OW + x] = static_cast<int8_t>(m);
            }
    return o;
}

static bool read_ints(const std::string &path, std::vector<int8_t> &v)
{
    std::ifstream f(path);
    if (!f) return false;
    long x;
    while (f >> x) v.push_back(static_cast<int8_t>(x));
    return true;
}

static void reset(GvuTop &dut)
{
    GvuTop::In i;
    for (int c = 0; c < 5; c++) dut.tick(i, false);
    dut.tick(i, true);
}

struct Result { long long differ{0}, lanes{0}, runs{0}, timeouts{0}; uint64_t cycles{0}; std::set<int> pos; };

// reset_each: reset the unit before every run (the verdict); false = runs back to back (diagnostic of the feeder state)
static Result run_case(GvuTop &dut, const Case &cs, bool reset_each = true)
{
    Result r;
    const int Wt = (cs.W + 31) / 32, OH = (cs.H - cs.k) / cs.s + 1, OW = (cs.W - cs.k) / cs.s + 1, OL = out_lanes(cs.k, cs.s);
    const int cmax = std::min({800 / (cs.H * Wt), 1024 / (OH * Wt), 255});
    for (int c0 = 0; c0 < cs.C; c0 += cmax)
    {
        if (reset_each) reset(dut);
        const int n = std::min(cmax, cs.C - c0);
        for (int c = 0; c < n; c++)
            for (int h = 0; h < cs.H; h++)
                for (int t = 0; t < Wt; t++)
                {
                    SpBytes row(32, 0);
                    for (int l = 0; l < 32 && 32 * t + l < cs.W; l++)
                        row[static_cast<size_t>(l)] = static_cast<uint8_t>(cs.in[(static_cast<size_t>(c0 + c) * cs.H + h) * cs.W + 32 * t + l]);
                    dut.scratchpad().sram(0, 0).poke(static_cast<unsigned>((c * cs.H + h) * Wt + t), row);
                }
        GvuTop::In i;
        i.i_gvu_op = GVU_OP_MAXPOOL;
        i.i_pooling_mode = cs.k - 2;
        i.i_pooling_stride = static_cast<unsigned>(cs.s);
        i.i_pooling_num_w_tile = static_cast<unsigned>(Wt);
        i.i_pooling_c = static_cast<unsigned>(n);
        i.i_pooling_oh = static_cast<unsigned>(OH);
        i.i_pooling_c_move = static_cast<unsigned>(Wt * cs.H);
        i.i_pooling_h_move = static_cast<unsigned>(Wt * cs.s);
        i.i_pipeline_en = true;
        bool done = false;
        uint64_t c = 0;
        const uint64_t guard = static_cast<uint64_t>(n) * OH * Wt * 64 + 1000;
        for (; c < guard && !done; c++) done = dut.tick(i, true).o_maxpool_pipe_done;
        i.i_pipeline_en = false;
        for (int q = 0; q < 4 && dut.state() != IDLE; q++) dut.tick(i, true);
        r.runs++;
        if (!done || dut.state() != IDLE) { r.timeouts++; reset(dut); continue; }
        r.cycles += c;
        for (int cc = 0; cc < n; cc++)
            for (int oh = 0; oh < OH; oh++)
                for (int ow = 0; ow < OW; ow++)
                {
                    const unsigned row = static_cast<unsigned>((cc * OH + oh) * Wt + ow / OL);
                    const SpBytes w = dut.scratchpad().sram(2, 0).word(row >> 2);
                    const int8_t got = static_cast<int8_t>(w[(row & 3u) * 32u + static_cast<unsigned>(ow % OL)]);
                    const int8_t exp = cs.gold[(static_cast<size_t>(c0 + cc) * OH + oh) * OW + ow];
                    r.lanes++;
                    if (got != exp) { r.differ++; r.pos.insert(ow % OL); }
                }
    }
    return r;
}

static std::string fmt(const std::set<int> &s)
{
    if (s.empty()) return "-";
    std::string o;
    for (int v : s) o += (o.empty() ? "" : ",") + std::to_string(v);
    return o;
}

// --trace: one run of the first c channels of an input.txt, written as the cycle trace of tb_oracle_maxpool.sv (same
// line format, same start: cycle 0 is the first cycle with i_pipeline_en, the enable drops after the edge that follows
// the done output, the trace ends two cycles after it; with runs > 1 the enable comes back after those two cycles and the
// same run starts again without a reset, its trace following).
static int trace(const char *fin, const char *fout, int h, int w, int k, int s, int c, int runs)
{
    std::vector<int8_t> v;
    if (!read_ints(fin, v) || v.size() < static_cast<size_t>(c) * h * w) { std::fprintf(stderr, "cannot read %d channels of %s\n", c, fin); return 2; }
    const int wt = (w + 31) / 32, oh = (h - k) / s + 1;
    GvuTop dut;
    reset(dut);
    for (int ch = 0; ch < c; ch++)
        for (int y = 0; y < h; y++)
            for (int t = 0; t < wt; t++)
            {
                SpBytes row(32, 0);
                for (int l = 0; l < 32 && 32 * t + l < w; l++) row[static_cast<size_t>(l)] = static_cast<uint8_t>(v[(static_cast<size_t>(ch) * h + y) * w + 32 * t + l]);
                dut.scratchpad().sram(0, 0).poke(static_cast<unsigned>((ch * h + y) * wt + t), row);
            }
    FILE *fo = std::fopen(fout, "w");
    if (!fo) return 2;
    GvuTop::In i;
    i.i_gvu_op = GVU_OP_MAXPOOL;
    i.i_pooling_mode = k - 2; i.i_pooling_stride = static_cast<unsigned>(s); i.i_pooling_num_w_tile = static_cast<unsigned>(wt);
    i.i_pooling_c = static_cast<unsigned>(c); i.i_pooling_oh = static_cast<unsigned>(oh);
    i.i_pooling_c_move = static_cast<unsigned>(wt * h); i.i_pooling_h_move = static_cast<unsigned>(wt * s);
    i.i_pipeline_en = true;
    long done_at = -1, n = 0;
    int runs_done = 0;
    for (; n < 800000; n++)
    {
        const GvuTop::Out o = dut.eval(i);
        if (o.d_vbank0_en || o.d_vbank2_wen_1 || o.o_maxpool_pipe_done)
        {
            std::fprintf(fo, "T %ld %d %d %u %d %u %08x ", n, dut.state() == POOLING_MAX ? 1 : 0, o.d_vbank0_en ? 1 : 0, o.d_vbank0_en ? o.d_vbank0_addr : 0u,
                         o.d_vbank2_wen_1 ? 1 : 0, o.d_vbank2_wen_1 ? o.d_vbank2_addr_1 : 0u, o.d_vbank2_wen_1 ? o.d_vbank2_wmask_1 : 0u);
            for (int l = 31; l >= 0; l--)
                std::fprintf(fo, "%02x", ((o.d_vbank2_wmask_1 >> l) & 1u) ? o.d_vbank2_wdata_1[static_cast<size_t>(l)] : 0u);
            std::fprintf(fo, "\n");
        }
        if (o.o_maxpool_pipe_done && done_at < 0) done_at = n;
        bool restart = false;
        if (done_at >= 0 && n >= done_at + 2)
        {
            if (++runs_done == runs) break;
            std::fprintf(fo, "DONE %ld\n", done_at);
            done_at = -1;
            restart = true;
        }
        dut.tick(i, true);
        if (restart) i.i_pipeline_en = true;              // next run on the same data, no reset
        else if (done_at >= 0) i.i_pipeline_en = false;
    }
    if (done_at < 0) std::fprintf(fo, "TIMEOUT %ld\n", n);
    else std::fprintf(fo, "DONE %ld\n", done_at);
    std::fclose(fo);
    std::printf("MAXPOOL trace %s cycles %ld done %ld\n", fout, n, done_at);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc >= 9 && std::string(argv[1]) == "--trace")   // optional 10th argument: runs back to back (default 1)
        return trace(argv[2], argv[3], std::atoi(argv[4]), std::atoi(argv[5]), std::atoi(argv[6]), std::atoi(argv[7]), std::atoi(argv[8]), argc > 9 ? std::atoi(argv[9]) : 1);
    if (argc < 2) { std::fprintf(stderr, "usage: %s <target/test/maxpool directory or -> [seed]\n       %s --trace <input.txt> <trace out> <h> <w> <k> <s> <c> [runs]\n", argv[0], argv[0]); return 2; }
    setenv("HAS_GVU_TOP", "1", 1);   // for part (c): the socket reads it once, at its first use
    const std::string root = argv[1];
    std::mt19937_64 rng(argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 1);
    std::vector<Case> cases;
    if (root != "-")
    {
        const struct { const char *tile, *dir; int k, s; } sets[] = {
            {"single", "stride_1/k2", 2, 1}, {"single", "stride_1/k3", 3, 1}, {"multi", "stride_1/k2", 2, 1}, {"multi", "stride_1/k3", 3, 1},
            {"multi", "stride_1/k4", 4, 1}, {"multi", "stride_1/k5", 5, 1}, {"multi", "stride_2/k2", 2, 2}, {"multi", "stride_2/k4", 4, 2},
            {"multi", "stride_4/k4", 4, 4}};
        for (const auto &st : sets)
        {
            Case cs;
            cs.name = std::string("hw ") + st.tile + "/" + st.dir;
            cs.H = cs.W = std::string(st.tile) == "single" ? 20 : 80;
            cs.k = st.k; cs.s = st.s;
            const std::string d = root + "/" + st.tile + "/" + st.dir + "/";
            if (!read_ints(d + "input.txt", cs.in) || !read_ints(d + "golden_output.txt", cs.gold)) { std::printf("MISSING %s\n", d.c_str()); return 1; }
            cs.C = static_cast<int>(cs.in.size() / (static_cast<size_t>(cs.H) * cs.W));
            cases.push_back(cs);
        }
    }
    // generated: every supported (k, s) on one W tile, random data; then the YOLOv8m SPPF case (padded by 2 with -128)
    const struct { int k, s, H, W, C; } gen[] = {{2, 1, 17, 31, 13}, {2, 2, 18, 32, 9}, {3, 1, 19, 29, 11}, {4, 1, 16, 32, 7},
                                                 {4, 2, 22, 30, 6}, {4, 4, 20, 32, 5}, {5, 1, 21, 27, 10}};
    for (const auto &g : gen)
    {
        Case cs;
        cs.name = "gen k" + std::to_string(g.k) + " s" + std::to_string(g.s) + " " + std::to_string(g.C) + "x" + std::to_string(g.H) + "x" + std::to_string(g.W);
        cs.C = g.C; cs.H = g.H; cs.W = g.W; cs.k = g.k; cs.s = g.s;
        cs.in.resize(static_cast<size_t>(g.C) * g.H * g.W);
        for (auto &v : cs.in) v = static_cast<int8_t>(std::uniform_int_distribution<int>(-128, 127)(rng));
        cs.gold = plain_maxpool(cs.in, cs.C, cs.H, cs.W, cs.k, cs.s);
        cases.push_back(cs);
    }
    {
        Case cs;
        cs.name = "YOLOv8m SPPF 288x20x20 p2 (24x24) k5 s1";
        cs.C = 288; cs.H = cs.W = 24; cs.k = 5; cs.s = 1;
        cs.in.assign(static_cast<size_t>(cs.C) * 24 * 24, static_cast<int8_t>(-128));
        for (int c = 0; c < cs.C; c++)
            for (int y = 0; y < 20; y++)
                for (int x = 0; x < 20; x++)
                    cs.in[(static_cast<size_t>(c) * 24 + y + 2) * 24 + x + 2] = static_cast<int8_t>(std::uniform_int_distribution<int>(-128, 127)(rng));
        cs.gold = plain_maxpool(cs.in, cs.C, cs.H, cs.W, cs.k, cs.s);
        cases.push_back(cs);
    }

    GvuTop dut;
    reset(dut);
    int bad = 0;
    std::printf("[tb_gvu_top_maxpool] set | channels | runs | GvuTop cycles | outputs | differ | positions that differ (ow mod lanes per tile) | expected positions | verdict\n");
    for (const auto &cs : cases)
    {
        const Result r = run_case(dut, cs);
        const bool ok = r.timeouts == 0 && r.pos == cs.expect_pos && (cs.expect_pos.empty() ? r.differ == 0 : r.differ > 0);
        if (!ok) bad++;
        std::printf("[tb_gvu_top_maxpool] %-42s | %4d | %3lld | %10llu | %8lld | %7lld | %-18s | %-18s | %s%s\n", cs.name.c_str(), cs.C, r.runs,
                    static_cast<unsigned long long>(r.cycles), r.lanes, r.differ, fmt(r.pos).c_str(), fmt(cs.expect_pos).c_str(),
                    ok ? "as expected" : "UNEXPECTED", r.timeouts ? " (a run did not finish)" : "");
    }
    // diagnostic, outside the verdict: the last case again with its runs back to back, no reset in between
    {
        reset(dut);
        const Result r = run_case(dut, cases.back(), false);
        std::printf("[tb_gvu_top_maxpool] DIAG back-to-back runs (no reset), %s: runs %lld, cycles %llu, outputs %lld, differ %lld%s\n",
                    cases.back().name.c_str(), r.runs, static_cast<unsigned long long>(r.cycles), r.lanes, r.differ, r.timeouts ? ", a run did not finish" : "");
    }
    // (c) the socket of the instruction path, has::GvuTopBridge::maxpool (HAS_GVU_TOP=1, set at the start of main): padded
    //     random tensors, groups that need several runs, several W tiles, a residual add on the same unit between two max
    //     pools, and shapes GvuTop does not pool (the socket must refuse them and leave the result untouched)
    int bad_c = 0, n_c = 0;
    {
        const struct { int k, s, p, C, H, W; bool pools; } sk[] = {
            {5, 1, 2, 40, 20, 20, true},     // SPPF-like, 40 channels of 24 x 24 padded: 2 runs (33 + 7)
            {3, 1, 1, 17, 13, 30, true},     // Wp = 32
            {2, 2, 0, 9, 16, 32, true},
            {4, 2, 1, 5, 21, 33, true},      // Wp = 35: 2 W tiles, stride 2
            {3, 1, 1, 4, 10, 60, true},      // Wp = 62: 2 W tiles, stride 1
            {5, 1, 2, 288, 20, 20, true},    // the SPPF group of the default Scratchpad is 14 channels; here all 288 at once: 9 runs
            {3, 2, 1, 6, 12, 12, false},     // 3x3 stride 2: TODO in the RTL
            {5, 2, 2, 6, 12, 12, false}};    // 5x5 stride 2: TODO in the RTL
        std::vector<int8_t> A(200), B(200), O(200);
        for (size_t e = 0; e < A.size(); e++) { A[e] = static_cast<int8_t>(e * 7 - 100); B[e] = static_cast<int8_t>(50 - e * 3); }
        for (const auto &t : sk)
        {
            const int Hp = t.H + 2 * t.p, Wp = t.W + 2 * t.p, Ho = (Hp - t.k) / t.s + 1, Wo = (Wp - t.k) / t.s + 1;
            std::vector<int8_t> in(static_cast<size_t>(t.C) * t.H * t.W), padded(static_cast<size_t>(t.C) * Hp * Wp, static_cast<int8_t>(-128));
            for (auto &v : in) v = static_cast<int8_t>(std::uniform_int_distribution<int>(-128, 127)(rng));
            for (int c = 0; c < t.C; c++)
                for (int y = 0; y < t.H; y++)
                    for (int x = 0; x < t.W; x++) padded[(static_cast<size_t>(c) * Hp + y + t.p) * Wp + x + t.p] = in[(static_cast<size_t>(c) * t.H + y) * t.W + x];
            const std::vector<int8_t> ref = plain_maxpool(padded, t.C, Hp, Wp, t.k, t.s);
            std::vector<int8_t> out = ref;     // the socket compares with this and returns GvuTop's result in it
            std::fill(O.begin(), O.end(), 0);
            uint64_t cyc = 0, acyc = 0;
            has::GvuTopBridge::add(A.data(), B.data(), O.data(), O.size(), 0, 0, 0, 1 << 30, 1 << 30, 1 << 30, 30, 30, 30, 1, true, true, 0, &acyc);
            const bool ran = has::GvuTopBridge::maxpool(in.data(), out.data(), t.C, t.H, t.W, t.k, t.s, t.p, 0, &cyc);
            long long differ = 0;
            for (size_t e = 0; e < out.size(); e++) if (out[e] != ref[e]) differ++;
            const bool ok = t.pools ? (ran && differ == 0) : (!ran && differ == 0);
            n_c++;
            if (!ok) bad_c++;
            std::printf("[tb_gvu_top_maxpool] socket k%d s%d p%d %dx%dx%d -> %dx%d | %s | cycles %10llu | differ %lld | %s\n", t.k, t.s, t.p, t.C, t.H, t.W, Ho, Wo,
                        ran ? "ran on GvuTop" : "refused", static_cast<unsigned long long>(cyc), differ, ok ? "as expected" : "UNEXPECTED");
        }
    }
    std::printf("RESULT: %s (sets not as expected: %d of %zu; socket cases not as expected: %d of %d)\n", bad || bad_c ? "FAIL" : "PASS", bad, cases.size(), bad_c, n_c);
    return bad || bad_c ? 1 : 0;
}
