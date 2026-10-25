// tb_gvu_top_maxpool_runs.cpp -- model side of tb_oracle_maxpool_runs.sv: max pool of GvuTop (has/rtl_ref_gvu_top.h, port of
// gvu_top.sv) over a whole tensor in runs of `group` channels, written as the same cycle trace, so that the two traces can be
// compared line by line.
//   reset 1 (default): the unit is reset before every run after the first, outside the cycle count (what the model's data flow
//                      controller does, has/gvu_top_stream.h reset_unit; with patch B12 the RTL needs no reset, and its trace
//                      run back to back must equal this one);
//   reset 0          : runs back to back with no reset (the port behaves as the RTL as delivered: runs after the first go wrong).
// Run r: channels r*group .. r*group+n-1 of input.txt ([C][H][W] int8, already padded) poked into vbank0 buffer 0 from row 0
// (row c*H*Wt + h*Wt + w_tile, bytes past W left 0), POOLING C = n; i_pipeline_en high from cycle 0 of the run, low after the edge
// that follows o_maxpool_pipe_done; the run ends two cycles after the done; the next run starts on the next cycle.
// build (from the model root): g++ -std=c++17 -O2 -I. tb_gvu_top_maxpool_runs.cpp -o tb_gvu_top_maxpool_runs
// usage: tb_gvu_top_maxpool_runs <input.txt> <trace out> <h> <w> <k> <s> <c> [group, default c] [reset 1|0, default 1]
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
#include "has/rtl_ref_gvu_top.h"

using namespace rtl_ref_gvu;

static void reset(GvuTop &dut)
{
    GvuTop::In i;
    for (int c = 0; c < 5; c++) dut.tick(i, false);
    dut.tick(i, true);
}

int main(int argc, char **argv)
{
    if (argc < 8)
    {
        std::fprintf(stderr, "usage: %s <input.txt> <trace out> <h> <w> <k> <s> <c> [group] [reset 1|0]\n", argv[0]);
        return 2;
    }
    const int h = std::atoi(argv[3]), w = std::atoi(argv[4]), k = std::atoi(argv[5]), s = std::atoi(argv[6]), c = std::atoi(argv[7]);
    const int group = argc > 8 ? std::atoi(argv[8]) : c;
    const bool reset_each = argc > 9 ? std::atoi(argv[9]) != 0 : true;
    std::vector<int8_t> v;
    {
        std::ifstream f(argv[1]);
        long x;
        while (f >> x) v.push_back(static_cast<int8_t>(x));
    }
    if (v.size() < static_cast<size_t>(c) * h * w) { std::fprintf(stderr, "cannot read %d channels of %s\n", c, argv[1]); return 2; }
    const int wt = (w + 31) / 32, oh = (h - k) / s + 1;
    FILE *fo = std::fopen(argv[2], "wb");   // LF line ends on every host, so trace sha256 values compare
    if (!fo) return 2;
    GvuTop dut;
    reset(dut);
    long total = 0;
    int run = 0;
    for (int c0 = 0; c0 < c; c0 += group, run++)
    {
        const int nc = std::min(group, c - c0);
        if (run > 0 && reset_each) reset(dut);
        for (int ch = 0; ch < nc; ch++)
            for (int y = 0; y < h; y++)
                for (int t = 0; t < wt; t++)
                {
                    SpBytes row(32, 0);
                    for (int l = 0; l < 32 && 32 * t + l < w; l++)
                        row[static_cast<size_t>(l)] = static_cast<uint8_t>(v[(static_cast<size_t>(c0 + ch) * h + y) * w + 32 * t + l]);
                    dut.scratchpad().sram(0, 0).poke(static_cast<unsigned>((ch * h + y) * wt + t), row);
                }
        GvuTop::In i;
        i.i_gvu_op = GVU_OP_MAXPOOL;
        i.i_pooling_mode = k - 2; i.i_pooling_stride = static_cast<unsigned>(s); i.i_pooling_num_w_tile = static_cast<unsigned>(wt);
        i.i_pooling_c = static_cast<unsigned>(nc); i.i_pooling_oh = static_cast<unsigned>(oh);
        i.i_pooling_c_move = static_cast<unsigned>(wt * h); i.i_pooling_h_move = static_cast<unsigned>(wt * s);
        i.i_pipeline_en = true;
        long done_at = -1, n = 0;
        for (; n < 800000; n++)
        {
            const GvuTop::Out o = dut.eval(i);
            if (o.d_vbank0_en || o.d_vbank2_wen_1 || o.o_maxpool_pipe_done)
            {
                std::fprintf(fo, "T %ld %d %d %u %d %u %08x ", total + n, dut.state() == POOLING_MAX ? 1 : 0, o.d_vbank0_en ? 1 : 0,
                             o.d_vbank0_en ? o.d_vbank0_addr : 0u, o.d_vbank2_wen_1 ? 1 : 0, o.d_vbank2_wen_1 ? o.d_vbank2_addr_1 : 0u,
                             o.d_vbank2_wen_1 ? o.d_vbank2_wmask_1 : 0u);
                for (int l = 31; l >= 0; l--)
                    std::fprintf(fo, "%02x", ((o.d_vbank2_wmask_1 >> l) & 1u) ? o.d_vbank2_wdata_1[static_cast<size_t>(l)] : 0u);
                std::fprintf(fo, "\n");
            }
            if (o.o_maxpool_pipe_done && done_at < 0) done_at = n;
            if (done_at >= 0 && n >= done_at + 2) break;
            dut.tick(i, true);
            if (done_at >= 0) i.i_pipeline_en = false;
        }
        if (done_at < 0) { std::fprintf(fo, "TIMEOUT %ld\n", total + n); break; }
        std::fprintf(fo, "DONE %ld\n", total + done_at);
        total += n + 1;
        // the RTL bench ticks one more edge before the next run starts (the enable comes back at the negedge after the last sample)
        dut.tick(i, true);
    }
    std::fclose(fo);
    std::printf("MAXPOOL runs trace %s runs %d cycles %ld reset between runs %d\n", argv[2], run, total, reset_each ? 1 : 0);
    return 0;
}
