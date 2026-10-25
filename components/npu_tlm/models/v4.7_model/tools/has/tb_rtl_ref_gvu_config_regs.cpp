// tb_rtl_ref_gvu_config_regs.cpp -- test bench of has/rtl_ref_gvu_config_regs.h (port of neo_core/gvu/gvu_config_regs.sv, latest RTL drop).
//   <binary>                    directed self-check of the behaviours listed in the header; prints RESULT: PASS / FAIL
//   <binary> --dump <out dir>   writes the cycle-by-cycle stimulus and the model outputs for replay on the RTL by
//                               tools/has/gvu_oracle/tb_oracle_cfgregs.sv (run_verilator_cfgregs.sh / run_modelsim_cfgregs.sh):
//                                 cfgregs_dlv.txt  model b13_fixed = false  (replay on the RTL as delivered)
//                                 cfgregs_b13.txt  model b13_fixed = true   (replay on the RTL with the B13 patch)
//                                 cfgregs_dlvvl.txt model b13_fixed = false + flat_array_oob (the delivered RTL as Verilator 5 simulates it)
//                               one line per clock edge, hex fields:
//                                 rstn addr wren rden wmask wdata lut_init_done pipe_done |
//                                 rd_seen rdata pipe_en op luts bufs skip z num_tile token_len attn_z attn_scale attn_shift
//                                 pmode pstride pnwt pc poh pcm phm
//                               (luts = {rsqrt, recip, exp, act}, bufs = {v2, v1, v0, s}; rd_seen = a read has happened since the
//                               start, rdata is only compared then)
// build: g++ -std=c++17 -O2 -Wall -I. tools/has/tb_rtl_ref_gvu_config_regs.cpp -o tb_rtl_ref_gvu_config_regs
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include "has/rtl_ref_gvu_config_regs.h"

using namespace rtl_ref_gvu;

static int g_fail = 0;
#define CHECK(c, ...) do { if (!(c)) { g_fail++; std::printf("FAIL line %d: ", __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static GvuConfigIn mk(unsigned addr, bool wren, uint32_t wdata, uint32_t wmask = 0xFFFFFFFFu)
{
    GvuConfigIn in; in.addr = addr; in.wren = wren; in.wdata = wdata; in.wmask = wmask; return in;
}

static void directed()
{
    // ---- as delivered: only byte 0 is writable (B13)
    { GvuConfigRegs r;
      r.tick(mk(CFG_GEMM_FUSED, true, 0xAABBCCDDu));
      CHECK(r.reg(CFG_GEMM_FUSED) == 0xDDu, "as delivered: Reg 3 reads 0x%08x, expected 0xdd", r.reg(CFG_GEMM_FUSED));
      CHECK(r.out().gemm_fused_z == 0, "as delivered: z %d", r.out().gemm_fused_z);
      r.tick(mk(CFG_POOLING_0, true, 0x28181405u));
      CHECK(r.out().pooling_mode == 1 && r.out().pooling_stride == 1 && r.out().pooling_num_w_tile == 0 && r.out().pooling_c == 0, "as delivered pooling");
      r.tick(mk(CFG_SOFTMAX_1, true, 0x12345678u));
      CHECK(r.out().attn_scale == 0x78, "as delivered attn_scale 0x%x", r.out().attn_scale);
      // a mask that does not include bit 0 writes nothing
      r.tick(mk(CFG_SOFTMAX_1, true, 0xFFFFFFFFu, 0xFFFFFF00u));
      CHECK(r.out().attn_scale == 0x78, "as delivered: mask without bit 0 changed the register");
    }
    // ---- patch B13: all four bytes, bit mask
    { GvuConfigParams p; p.b13_fixed = true; GvuConfigRegs r(p);
      r.tick(mk(CFG_GEMM_FUSED, true, 0xAABBCCDDu));
      CHECK(r.reg(CFG_GEMM_FUSED) == 0xAABBCCDDu, "B13: Reg 3 0x%08x", r.reg(CFG_GEMM_FUSED));
      CHECK(static_cast<uint8_t>(r.out().gemm_fused_z) == 0xCC, "B13: z 0x%02x", static_cast<uint8_t>(r.out().gemm_fused_z));
      r.tick(mk(CFG_POOLING_0, true, 0xFFFFFFFFu));
      r.tick(mk(CFG_POOLING_0, true, 0x11223344u, 0x0000FF00u));
      CHECK(r.reg(CFG_POOLING_0) == 0xFFFF33FFu, "B13: partial write 0x%08x", r.reg(CFG_POOLING_0));
      r.tick(mk(CFG_SOFTMAX_1, true, 0x12345678u));
      CHECK(r.out().attn_scale == 0x12345678, "B13 attn_scale");
      // POOLING_1 fields (10 bits each) and SOFTMAX_0 fields (9 bits each)
      r.tick(mk(CFG_POOLING_1, true, (0x155u << 10) | 0x2AAu));
      CHECK(r.out().pooling_h_move == 0x2AAu && r.out().pooling_c_move == 0x155u, "B13: moves");
      r.tick(mk(CFG_SOFTMAX_0, true, (0x1A5u << 9) | 0x0C3u));
      CHECK(r.out().token_len == 0x0C3u && r.out().num_tile == 0x1A5u, "B13: SOFTMAX_0");
      r.tick(mk(CFG_SOFTMAX_2, true, 0xA55Au));
      CHECK(r.out().attn_shift == 0x5Au && static_cast<uint8_t>(r.out().attn_z) == 0xA5u, "B13: SOFTMAX_2");
      r.tick(mk(CFG_CONTROL, true, (7u << 3) | 1u));
      CHECK(r.out().op == 7 && r.out().pipe_en, "B13: opcode / enable");
    }
    // ---- interactions (patched variant, both give the same here)
    for (int variant = 0; variant < 2; variant++)
    {
        GvuConfigParams p; p.b13_fixed = variant == 1; GvuConfigRegs r(p);
        r.tick(mk(CFG_CONTROL, true, 1u));                                     // enable
        CHECK(r.out().pipe_en, "enable written");
        GvuConfigIn in = mk(CFG_CONTROL, true, 1u); in.gvu_pipe_done = true;    // host writes bit 0 on the pipe_done edge
        r.tick(in);
        CHECK(!r.out().pipe_en && (r.reg(CFG_CONTROL) & 2u), "pipe_done beats the host write of bit 0, sets the done flag (v%d)", variant);
        in = mk(CFG_CONTROL, true, 4u); in.gvu_pipe_done = true;                // clear request on a pipe_done edge: clear wins
        r.tick(in);
        CHECK(!(r.reg(CFG_CONTROL) & 2u), "clear beats pipe_done set (v%d)", variant);
        // clear request ignores the mask
        in = mk(CFG_CONTROL, true, 4u, 0u); in.gvu_pipe_done = true; r.tick(in);
        CHECK(!(r.reg(CFG_CONTROL) & 2u), "clear request looks at wdata[2] only (v%d)", variant);
        // done flag stays when nothing clears it
        in = GvuConfigIn(); in.gvu_pipe_done = true; r.tick(in); r.tick(GvuConfigIn());
        CHECK(r.reg(CFG_CONTROL) & 2u, "done flag held (v%d)", variant);
        // vbank1 buffer toggle
        r.tick(GvuConfigIn());
        in = GvuConfigIn(); in.lut_init_done = true; r.tick(in);
        CHECK(r.out().vbank1_buf_sel, "toggle 0 -> 1 (v%d)", variant);
        r.tick(in);
        CHECK(!r.out().vbank1_buf_sel, "toggle 1 -> 0 (v%d)", variant);
        in = GvuConfigIn(); in.lut_init_done = true; r.tick(in);                    // bit 2 = 1 again
        CHECK(r.out().vbank1_buf_sel, "toggle 0 -> 1 again (v%d)", variant);
        in = mk(CFG_SCRATCHPAD, true, 0x9u); in.lut_init_done = true; r.tick(in);   // host writes bit 2 = 0 on a toggle edge: old value (1) inverted = 0
        CHECK(!r.out().vbank1_buf_sel && r.out().sbank_buf_sel && r.out().vbank2_buf_sel, "toggle inverts the OLD value, other bits take the write (v%d)", variant);
        in = mk(CFG_SCRATCHPAD, true, 0x0u); in.lut_init_done = true; r.tick(in);   // old 0 inverted = 1 although the host writes 0
        CHECK(r.out().vbank1_buf_sel && !r.out().sbank_buf_sel, "toggle beats the host write (v%d)", variant);
        // registered read, one cycle; the value is the one from before the edge
        in = mk(CFG_GEMM_FUSED, true, 0x55u); in.rden = true; in.addr = CFG_GEMM_FUSED;
        r.tick(in);
        CHECK(r.rdata() == 0u, "read sees the value from before a same-edge write (v%d)", variant);
        in = GvuConfigIn(); in.addr = CFG_GEMM_FUSED; in.rden = true; r.tick(in);
        CHECK(r.rdata() == 0x55u, "read data (v%d) 0x%x", variant, r.rdata());
        // o_rdata holds without rden
        in = GvuConfigIn(); in.addr = CFG_CONTROL; r.tick(in);
        CHECK(r.rdata() == 0x55u, "o_rdata holds (v%d)", variant);
        // async reset
        in = GvuConfigIn(); in.rstn = false; r.tick(in);
        CHECK(r.reg(CFG_GEMM_FUSED) == 0 && r.reg(CFG_SCRATCHPAD) == 0 && r.rdata() == 0x55u, "reset clears registers, not o_rdata (v%d)", variant);
    }
    // ---- word index (B17)
    { GvuConfigRegs a; GvuConfigParams p; p.word_addressed = true; GvuConfigRegs b(p);
      CHECK(a.reg_index(0x0C) == 12 && b.reg_index(0x0C) == 3, "index 0x0C: %u / %u", a.reg_index(0x0C), b.reg_index(0x0C));
      CHECK(a.reg_index(0x14) == 4 && b.reg_index(0x14) == 5, "index 0x14: %u / %u", a.reg_index(0x14), b.reg_index(0x14));
      CHECK(a.reg_index(0x20) == 0 && b.reg_index(0x20) == 8, "index 0x20");
      CHECK(GvuConfigRegs::strb_to_mask(0x2) == 0x0000FF00u && GvuConfigRegs::strb_to_mask(0xF) == 0xFFFFFFFFu, "strb mask");
      GvuConfigParams q; q.b13_fixed = true; q.word_addressed = true; GvuConfigRegs c(q);
      c.write_axi(0x0C, 0x0000C501u); c.write_axi(0x14, 0x12345678u);
      CHECK(c.read_axi(0x0C) == 0xC501u && c.read_axi(0x14) == 0x12345678u, "patched host path read back");
      CHECK(static_cast<uint8_t>(c.out().gemm_fused_z) == 0xC5 && c.out().gemm_fused_skip_act && c.out().attn_scale == 0x12345678, "patched host path decode");
      // as delivered: Reg 3 written at offset 0x0C lands in register 12 (not used), Reg 5 at 0x14 in register 4
      GvuConfigRegs d; d.write_axi(0x0C, 0x0000C501u); d.write_axi(0x14, 0x12345678u);
      CHECK(d.reg(12) == 0x01u && d.reg(4) == 0x78u && d.reg(CFG_GEMM_FUSED) == 0 && d.out().attn_scale == 0, "as delivered host path: wrong registers");
    }
}

// ---- registers 9..15 (residual add proposal) and the 32-register variant
static void directed_res()
{
    for (unsigned nr = 16; nr <= 32; nr += 16)
    {
        GvuConfigParams p; p.b13_fixed = true; p.word_addressed = true; p.num_regs = nr; GvuConfigRegs r(p);
        // reset state: everything 0
        { const GvuConfigOut o = r.out();
          CHECK(o.res_scale_a == 0 && o.res_scale_b == 0 && o.res_scale_out == 0 && o.res_shift_a == 0 && o.res_shift_b == 0 && o.res_shift_out == 0 &&
                o.res_round == 0 && !o.res_out_int16 && o.res_zp_a == 0 && o.res_zp_b == 0 && o.res_zp_out == 0 && o.res_a_row == 0 && o.res_b_row == 0 &&
                o.res_sum_row == 0 && o.res_last_lane_mask == 0, "res_* reset state (num_regs %u)", nr); }
        // full 32-bit writes through the AXI-Lite path (word address = 4 * register)
        r.write_axi(4 * CFG_RES_SCALE_A, 0x80000001u);          // negative int32
        r.write_axi(4 * CFG_RES_SCALE_B, 0x7FFFFFFFu);
        r.write_axi(4 * CFG_RES_SCALE_OUT, 0xFFFFFFFFu);
        r.write_axi(4 * CFG_RES_SHIFT, (1u << 27) | (6u << 24) | (63u << 16) | (0xA5u << 8) | 17u);
        r.write_axi(4 * CFG_RES_ZP, (0x80u << 16) | (0x7Fu << 8) | 0xF6u);
        r.write_axi(4 * CFG_RES_ROW, (799u << 20) | (127u << 10) | 255u);
        r.write_axi(4 * CFG_RES_LASTMASK, 0x0000FFFFu);
        { const GvuConfigOut o = r.out();
          CHECK(o.res_scale_a == static_cast<int32_t>(0x80000001u) && o.res_scale_b == 0x7FFFFFFF && o.res_scale_out == -1, "res scales (num_regs %u)", nr);
          CHECK(o.res_shift_a == 17 && o.res_shift_b == 0xA5 && o.res_shift_out == 63 && o.res_round == 6 && o.res_out_int16, "res shift fields (num_regs %u)", nr);
          CHECK(o.res_zp_a == -10 && o.res_zp_b == 127 && o.res_zp_out == -128, "res zp fields %d %d %d (num_regs %u)", o.res_zp_a, o.res_zp_b, o.res_zp_out, nr);
          CHECK(o.res_a_row == 255 && o.res_b_row == 127 && o.res_sum_row == 799, "res rows %u %u %u (num_regs %u)", o.res_a_row, o.res_b_row, o.res_sum_row, nr);
          CHECK(o.res_last_lane_mask == 0x0000FFFFu, "res last mask (num_regs %u)", nr); }
        // read back through the registered read path
        CHECK(r.read_axi(4 * CFG_RES_SHIFT) == ((1u << 27) | (6u << 24) | (63u << 16) | (0xA5u << 8) | 17u), "read back Reg 12 (num_regs %u)", nr);
        CHECK(r.read_axi(4 * CFG_RES_LASTMASK) == 0x0000FFFFu, "read back Reg 15 (num_regs %u)", nr);
        // partial write by strobe: only byte 2 of Reg 13
        r.write_axi(4 * CFG_RES_ZP, 0x00330000u, 0x4);
        CHECK(r.out().res_zp_out == 0x33 && r.out().res_zp_b == 127 && r.out().res_zp_a == -10, "res zp byte write (num_regs %u)", nr);
        // the other decodes do not change: Reg 0..8 untouched
        CHECK(r.reg(CFG_CONTROL) == 0 && r.reg(CFG_POOLING_1) == 0 && r.reg(CFG_SOFTMAX_1) == 0, "Reg 0..8 untouched by res writes (num_regs %u)", nr);
        // a write to Reg 0 does not disturb the res_* outputs, and reset clears them
        r.write_axi(4 * CFG_CONTROL, (6u << 3) | 1u);
        CHECK(r.out().op == 6 && r.out().pipe_en && r.out().res_scale_a == static_cast<int32_t>(0x80000001u), "opcode 6 next to res_* (num_regs %u)", nr);
        { GvuConfigIn in; in.rstn = false; r.tick(in); }
        CHECK(r.out().res_scale_a == 0 && r.out().res_last_lane_mask == 0 && r.out().res_a_row == 0 && r.out().op == 0, "reset clears res_* (num_regs %u)", nr);
    }
    // 16 versus 32: the index wraps at the register count
    { GvuConfigParams p16; p16.b13_fixed = true; p16.word_addressed = true; GvuConfigRegs a(p16);
      GvuConfigParams p32 = p16; p32.num_regs = 32; GvuConfigRegs b(p32);
      CHECK(a.reg_index(0x40) == 0 && b.reg_index(0x40) == 16 && a.reg_index(0x7C) == 15 && b.reg_index(0x7C) == 31, "word index 0x40 / 0x7C: %u %u %u %u", a.reg_index(0x40), b.reg_index(0x40), a.reg_index(0x7C), b.reg_index(0x7C));
      a.write_axi(0x40, 0x1u);                       // wraps onto CONTROL (register 0): enable
      b.write_axi(0x40, 0x1u);                       // register 16: storage only
      CHECK(a.out().pipe_en && !b.out().pipe_en && b.reg(16) == 1u && a.reg(0) == 1u, "num_regs 16 wraps, 32 does not");
      b.write_axi(0x7C, 0xCAFEF00Du);
      CHECK(b.read_axi(0x7C) == 0xCAFEF00Du && b.reg(31) == 0xCAFEF00Du && b.reg(15) == 0, "register 31 storage");
      GvuConfigParams pq = p32; pq.num_regs = 24; GvuConfigRegs c(pq);          // anything but 32 counts as 16
      CHECK(c.params().num_regs == 16, "num_regs 24 -> 16");
      // as delivered byte index: low 5 bits when 32
      GvuConfigParams pd; pd.num_regs = 32; GvuConfigRegs d(pd);
      CHECK(d.reg_index(0x1F) == 31 && d.reg_index(0x20) == 0, "byte index 5 bits");
    }
    // same stimulus on 16 and 32 registers: outputs of Reg 0..8 identical edge by edge (stimulus addresses 0..8 only)
    { std::mt19937_64 rg(7);
      GvuConfigParams p16; p16.b13_fixed = true; GvuConfigRegs a(p16);
      GvuConfigParams p32 = p16; p32.num_regs = 32; GvuConfigRegs b(p32);
      long diff = 0;
      for (int i = 0; i < 20000; i++)
      {
          GvuConfigIn in; in.rstn = (rg() % 500) != 0; in.addr = static_cast<unsigned>(rg() % 9); in.wren = rg() % 100 < 40; in.rden = rg() % 100 < 30;
          in.wmask = static_cast<uint32_t>(rg()); in.wdata = static_cast<uint32_t>(rg());
          in.lut_init_done = rg() % 100 < 8; in.gvu_pipe_done = rg() % 100 < 8;
          a.tick(in); b.tick(in);
          const GvuConfigOut x = a.out(), y = b.out();
          if (std::memcmp(&x, &y, sizeof(GvuConfigOut)) != 0 && (x.op != y.op || x.rdata != y.rdata || x.pipe_en != y.pipe_en || x.attn_scale != y.attn_scale ||
              x.pooling_c != y.pooling_c || x.pooling_h_move != y.pooling_h_move || x.token_len != y.token_len || x.vbank1_buf_sel != y.vbank1_buf_sel)) diff++;
      }
      CHECK(diff == 0, "num_regs 16 vs 32 differ on %ld edges", diff);
    }
}

// ---------------------------------------------------------------------------------------------------- dump
static std::mt19937_64 g_rng(0x47564346u);   // "GVCF"
static unsigned rnd(unsigned n) { return static_cast<unsigned>(g_rng() % n); }

struct Stats { long both_pd_w0 = 0, pd_clear = 0, lut_w2 = 0, writes = 0, reads = 0, resets = 0, mid_bytes = 0; };

static void dump(const std::string &path, bool b13, long cycles, bool oob)
{
    g_rng.seed(0x47564346u);
    GvuConfigParams p; p.b13_fixed = b13; p.flat_array_oob = oob;
    GvuConfigRegs dut(p);
    FILE *f = std::fopen(path.c_str(), "w");
    if (!f) { std::printf("cannot write %s\n", path.c_str()); std::exit(1); }
    Stats st; bool rd_seen = false; int rst_left = 3;
    for (long cyc = 0; cyc < cycles; cyc++)
    {
        GvuConfigIn in;
        if (rst_left > 0) { in.rstn = false; rst_left--; }
        else if (rnd(2500) == 0) { in.rstn = false; rst_left = static_cast<int>(rnd(3)); }
        in.addr = rnd(10) < 7 ? rnd(9) : rnd(16);
        in.wren = rnd(100) < 40; in.rden = rnd(100) < 30;
        switch (rnd(10))
        {
            case 0: case 1: case 2: in.wmask = 0xFFFFFFFFu; break;
            case 3: case 4: case 5: in.wmask = GvuConfigRegs::strb_to_mask(rnd(16)); break;
            case 6: case 7: in.wmask = static_cast<uint32_t>(g_rng()); break;
            default: in.wmask = 1u << (8 * rnd(4)); break;
        }
        in.wdata = static_cast<uint32_t>(g_rng());
        if (in.addr == CFG_CONTROL && rnd(2)) in.wdata |= 4u; else if (in.addr == CFG_CONTROL) in.wdata &= ~4u;
        in.lut_init_done = rnd(100) < 8; in.gvu_pipe_done = rnd(100) < 8;
        if (in.rstn)
        {
            if (in.wren) st.writes++;
            if (in.rden) st.reads++;
            if (in.gvu_pipe_done && in.wren && in.addr == CFG_CONTROL && (in.wmask & 1u) && (in.wdata & 1u)) st.both_pd_w0++;
            if (in.gvu_pipe_done && in.wren && in.addr == CFG_CONTROL && (in.wdata & 4u)) st.pd_clear++;
            if (in.lut_init_done && in.wren && in.addr == CFG_SCRATCHPAD) st.lut_w2++;
            if (in.wren && (in.wmask & 0xFFFFFF00u)) st.mid_bytes++;
        }
        else st.resets++;
        std::fprintf(f, "%x %x %x %x %08x %08x %x %x | ", in.rstn, in.addr, in.wren, in.rden, in.wmask, in.wdata, in.lut_init_done, in.gvu_pipe_done);
        dut.tick(in);
        if (in.rden) rd_seen = true;
        const GvuConfigOut o = dut.out();
        std::fprintf(f, "%x %08x %x %x %x %x %x %02x %03x %03x %02x %08x %02x %x %x %02x %02x %02x %03x %03x\n", rd_seen ? 1 : 0, o.rdata, o.pipe_en, o.op,
                     (o.lut_rsqrt_reset << 3) | (o.lut_recip_reset << 2) | (o.lut_exp_reset << 1) | (o.lut_act_reset ? 1 : 0),
                     (o.vbank2_buf_sel << 3) | (o.vbank1_buf_sel << 2) | (o.vbank0_buf_sel << 1) | (o.sbank_buf_sel ? 1 : 0),
                     o.gemm_fused_skip_act, static_cast<uint8_t>(o.gemm_fused_z), o.num_tile, o.token_len, static_cast<uint8_t>(o.attn_z),
                     static_cast<uint32_t>(o.attn_scale), o.attn_shift, o.pooling_mode, o.pooling_stride, o.pooling_num_w_tile, o.pooling_c,
                     o.pooling_oh, o.pooling_c_move, o.pooling_h_move);
    }
    std::fclose(f);
    std::printf("DUMP %s b13 %d cycles %ld: writes %ld reads %ld resets %ld; same-edge cases: pipe_done+write bit0 %ld, pipe_done+clear %ld, lut_init_done+write Reg 2 %ld, writes touching bytes 1-3 %ld\n",
                path.c_str(), b13 ? 1 : 0, cycles, st.writes, st.reads, st.resets, st.both_pd_w0, st.pd_clear, st.lut_w2, st.mid_bytes);
}

int main(int argc, char **argv)
{
    if (argc >= 3 && std::strcmp(argv[1], "--dump") == 0)
    {
        const std::string dir = argv[2];
        const long cycles = argc >= 4 ? std::atol(argv[3]) : 200000;
        dump(dir + "/cfgregs_dlv.txt", false, cycles, false);
        dump(dir + "/cfgregs_b13.txt", true, cycles, false);
        dump(dir + "/cfgregs_dlvvl.txt", false, cycles, true);   // delivered RTL as Verilator simulates it
        std::printf("RESULT: DUMP DONE\n");
        return 0;
    }
    directed();
    directed_res();
    std::printf("RESULT: %s (%d checks failed)\n", g_fail == 0 ? "PASS" : "FAIL", g_fail);
    return g_fail == 0 ? 0 : 1;
}
