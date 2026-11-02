// tb_gvu_top_replay.cpp -- replays the GEMM_FUSED epilogue beats of a run of the instruction path through GvuTop
// (has/rtl_ref_gvu_top.h) in stream mode, outside the core. The beats come from a run with HAS_OBP_RTL=1 and
// HAS_OBP_DUMP=<file> (has/gvu_rtl_obp.h): every activation table and every beat in the order the PSM wrote them, with
// the result the cycle-level epilogue gave in that run (equal to the model there, "differ 0").
// Two passes over the same beats, each on one GvuTop that loads every table through vbank1 + LUT_ACT_INIT:
//   back to back   one beat per cycle while the held parameters (zero point, rounding, activation) stay the same: more
//                  beats in flight than the core ever has (it writes at most 64 in a row), every beat with its own scale
//                  and shift
//   one at a time  every beat alone in the pipeline, as the core-path socket runs it (has/gvu_top_bridge.h)
// Checks per pass: every masked lane equals the recorded result; every beat leaves 9 cycles after it entered, in order,
// with its address and mask; nothing leaves that did not enter.
// build: g++ -std=c++17 -O2 -Wall -I. tools/has/tb_gvu_top_replay.cpp -o tb_gvu_top_replay
// usage: tb_gvu_top_replay <dump file>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <vector>
#include "has/rtl_ref_gvu_top.h"

using namespace rtl_ref_gvu;

static const RoundMode kRtl[7] = {RND_FLOOR, RND_NEAR_UP, RND_NEAR_AWAY, RND_NEAR_EVEN, RND_CEIL, RND_TRUNC, RND_AWAY};
static const int kDepth = GVU_GEMM_STREAM_DEPTH;   // 11 = RTL (default), 9 = MAS build

struct Beat { int32_t h[9]; int32_t ps[GVU_LANES]; int8_t res[GVU_LANES]; int table; };

struct Pass
{
    uint64_t beats{0}, lanes{0}, differ{0}, lat_bad{0}, meta_bad{0}, extra{0}, cycles{0}, tables{0}, drains{0};
    long first{-1};
};

class Replayer
{
public:
    Replayer()
    {
        for (int c = 0; c < 5; c++) top_.tick(in_, false);
        top_.tick(in_, true);
    }
    // sends every beat; one_at_a_time: wait for each beat to leave before the next
    void run(const std::vector<std::vector<int8_t>> &tables, const std::vector<Beat> &beats, bool one_at_a_time, Pass &p)
    {
        int cur_table = -1;
        size_t b = 0;
        while (b < beats.size() || !q_.empty())
        {
            bool send = false;
            if (b < beats.size())
            {
                const Beat &x = beats[b];
                const bool same_hold = top_.state() == GEMM_FUSED_ST1 && x.table == cur_table && in_.i_gemm_zp16 == x.h[3] &&
                                       in_.i_gemm_round == kRtl[x.h[4]] && in_.i_gemm_skip_act == (x.h[5] == 0);
                if (!same_hold || (one_at_a_time && !q_.empty()))
                {
                    if (q_.empty())
                    {
                        // pipeline empty: change the held parameters, load the table if it changed
                        if (x.table != cur_table) { load_table(tables[size_t(x.table)], p); cur_table = x.table; }
                        in_.i_gemm_zp16 = x.h[3]; in_.i_gemm_round = kRtl[x.h[4]]; in_.i_gemm_skip_act = x.h[5] == 0;
                        enter(p);
                        if (!same_hold) p.drains++;
                        continue;
                    }
                }
                else send = true;
            }
            in_.i_psum_valid = send;
            if (send)
            {
                const Beat &x = beats[b];
                for (int l = 0; l < GVU_LANES; l++) in_.i_psum_vec[size_t(l)] = x.ps[l];
                in_.i_psum_scale = x.h[1]; in_.i_psum_shift = static_cast<uint8_t>(x.h[2]); in_.i_psum_mask = static_cast<uint32_t>(x.h[0]);
                in_.i_psum_addr = static_cast<uint32_t>(b);
            }
            const GvuTop::Out o = top_.tick(in_, true);
            if (o.o_gemm_fused_valid)
            {
                if (q_.empty()) p.extra++;
                else
                {
                    const Pend e = q_.front();
                    q_.pop_front();
                    const Beat &x = beats[e.b];
                    if (cyc_ != e.t + kDepth) p.lat_bad++;
                    if (o.o_gemm_fused_addr != uint32_t(e.b) || o.o_gemm_fused_mask != static_cast<uint32_t>(x.h[0])) p.meta_bad++;
                    for (int l = 0; l < GVU_LANES; l++)
                    {
                        if (!((static_cast<uint32_t>(x.h[0]) >> l) & 1u)) continue;
                        p.lanes++;
                        if (o.o_gemm_fused_data[size_t(l)] != x.res[l]) { if (p.first < 0) p.first = long(e.b); p.differ++; }
                    }
                }
            }
            if (send) { q_.push_back({b, cyc_}); b++; p.beats++; }
            cyc_++;
            p.cycles++;
        }
        stop();
    }

private:
    struct Pend { size_t b; uint64_t t; };
    GvuTop top_;
    GvuTop::In in_;
    std::deque<Pend> q_;
    uint64_t cyc_{0};
    bool loaded_{false};

    void stop()
    {
        if (top_.state() == IDLE) return;
        in_.i_psum_valid = false; in_.i_gemm_stream_stop = true;
        for (int k = 0; k < 50 && top_.state() != IDLE; k++) { top_.tick(in_, true); cyc_++; }
        in_.i_pipeline_en = false; in_.i_gemm_stream_stop = false;
        top_.tick(in_, true); cyc_++;
    }
    void load_table(const std::vector<int8_t> &t, Pass &p)
    {
        stop();
        for (unsigned w = 0; w < 2; w++) top_.scratchpad().sram(1, 1).poke(w, SpBytes(t.begin() + 128 * w, t.begin() + 128 * (w + 1)));
        in_.i_vbank1_buf_sel = true;
        in_.i_gemm_lut_reload = loaded_;
        loaded_ = true;
        p.tables++;
    }
    void enter(Pass &)
    {
        if (top_.state() == GEMM_FUSED_ST1 && !in_.i_gemm_lut_reload) return;
        stop();
        in_.i_gvu_op = GVU_OP_GEMM_FUSED; in_.i_gemm_stream = true; in_.i_psum_valid = false; in_.i_pipeline_en = true;
        for (int k = 0; k < 100 && top_.state() != GEMM_FUSED_ST1; k++)
        {
            top_.tick(in_, true); cyc_++;
            if (top_.state() == LUT_ACT_INIT) in_.i_gemm_lut_reload = false;
        }
        in_.i_gemm_lut_reload = false;
    }
};

int main(int argc, char **argv)
{
    if (argc < 2) { std::fprintf(stderr, "usage: %s <dump file of HAS_OBP_DUMP>\n", argv[0]); return 2; }
    std::FILE *f = std::fopen(argv[1], "rb");
    if (!f) { std::fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
    std::vector<std::vector<int8_t>> tables;
    std::vector<Beat> beats;
    int c;
    bool broken = false;
    while ((c = std::fgetc(f)) != EOF)
    {
        if (c == 'T')
        {
            std::vector<int8_t> t(256);
            if (std::fread(t.data(), 1, 256, f) != 256) { broken = true; break; }
            tables.push_back(t);
        }
        else if (c == 'B')
        {
            Beat x;
            if (std::fread(x.h, 4, 9, f) != 9 || std::fread(x.ps, 4, GVU_LANES, f) != size_t(GVU_LANES) ||
                std::fread(x.res, 1, GVU_LANES, f) != size_t(GVU_LANES) || x.h[4] < 0 || x.h[4] > 6) { broken = true; break; }
            x.table = static_cast<int>(tables.size()) - 1;
            if (x.table < 0) { tables.push_back(std::vector<int8_t>(256, 0)); x.table = 0; }   // beats before any table: no activation
            beats.push_back(x);
        }
        else { broken = true; break; }
    }
    std::fclose(f);
    if (broken) { std::printf("RESULT: FAIL (the dump file is truncated or not a HAS_OBP_DUMP file)\n"); return 1; }
    std::printf("dump: %zu tables, %zu beats\n", tables.size(), beats.size());
    bool ok = !beats.empty();
    for (int pass = 0; pass < 2; pass++)
    {
        Replayer r;
        Pass p;
        r.run(tables, beats, pass == 1, p);
        const bool good = p.differ == 0 && p.lat_bad == 0 && p.meta_bad == 0 && p.extra == 0 && p.beats == beats.size();
        std::printf("%-13s: beats %llu, lanes %llu, differ %llu, latency other than %d: %llu, address / mask wrong %llu, beats out of nothing %llu, "
                    "tables loaded %llu, parameter changes %llu, cycles %llu%s\n", pass ? "one at a time" : "back to back",
                    (unsigned long long)p.beats, (unsigned long long)p.lanes, (unsigned long long)p.differ, kDepth, (unsigned long long)p.lat_bad,
                    (unsigned long long)p.meta_bad, (unsigned long long)p.extra, (unsigned long long)p.tables, (unsigned long long)p.drains,
                    (unsigned long long)p.cycles, good ? "" : " <-- FAIL");
        if (p.first >= 0) std::printf("  first beat that differs: %ld\n", p.first);
        ok = ok && good;
    }
    std::printf("RESULT: %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
