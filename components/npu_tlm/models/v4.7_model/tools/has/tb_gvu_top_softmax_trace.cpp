// tb_gvu_top_softmax_trace.cpp -- SOFTMAX of GvuTop (has/rtl_ref_gvu_top.h) on the stimulus of tools/has/gvu_oracle/tb_top_softmax.sv (the RTL report bench), printed
// in the same style: the cycle of every change of the controller state and the cycle of the done output, so that the trace of the model of one RTL revision can be
// compared with the trace of that RTL (tools/has/gvu_oracle/run_verilator_top.sh, lines "TOP"). Cycle 0 = the cycle before the first edge with i_pipeline_en = 1;
// "cycle N" = the state after N such edges (the RTL bench samples the state 1 ns after the negedge that follows the same edge).
// Stimulus of the RTL bench: i_attn_scale 35, i_attn_shift 8, i_attn_z 0, i_num_tile 1, token length L; reciprocal table in vbank1 buffer 0 rows 0..3 (16-bit word
// 0x4000 + 64 * row + j), exponential table rows 4..5 (byte 255 - (128 * (row - 4) + j)); vbank0 buffer 0 row t, lane l = int8 (7 * t + 3 * l); no requant input.
// usage: tb_gvu_top_softmax_trace <token length L> <neo_rev 0 | 1 | 2>      prints "TOP ..." lines and "RESULT: DONE" (exit 0) or "RESULT: TIMEOUT"
// build: g++ -std=c++17 -O2 -Wall -I. tools/has/tb_gvu_top_softmax_trace.cpp -o tb_gvu_top_softmax_trace
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "has/rtl_ref_gvu_top.h"

using namespace rtl_ref_gvu;

static const char *name(control_fsm_t s)
{
    switch (s)
    {
    case IDLE: return "IDLE";
    case LUT_ACT_INIT: return "LUT_ACT_INIT";
    case LUT_EXP_INIT: return "LUT_EXP_INIT";
    case LUT_RECIP_INIT: return "LUT_RECIP_INIT";
    case LUT_RSQRT_INIT: return "LUT_RSQRT_INIT";
    case SOFTMAX_ST1: return "SOFTMAX_ST1";
    case SOFTMAX_ST2: return "SOFTMAX_ST2";
    case SOFTMAX_ST3: return "SOFTMAX_ST3";
    default: return "OTHER";
    }
}

int main(int argc, char **argv)
{
    if (argc < 3) { std::fprintf(stderr, "usage: %s <token length> <neo_rev>\n", argv[0]); return 2; }
    const unsigned L = static_cast<unsigned>(std::atoi(argv[1]));
    GvuTopParams p;
    p.neo_rev = std::atoi(argv[2]);
    GvuTop dut(p);
    for (unsigned k = 0; k < 4; k++)
    {
        std::vector<uint8_t> w(128, 0);
        for (unsigned j = 0; j < 64; j++) { const uint16_t v = static_cast<uint16_t>(0x4000 + 64 * k + j); w[2 * j] = static_cast<uint8_t>(v); w[2 * j + 1] = static_cast<uint8_t>(v >> 8); }
        dut.scratchpad().sram(1, 0).poke(k, w);
    }
    for (unsigned k = 0; k < 2; k++)
    {
        std::vector<uint8_t> w(128, 0);
        for (unsigned j = 0; j < 128; j++) w[j] = static_cast<uint8_t>(255 - (128 * k + j));
        dut.scratchpad().sram(1, 0).poke(4 + k, w);
    }
    for (unsigned t = 0; t < 300; t++)
    {
        std::vector<uint8_t> w(32, 0);
        for (unsigned l = 0; l < 32; l++) w[l] = static_cast<uint8_t>(7 * t + 3 * l);
        dut.scratchpad().sram(0, 0).poke(t, w);
    }
    GvuTop::In in;
    for (int c = 0; c < 4; ++c) dut.tick(in, false);   // reset
    in.i_attn_z = 0; in.i_attn_scale = 35; in.i_attn_shift = 8; in.i_num_tile = 1; in.i_token_len = L;
    in.i_vbank1_buf_sel = false;
    in.i_gvu_op = GVU_OP_SOFTMAX;
    in.i_pipeline_en = true;
    std::printf("TOP token length %u; model neo_rev %d\n", L, p.neo_rev);
    control_fsm_t last = dut.state();
    bool seen_done = false;
    unsigned cyc = 0;
    for (cyc = 1; cyc <= 4000; cyc++)
    {
        const control_fsm_t pre = last;                    // the state of cycle cyc - 1: the outputs of tick() belong to it
        const GvuTop::Out o = dut.tick(in, true);
        const control_fsm_t s = dut.state();
        if (o.o_softmax_pipe_done && !seen_done) { seen_done = true; std::printf("TOP cycle %u: done output = 1 (state %s)\n", cyc - 1, name(pre)); }
        if (s != last) { std::printf("TOP cycle %u: %s -> %s\n", cyc, name(last), name(s)); last = s; }
        if (seen_done && s == IDLE) break;
    }
    std::printf("TOP end at cycle %u: state %s, done seen %d\n", cyc, name(last), seen_done ? 1 : 0);
    std::printf("RESULT: %s\n", seen_done ? "DONE" : "TIMEOUT");
    return seen_done ? 0 : 1;
}
