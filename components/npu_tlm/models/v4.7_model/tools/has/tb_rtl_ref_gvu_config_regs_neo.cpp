// tb_rtl_ref_gvu_config_regs_neo.cpp -- self-checks of has/rtl_ref_gvu_config_regs_neo.h and the vector dump for the RTL replay (tools/has/gvu_oracle/tb_oracle_cfgregs_neo.sv).
//   usage: tb_rtl_ref_gvu_config_regs_neo [--dump <file> [cycles [--rev <0|1>]]]
//   --rev 0 (default): vectors for neo_lite RTL revision 0; --rev 1: the port with rev 1 (neo_lite RTL revision 1) and CONTROL writes that also set bit 3 (force buffer swap)
// Dump line per clock edge, hex: rstn byte_addr wren rden wmask wdata pipe_done | outputs after the edge.
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <random>
#include "has/rtl_ref_gvu_config_regs_neo.h"
#include "has/gvu_regs_neo_shadow.h"
#include "has/gvu_regs_dfc.h"
#include "has/neo_rev.h"
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

// neo_lite RTL revision 1 (rev 1): Reg 12 C / OH swapped, CONTROL bit 3 swaps the four buffer flops unless the enable rises in the same edge (gvu_config_regs.sv:221-232).
static void directed_rev()
{
    for (unsigned rev = 0; rev < 2; rev++)
    {
        GvuConfigRegsNeo r(GvuConfigRegsNeo::kNumRegs, rev); GvuConfigNeoIn in;
        auto ctl = [&](uint32_t d, uint32_t m) { in = GvuConfigNeoIn(); in.wren = true; in.byte_addr = 0; in.wdata = d; in.wmask = m; r.tick(in); };
        auto flops = [&]() { GvuConfigNeoOut o = r.out(); return (o.sbank_buf_sel ? 1u : 0u) | (o.vbank0_buf_sel ? 2u : 0u) | (o.vbank1_buf_sel ? 4u : 0u) | (o.vbank2_buf_sel ? 8u : 0u); };
        const unsigned s1 = rev ? 15u : 0u;
        ctl(8u, 0xFFFFFFFFu);                         // bit 3 alone, enable low
        CHECK(flops() == s1 && r.forced_swaps() == rev, "rev %u: bit 3 with the enable low: flops %x\n", rev, flops());
        CHECK((r.reg(0) & 8u) == 0, "rev %u: bit 3 is not stored\n", rev);
        ctl(8u, 0xFFFFFFF7u);                         // mask bit 3 low: nothing
        CHECK(flops() == s1, "rev %u: bit 3 without its mask bit\n", rev);
        ctl(9u, 0xFFFFFFFFu);                         // start + bit 3 in one write: the enable edge wins, one flip only
        CHECK(flops() == (15u ^ s1) && r.out().pipe_en && r.forced_swaps() == rev, "rev %u: start + bit 3 in one write: one flip\n", rev);
        ctl(9u, 0xFFFFFFFFu);                         // enable already high: no edge, bit 3 flips (rev 1)
        CHECK(flops() == 15u && r.forced_swaps() == 2u * rev, "rev %u: bit 3 with the enable already high\n", rev);
        in = GvuConfigNeoIn(); in.rstn = false; in.wren = true; in.byte_addr = 0; in.wdata = 8u; r.tick(in);
        CHECK(flops() == 0u && r.forced_swaps() == 2u * rev, "rev %u: reset clears the flops, no swap\n", rev);
        // Reg 12 packed as the revision packs it: mode 2, stride 2, W tiles 4, C 33, OH 15
        in = GvuConfigNeoIn(); in.wren = true; in.byte_addr = 4 * NEO_POOLING_0;
        in.wdata = 2u | 2u << 2 | 4u << 8 | (rev ? (15u << 16 | 33u << 24) : (33u << 16 | 15u << 24)); r.tick(in);
        const GvuConfigNeoOut o = r.out();
        CHECK(o.pooling_c == 33 && o.pooling_oh == 15 && o.pooling_num_w_tile == 4 && o.pooling_stride == 2 && o.pooling_mode == 2, "rev %u: Reg 12 fields\n", rev);
    }
}

// The 32-register proposal (HAS_GVU_REGS_NEO) and the command sequences of gvu_regs_dfc::neo, run through GvuRegFile. NOT the oracle: the oracle replays the 14-register RTL verbatim.
static void directed32()
{
    namespace N = has::gvu_regs_dfc::neo;
    GvuConfigRegsNeo a;                                   // default = the RTL: 14 registers
    GvuConfigRegsNeo b(GvuConfigRegsNeo::kMaxRegs);
    GvuConfigNeoIn in; in.wren = true; in.wdata = 0xCAFEF00Du;
    for (unsigned i = 14; i < 32; i++) { in.byte_addr = 4 * i; a.tick(in); b.tick(in); }
    bool all32 = true, none14 = true;
    for (unsigned i = 14; i < 32; i++) { all32 = all32 && b.reg(i) == 0xCAFEF00Du; none14 = none14 && a.reg(i) == 0; }
    CHECK(all32, "32-register file keeps 14..31\n"); CHECK(none14, "14-register file drops 14..31\n");
    CHECK(a.num_regs() == 14 && b.num_regs() == 32, "num_regs\n");

    has::GvuRegFile f; GvuConfigParams cp; cp.b13_fixed = true; cp.word_addressed = true; cp.num_regs = 32; f.configure(cp, true);
    auto run = [&](const std::vector<has::GvuRegMaster::W> &s) { for (const auto &w : s) f.write_axi(w.byte_addr, w.data, w.strb); };
    // GEMM_FUSED with a table reset: opcode, zero point, one LUT pulse that has ended, enable, the four flops flipped once
    const std::vector<has::GvuRegMaster::W> lb = N::layer_begin(-5, true, true);
    run(lb);
    GvuConfigOut o = f.out();
    CHECK(o.pipe_en && o.op == 2 && o.gemm_fused_skip_act && o.gemm_fused_z == -5, "layer_begin fields\n");
    CHECK(!o.lut_act_reset && f.buf(0) && f.buf(1) && f.buf(2) && f.buf(3), "pulse ended, flops flipped once\n");
    run(N::clear_done()); CHECK(!f.out().pipe_en && !(f.reg(0) & 2u), "clear: enable and done low\n");
    run(lb); CHECK(!f.buf(2), "second command flips the flops back\n");
    std::vector<has::GvuRegMaster::W> p = lb; const unsigned pad = N::pad_to_14(p);
    CHECK(p.size() == 15 && pad == 11, "start sequence padded to 14 + CONTROL (%zu, pad %u)\n", p.size(), pad);
    std::vector<has::GvuRegMaster::W> c = N::clear_done(); CHECK(N::pad_to_14(c) == 0 && c.size() == 1, "clear is not padded\n");
    // the last CONTROL write of a command (HAS_NEO_REV >= 1): start = 0 with bit 3, after the clear in the same chain; the flops flip once more, the enable stays low
    const std::vector<has::GvuRegMaster::W> cl = has::gvu_regs_dfc::clear_done_last(true);
    std::vector<has::GvuRegMaster::W> cl2 = cl;
    CHECK(cl.size() == (has::neo_rev() ? 2u : 1u) && has::gvu_regs_dfc::clear_done_last(false).size() == 1u && N::pad_to_14(cl2) == 0, "clear + last CONTROL write: %zu writes\n", cl.size());
    const bool b2 = f.buf(2);
    run(cl);
    CHECK(f.buf(2) == (has::neo_rev() ? !b2 : b2) && f.buf(0) == f.buf(2) && !f.out().pipe_en && !(f.reg(0) & 2u), "last CONTROL write for HAS_NEO_REV %u\n", has::neo_rev());
    // average pool: window in 12 / 13, scale / shift / round in 14 / 15, opcode 8
    run(N::avg_run(2, 2, 4, 33, 15, 160, 8, -1234567, 17, 3));
    o = f.out();
    CHECK(o.op == 8 && o.pooling_mode == 2 && o.pooling_stride == 2 && o.pooling_num_w_tile == 4 && o.pooling_c == 33 && o.pooling_oh == 15 && o.pooling_c_move == 160 && o.pooling_h_move == 8,
          "avg pooling fields\n");
    CHECK(((f.reg(12) >> 16) & 0xFFu) == (has::neo_rev() ? 15u : 33u) && (f.reg(12) >> 24) == (has::neo_rev() ? 33u : 15u), "Reg 12 packed for HAS_NEO_REV %u\n", has::neo_rev());
    CHECK(static_cast<int32_t>(f.reg(14)) == -1234567 && (f.reg(15) & 0x3Fu) == 17 && ((f.reg(15) >> 8) & 7u) == 3, "avg scale / shift / round at 14 / 15\n");
    run(N::clear_done());
    // Softmax with a token length that the previous 9-bit field could not hold
    run(N::softmax_run(-77, 9, -3, 800));
    o = f.out();
    CHECK(o.op == 3 && o.token_len == 800 && o.num_tile == 1 && o.attn_scale == -77 && o.attn_shift == 9 && o.attn_z == -3, "softmax fields\n");
    run(N::clear_done());
    // residual add: rows / output zero point / output shift in RESIDUAL_0 (6 bits), scales in RESIDUAL_1 / 2, the rest above the 14 registers
    has::gvu_regs_dfc::AddRegs ar; ar.scale_a = 12345; ar.scale_b = -6789; ar.scale_out = 4242; ar.shift_a = 3; ar.shift_b = 5; ar.shift_out = 70; ar.round = 4; ar.out_int16 = true;
    ar.zp_a = -2; ar.zp_b = 7; ar.zp_out = -100; ar.a_row = 5; ar.b_row = 6; ar.sum_row = 7;
    run(N::add_run(ar, 128, 0xFFFF0000u, static_cast<int64_t>(-5) * 4096));
    o = f.out();
    CHECK(o.op == 6 && o.token_len == 128 && o.num_tile == 1 && o.res_scale_a == 12345 && o.res_scale_b == -6789 && o.res_scale_out == 4242, "residual scales / rows\n");
    CHECK(o.res_shift_a == 3 && o.res_shift_b == 5 && o.res_shift_out == (70u & 0x3Fu) && o.res_round == 4 && o.res_out_int16, "residual shifts cut to 6 bits for the output\n");
    CHECK(o.res_zp_a == -2 && o.res_zp_b == 7 && o.res_zp_out == -100 && o.res_a_row == 5 && o.res_b_row == 6 && o.res_sum_row == 7 && o.res_last_lane_mask == 0xFFFF0000u, "residual zero points / rows / mask\n");
    CHECK((static_cast<int64_t>((static_cast<uint64_t>(f.reg(11)) << 32) | f.reg(10))) == static_cast<int64_t>(-5) * 4096, "residual bias at 10 / 11\n");
    // the previous map is untouched when the knob is off
    has::GvuRegFile g; g.configure(cp, false); g.write_axi(0, 1u, 0xF);
    CHECK(g.reg(0) == 1u && !g.neo(), "previous map through GvuRegFile\n");
}

int main(int argc, char **argv)
{
    directed();
    directed_rev();
    directed32();
    std::printf("directed checks: %s\n", g_fail ? "FAIL" : "PASS");
    if (argc > 2 && !std::strcmp(argv[1], "--dump"))
    {
        const long cycles = argc > 3 ? std::atol(argv[3]) : 30000;
        const unsigned rev = (argc > 5 && !std::strcmp(argv[4], "--rev")) ? static_cast<unsigned>(std::atoi(argv[5])) : 0u;
        std::FILE *f = std::fopen(argv[2], "w"); if (!f) return 2;
        std::mt19937_64 g(20261101);
        GvuConfigRegsNeo r(GvuConfigRegsNeo::kNumRegs, rev);
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
            if (in.byte_addr / 4 == 0 && rnd(2)) in.wdata = rnd(rev ? 16 : 8);     // CONTROL: enable / clear bits (rev 1: and the force buffer swap bit)
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
        std::printf("dumped %ld cycles, rev %u, %llu enable edges, %llu forced swaps\n", cycles, rev, (unsigned long long)r.enable_edges(), (unsigned long long)r.forced_swaps());
    }
    std::printf("RESULT: %s\n", g_fail ? "FAIL" : "PASS");
    return g_fail ? 1 : 0;
}
