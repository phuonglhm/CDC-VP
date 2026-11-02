// tb_rtl_ref_gvu_config_regs_neo.cpp -- self-checks of has/rtl_ref_gvu_config_regs_neo.h and the vector dump for the RTL replay (tools/has/gvu_oracle/tb_oracle_cfgregs_neo.sv).
//   usage: tb_rtl_ref_gvu_config_regs_neo [--dump <file> [cycles]]
// Dump line per clock edge, hex: rstn byte_addr wren rden wmask wdata pipe_done | outputs after the edge.
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <random>
#include "has/rtl_ref_gvu_config_regs_neo.h"
using namespace rtl_ref_gvu;

static int g_fail = 0;
#define CHECK(c, ...) do { if (!(c)) { if (g_fail < 20) std::printf(__VA_ARGS__); g_fail++; } } while (0)

static void directed()
{
    GvuConfigRegsNeo r; GvuConfigNeoIn in;
    // enable rises: the four buffer flops toggle once, stay after
    in = GvuConfigNeoIn(); in.wren = true; in.byte_addr = 0; in.wdata = 1; r.tick(in);
    GvuConfigNeoOut o = r.out();
    CHECK(o.pipe_en && o.sbank_buf_sel && o.vbank0_buf_sel && o.vbank1_buf_sel && o.vbank2_buf_sel, "enable edge must toggle the four buffer flops\n");
    in = GvuConfigNeoIn(); r.tick(in); o = r.out(); CHECK(o.vbank1_buf_sel, "buffer flop must hold without a new edge\n");
    // pipe_done clears the enable and sets done; the done flag is cleared by writing bit 2
    in = GvuConfigNeoIn(); in.gvu_pipe_done = true; r.tick(in); o = r.out(); CHECK(!o.pipe_en && o.done_intr, "pipe_done: enable low, done high\n");
    in = GvuConfigNeoIn(); in.wren = true; in.byte_addr = 0; in.wdata = 4; in.wmask = 4; r.tick(in); o = r.out(); CHECK(!o.done_intr, "clear done\n");
    // the clear request of the same edge as pipe_done wins (the set is lost)
    in = GvuConfigNeoIn(); in.wren = true; in.byte_addr = 0; in.wdata = 4; in.wmask = 4; in.gvu_pipe_done = true; r.tick(in); o = r.out();
    CHECK(!o.done_intr, "pipe_done + clear in one edge: done stays low\n");
    // a host write of bit 0 in the edge of pipe_done keeps the enable
    in = GvuConfigNeoIn(); in.wren = true; in.byte_addr = 0; in.wdata = 1; in.wmask = 1; in.gvu_pipe_done = true; r.tick(in); o = r.out();
    CHECK(o.pipe_en, "write of the enable bit beats the auto-disable of the same edge\n");
    // LUT reset is a one-edge pulse
    in = GvuConfigNeoIn(); in.wren = true; in.byte_addr = 8; in.wdata = 0xA; r.tick(in); o = r.out();
    CHECK(!o.lut_act_reset && o.lut_exp_reset && !o.lut_recip_reset && o.lut_rsqrt_reset, "LUT pulse bits\n");
    in = GvuConfigNeoIn(); r.tick(in); o = r.out(); CHECK(!o.lut_exp_reset && !o.lut_rsqrt_reset, "LUT pulse clears itself\n");
    // index 14 / 15 do not exist
    in = GvuConfigNeoIn(); in.wren = true; in.byte_addr = 0x38; in.wdata = 0x12345678; r.tick(in); CHECK(r.reg(14) == 0, "write to index 14 dropped\n");
    // field decode
    in = GvuConfigNeoIn(); in.wren = true; in.byte_addr = 4 * NEO_SOFTMAX_0; in.wdata = (7u << 16) | 197u; r.tick(in); o = r.out(); CHECK(o.token_len == 197 && o.num_tile == 7, "softmax 16/16 bit fields\n");
    in = GvuConfigNeoIn(); in.wren = true; in.byte_addr = 4 * NEO_POOLING_1; in.wdata = (0x155u << 16) | 0x2AAu; r.tick(in); o = r.out(); CHECK(o.pooling_c_move == 0x155 && o.pooling_h_move == 0x2AA, "pooling move fields\n");
}

int main(int argc, char **argv)
{
    directed();
    std::printf("directed checks: %s\n", g_fail ? "FAIL" : "PASS");
    if (argc > 2 && !std::strcmp(argv[1], "--dump"))
    {
        const long cycles = argc > 3 ? std::atol(argv[3]) : 30000;
        std::FILE *f = std::fopen(argv[2], "w"); if (!f) return 2;
        std::mt19937_64 g(20261101);
        GvuConfigRegsNeo r;
        auto rnd = [&](uint64_t m) { return static_cast<uint32_t>(g() % m); };
        for (long c = 0; c < cycles; c++)
        {
            GvuConfigNeoIn in;
            in.rstn = !(c < 3 || rnd(997) == 0);
            in.byte_addr = rnd(3) ? (4u * rnd(16)) : rnd(64);               // mostly aligned, some odd byte addresses
            const unsigned k = rnd(8);
            in.wren = rnd(10) < 4; in.rden = rnd(10) < 3;
            switch (k) { case 0: in.wmask = 0xFFFFFFFFu; break; case 1: in.wmask = 1u << rnd(32); break; case 2: in.wmask = 0xFu; break; case 3: in.wmask = 0u; break;
                         case 4: in.wmask = rnd(0xFFFFFFFFull); break; default: in.wmask = 0xFFFFFFFFu; }
            in.wdata = static_cast<uint32_t>(g());
            if (in.byte_addr / 4 == 0 && rnd(2)) in.wdata = rnd(8);                // CONTROL: enable / clear bits
            if (in.byte_addr / 4 == 2 && rnd(2)) in.wdata = rnd(16);               // LUT: reset pulses
            in.gvu_pipe_done = rnd(12) == 0;
            r.tick(in);
            const GvuConfigNeoOut o = r.out();
            std::fprintf(f, "%x %x %x %x %x %x %x | %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %llx %x %x %x %x %x %x %x %x %x %x %x\n",
                in.rstn, in.byte_addr, in.wren, in.rden, in.wmask, in.wdata, in.gvu_pipe_done,
                o.pipe_en, o.done_intr, o.op, o.lut_act_reset, o.lut_exp_reset, o.lut_recip_reset, o.lut_rsqrt_reset, o.sbank_buf_sel, o.vbank0_buf_sel, o.vbank1_buf_sel, o.vbank2_buf_sel,
                o.gemm_fused_skip_act, static_cast<uint8_t>(o.gemm_fused_z), o.num_tile, o.token_len, static_cast<uint8_t>(o.attn_z), static_cast<uint32_t>(o.attn_scale), o.attn_shift,
                o.residual_sequence_len, o.residual_sub_mode, static_cast<uint32_t>(o.residual_scale_a), static_cast<uint32_t>(o.residual_scale_b),
                static_cast<unsigned long long>(o.residual_bias), static_cast<uint8_t>(o.residual_out_z), o.residual_out_shift,
                o.pooling_mode, o.pooling_stride, o.pooling_num_w_tile, o.pooling_c, o.pooling_oh, o.pooling_c_move, o.pooling_h_move, o.rdata, 0u);
        }
        std::fclose(f);
        std::printf("dumped %ld cycles\n", cycles);
    }
    std::printf("RESULT: %s\n", g_fail ? "FAIL" : "PASS");
    return g_fail ? 1 : 0;
}
