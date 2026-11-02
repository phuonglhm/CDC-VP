// tb_gvu_top_softmax_vit.cpp -- replays the SOFTMAX rows of ViT-B/16 (tools/has/gvu_top_vectors/softmax_vit_*.txt, written from the
// functional model) through GvuTop (has/rtl_ref_gvu_top.h) and compares every stage.
//   stage 1: row maximum M (vbank1 buffer 0, one int8 per lane, read when ST1 ends)
//   stage 2: sigma SIG (vbank1 buffer 0, one int32 per lane, read when ST2 ends)
//   stage 3: output A (vbank0 buffer 0, in place of the input)
// Also counts requant inputs T outside int16 and, in mode "level", flips vbank1_buf_sel on every cycle the LUT init done
// output is high, as gvu_config_regs.sv toggles SCRATCHPAD_REG[2] on a level; mode "edge" flips it once, as tb_gvu_top.cpp.
// build: g++ -std=c++17 -O2 -I. tools/has/tb_gvu_top_softmax_vit.cpp -o tb_gvu_top_softmax_vit
// usage: tb_gvu_top_softmax_vit <vector file> [edge|level]   (GVU_SMX_SHIFT overrides ATTN_Shift, GVU_SMX_DUMP lists differences)
// knobs (read here, once): HAS_GVU_SOFTMAX=1 -> shift E + Q - P (GvuTop::In::i_softmax_qp); HAS_GVU_SOFTMAX_AS_RTL=1 overrides it (shift E, as the RTL).
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include "has/rtl_ref_gvu_top.h"

using namespace rtl_ref_gvu;

int main(int argc, char **argv)
{
    if (argc < 2) { std::fprintf(stderr, "usage: %s <vector file> [edge|level]\n", argv[0]); return 2; }
    const bool level = argc > 2 && std::string(argv[2]) == "level";
    std::ifstream f(argv[1]);
    if (!f) { std::fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    std::map<std::string, std::vector<std::vector<long long>>> rec;
    std::string line;
    while (std::getline(f, line))
    {
        std::istringstream in(line);
        std::string tag;
        in >> tag;
        std::vector<long long> v;
        long long x;
        while (in >> x) v.push_back(x);
        rec[tag].push_back(v);
    }
    const auto &cfg = rec["CFG"].at(0);
    const unsigned L = unsigned(cfg[0]);
    const int32_t S = int32_t(cfg[1]);
    const unsigned sh = std::getenv("GVU_SMX_SHIFT") ? unsigned(std::atoi(std::getenv("GVU_SMX_SHIFT"))) : unsigned(cfg[2]);
    const int zp = int(cfg[3]);
    const auto &X = rec["X"], &M = rec["M"], &SIG = rec["SIG"], &A = rec["A"], &T = rec["T"];
    const unsigned rows = unsigned(X.size());
    const auto &ex = rec["EXP"].at(0), &rc = rec["RECIP"].at(0);
    if (rows == 0 || rows > 32 || ex.size() < 256 || rc.size() < 256) { std::fprintf(stderr, "bad vector file\n"); return 2; }

    long t_out16 = 0;
    for (const auto &t : T) for (long long v : t) t_out16 += (v < -32768 || v > 32767);

    GvuTop dut;
    for (unsigned i = 0; i < 4; ++i)
    {
        std::vector<uint8_t> w(128, 0);
        for (unsigned j = 0; j < 64; ++j) { const uint16_t v = uint16_t(rc[i * 64 + j]); w[2 * j] = uint8_t(v); w[2 * j + 1] = uint8_t(v >> 8); }
        dut.scratchpad().sram(1, 1).poke(i, w);
    }
    for (unsigned i = 0; i < 2; ++i)
    {
        std::vector<uint8_t> w(128, 0);
        for (unsigned j = 0; j < 128; ++j) w[j] = uint8_t(ex[i * 128 + j]);
        dut.scratchpad().sram(1, 1).poke(4 + i, w);
    }
    for (unsigned i = 0; i < L; ++i)
    {
        std::vector<uint8_t> w(32, 0);
        for (unsigned j = 0; j < rows; ++j) w[j] = uint8_t(int8_t(X[j][i]));
        dut.scratchpad().sram(0, 0).poke(i, w);
    }

    GvuTop::In in;
    for (int c = 0; c < 5; ++c) dut.tick(in, false);
    dut.tick(in, true);
    in.i_attn_z = int8_t(zp);
    in.i_attn_scale = S;
    in.i_attn_shift = uint8_t(sh);
    in.i_vbank1_buf_sel = true;
    in.i_num_tile = 1;
    in.i_token_len = L;
    in.i_softmax_qp = std::getenv("HAS_GVU_SOFTMAX") && std::atoi(std::getenv("HAS_GVU_SOFTMAX")) != 0 &&
                      !(std::getenv("HAS_GVU_SOFTMAX_AS_RTL") && std::atoi(std::getenv("HAS_GVU_SOFTMAX_AS_RTL")) != 0);
    in.i_gvu_op = GVU_OP_SOFTMAX;
    in.i_pipeline_en = true;

    long m_bad = -1, s_bad = -1, a_bad = 0, flips = 0;
    uint32_t last = 0xFFFFFFFFu;
    uint64_t cyc = 0, done_cyc = 0, st[8] = {0};
    bool done = false;
    for (cyc = 7; cyc < 200000; ++cyc)
    {
        const GvuTop::Out o = dut.tick(in, true);
        const uint32_t s = uint32_t(o.o_gvu_pipe_ctrl_state);
        if (s < 8) st[s]++;
        if (s != last)
        {
            if (last == uint32_t(SOFTMAX_ST1) && !level)
            {
                const auto w = dut.scratchpad().sram(1, 0).word(0);
                m_bad = 0;
                for (unsigned j = 0; j < rows; ++j)
                {
                    m_bad += int8_t(w[j]) != M[j][0];
                    if (std::getenv("GVU_SMX_DUMP")) std::printf("  M row %u got %d want %lld\n", j, int(int8_t(w[j])), M[j][0]);
                }
            }
            if (last == uint32_t(SOFTMAX_ST2) && !level)
            {
                const auto w = dut.scratchpad().sram(1, 0).word(0);
                s_bad = 0;
                for (unsigned j = 0; j < rows; ++j)
                {
                    const uint32_t v = uint32_t(w[4 * j]) | uint32_t(w[4 * j + 1]) << 8 | uint32_t(w[4 * j + 2]) << 16 | uint32_t(w[4 * j + 3]) << 24;
                    s_bad += (long long)v != SIG[j][0];
                }
            }
            last = s;
        }
        if (o.o_softmax_lut_init_done && (level || in.i_vbank1_buf_sel)) { in.i_vbank1_buf_sel = !in.i_vbank1_buf_sel; flips++; }
        if (o.o_softmax_pipe_done && !done) { done = true; done_cyc = cyc; if (std::getenv("S1_STOP")) in.i_pipeline_en = false; }
        if (done && cyc >= done_cyc + 15) break;
    }
    if (!done) { std::printf("TIMEOUT state %u\n", last); return 1; }
    long first_bad = -1;
    for (unsigned i = 0; i < L; ++i)
    {
        const auto w = dut.scratchpad().sram(0, 0).word(i);
        for (unsigned j = 0; j < rows; ++j)
            if (int8_t(w[j]) != A[j][i])
            {
                a_bad++;
                if (first_bad < 0) first_bad = long(j) * L + i;
                if (std::getenv("GVU_SMX_DUMP") && a_bad <= 40)
                    std::printf("  row %u pos %u: E %lld T %lld expected %lld got %d\n", j, i, rec["E"][j][i], T[j][i], A[j][i], int(int8_t(w[j])));
            }
    }
    std::printf("%s qp=%d lshift=%llu mode=%s rows=%u L=%u: M differ %ld, SIG differ %ld, A differ %ld / %u (first %ld), T outside int16 %ld, "
                "buf_sel flips %ld, done cycle %llu, ST1 %llu ST2 %llu ST3 %llu\n",
                argv[1], int(in.i_softmax_qp), (unsigned long long)dut.softmax_lshift(), level ? "level" : "edge", rows, L, m_bad, s_bad, a_bad, rows * L, first_bad, t_out16, flips,
                (unsigned long long)done_cyc, (unsigned long long)st[SOFTMAX_ST1], (unsigned long long)st[SOFTMAX_ST2],
                (unsigned long long)st[SOFTMAX_ST3]);
    std::printf("RESULT: %s\n", a_bad == 0 && m_bad <= 0 && s_bad <= 0 ? "PASS" : "FAIL");
    return a_bad == 0 ? 0 : 1;
}
