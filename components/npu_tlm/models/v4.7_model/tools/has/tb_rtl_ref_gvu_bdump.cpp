// tb_rtl_ref_gvu_bdump.cpp -- writes cycle-by-cycle stimulus and the outputs of the ported blocks as text, for replay on
// the RTL by tools/has/gvu_oracle/tb_oracle_b.sv: LUT unit (has/rtl_ref_gvu_lut.h), comparator (has/rtl_ref_gvu_fu.h),
// stream registers (has/rtl_ref_gvu_vrf.h), Scratchpad (has/rtl_ref_gvu_scratchpad.h), load / store unit
// (has/rtl_ref_gvu_vlsu.h). One line per clock cycle, hex fields; wide values are printed most significant digit first.
//   lutb_tab.txt   act exp recip rsqrt, one line per table index 0..511: tables placed directly in the memory
//   lutb.txt       valid sel mode compens base W lerp_shift idx8 idx32 ready | o_ready o_valid o_zero o_8b o_16b o_e_shift o_almost_done
//   lutb_wr.txt    wen wsel wdata(128 bit) | o_init_done write_address write_count     table load through the write port
//   lutb_mem.txt   the four tables after lutb_wr.txt, same format as lutb_tab.txt
//   cmpb0.txt, cmpb1.txt   OUT_PIPE 0 / 1, two lanes: every input | in_ready out_valid out_res
//   svrf.txt       stream_vrf: three configuration inputs, 19 ports (flush valid ready data) | 19 ports (o_ready o_valid o_data)
//   spadb.txt      every accelerator input of gvu_scratchpad | the eight read outputs of the same cycle
//   vlsub.txt      every input of gvu_vlsu | every output of the same cycle
//   lut32.txt      gvu_lut_unit with 32 lanes: inputs of every lane | handshake of lane 0 and the outputs of every lane
//   lutload_v1.txt, lutload.txt, lutload_mem.txt   table load of gvu_top.sv: rows of vbank1, state and load signals per cycle, tables after it
// build: g++ -std=c++17 -O2 -Wall -I. tools/has/tb_rtl_ref_gvu_bdump.cpp -o tb_rtl_ref_gvu_bdump ; run: <binary> <out dir> [bytemask]
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include "has/rtl_ref_gvu_ctrl.h"
#include "has/rtl_ref_gvu_lut.h"
#include "has/rtl_ref_gvu_scratchpad.h"
#include "has/rtl_ref_gvu_vlsu.h"
#include "has/rtl_ref_gvu_vrf.h"

using namespace rtl_ref_gvu;
static std::mt19937_64 g_rng(0x47565542u);

static void hex_bytes(FILE *f, const uint8_t *d, size_t n) { for (size_t i = n; i-- > 0;) fprintf(f, "%02x", d[i]); }
static void hex_v(FILE *f, const std::vector<uint8_t> &d) { hex_bytes(f, d.data(), d.size()); }
static std::vector<uint8_t> rnd_bytes(size_t n) { std::vector<uint8_t> r(n); for (auto &b : r) b = static_cast<uint8_t>(g_rng()); return r; }
static bool one_in(int n) { return g_rng() % static_cast<uint64_t>(n) == 0; }

static uint32_t rnd_x()
{
    switch (g_rng() % 8)
    {
    case 0: return 1u + static_cast<uint32_t>(g_rng() % 300);
    case 1: return 0xFFFFFFFFu - static_cast<uint32_t>(g_rng() % 300);
    case 2: return 1u << (g_rng() % 32);
    case 3: { const int e = 8 + static_cast<int>(g_rng() % 24); return (0x1FFu << (e - 8)) | static_cast<uint32_t>(g_rng() & ((1u << (e - 8)) - 1)); }
    default: { const uint32_t v = static_cast<uint32_t>(g_rng()) >> (g_rng() % 32); return v ? v : 1u; }
    }
}

static void dump_tables(const std::string &path, const LutMem &mem)
{
    FILE *f = fopen(path.c_str(), "w");
    for (unsigned k = 0; k < 512; k++) fprintf(f, "%02x %02x %04x %04x\n", mem.peek8(LUT_ACT, k), mem.peek8(LUT_EXP, k), mem.peek16(LUT_RECIP, k), mem.peek16(LUT_RSQRT, k));
    fclose(f);
}

static void dump_lut(const std::string &dir)
{
    LutMem mem;
    std::vector<uint8_t> act(256), ex(256);
    std::vector<uint16_t> recip(256), rsqrt(512);
    for (unsigned k = 0; k < 256; k++) { act[k] = static_cast<uint8_t>(255 - k); ex[k] = static_cast<uint8_t>(k * 7 + 3); }
    for (unsigned k = 0; k < 512; k++)
    {
        const unsigned p = k >> 8, j = k & 255;
        rsqrt[k] = static_cast<uint16_t>(std::lround(std::ldexp(1.0 / std::sqrt((1.0 + j / 256.0) * (1 << p)), 14)));
        // steeper than 1 / x in places so the 8-bit interpolation difference also wraps
        if (k < 256) recip[k] = static_cast<uint16_t>(std::lround(std::ldexp(1.0 / (1.0 + j / 256.0), 14)) + ((j % 37 == 5) ? 300 : 0));
    }
    mem.load8(LUT_ACT, act); mem.load8(LUT_EXP, ex); mem.load16(LUT_RECIP, recip); mem.load16(LUT_RSQRT, rsqrt);
    dump_tables(dir + "/lutb_tab.txt", mem);

    FILE *f = fopen((dir + "/lutb.txt").c_str(), "w");
    LutLookupLane lane;
    // segment: table, index mode, share of zero inputs (per 16), ready probability (per 4), offset of the 8-bit index, base, W, lerp shift
    const struct { LutType sel; LutIdxMode mode; int zeros, ready4; bool compens; unsigned base, W, sh; } seg[] = {
        {LUT_RECIP, LUT_PWL, 0, 3, false, 7, 16, 8}, {LUT_RSQRT, LUT_PWL, 0, 3, false, 7, 16, 8}, {LUT_RECIP, LUT_PWL, 0, 4, false, 7, 16, 8},
        {LUT_RECIP, LUT_LOG_SCALE, 0, 4, false, 7, 16, 8}, {LUT_RSQRT, LUT_LOG_SCALE, 0, 4, false, 3, 16, 8}, {LUT_RECIP, LUT_LOG_SCALE, 0, 3, false, 0, 16, 8},
        {LUT_RECIP, LUT_PWL, 4, 4, false, 7, 16, 8}, {LUT_RSQRT, LUT_PWL, 4, 3, false, 7, 16, 8}, {LUT_RECIP, LUT_LOG_SCALE, 4, 3, false, 5, 16, 8},
        {LUT_ACT, LUT_PWL, 0, 3, false, 0, 16, 8}, {LUT_EXP, LUT_PWL, 4, 3, false, 0, 16, 8}, {LUT_ACT, LUT_PWL, 2, 3, true, 0, 16, 8}, {LUT_EXP, LUT_LOG_SCALE, 0, 3, true, 0, 16, 8},
        {LUT_RECIP, LUT_PWL, 1, 3, false, 7, 12, 5}, {LUT_RSQRT, LUT_PWL, 1, 3, true, 7, 20, 11},
    };
    bool v = false; uint32_t x = 1;
    for (const auto &s : seg)
        for (int cyc = 0; cyc < 20000; cyc++)
        {
            const bool drain = cyc >= 19980;                         // empty the pipeline before the controls change
            if (!v) { v = !drain && (g_rng() % 4) != 0; x = (s.zeros && static_cast<int>(g_rng() % 16) < s.zeros) ? 0u : rnd_x(); }
            if (drain) v = false;
            LutLookupLane::In in;
            in.i_lookup_valid = v; in.i_lut_sel = s.sel; in.i_lut_idx_mode = s.mode; in.i_lut_compens = s.compens; in.i_base = s.base; in.i_W = s.W; in.i_lerp_shift = s.sh;
            in.i_8b_lookup_idx = static_cast<uint32_t>(g_rng() & 0xFF); in.i_32b_lookup_idx = x;
            in.i_lookup_ready = drain || static_cast<int>(g_rng() % 4) < s.ready4;
            const LutLookupLane::Out o = lane.eval(in, mem);
            fprintf(f, "%x %x %x %x %02x %02x %02x %02x %08x %x %x %x %x %02x %04x %02x %x\n", v ? 1 : 0, s.sel, s.mode, s.compens ? 1 : 0, s.base, s.W, s.sh, in.i_8b_lookup_idx, x,
                    in.i_lookup_ready ? 1 : 0, o.o_lookup_ready ? 1 : 0, o.o_lookup_valid ? 1 : 0, 0 /* column kept: the unit has no zero-flag output */, o.o_8b_lookup_data, o.o_16b_lookup_data, o.o_e_shift,
                    o.o_pipe_almost_done ? 1 : 0);
            lane.tick(in, mem);
            if (v && o.o_lookup_ready) v = false;
        }
    fclose(f);

    // table load through the write port: runs of one table (as a controller would do) mixed with table changes mid-run
    FILE *fw = fopen((dir + "/lutb_wr.txt").c_str(), "w");
    LutType wsel = LUT_ACT;
    for (int cyc = 0; cyc < 6000; cyc++)
    {
        if (cyc < 3000 ? one_in(40) : one_in(5)) wsel = static_cast<LutType>(g_rng() % 4);
        const bool wen = cyc < 3000 ? !one_in(6) : (g_rng() & 1);
        const std::vector<uint8_t> d = rnd_bytes(16);
        fprintf(fw, "%x %x ", wen ? 1 : 0, wsel); hex_v(fw, d);
        fprintf(fw, " %x %03x %x\n", mem.o_init_done(wsel) ? 1 : 0, mem.lut_waddr(), mem.wr_cnt());
        mem.tick(true, wen, wsel, d.data());
    }
    fclose(fw);
    dump_tables(dir + "/lutb_mem.txt", mem);
}

static void dump_cmp(const std::string &dir, bool out_pipe)
{
    FuParams p; p.CMP_OUT_PIPE = out_pipe;
    Vcmp32 dut(p);
    FILE *f = fopen((dir + (out_pipe ? "/cmpb1.txt" : "/cmpb0.txt")).c_str(), "w");
    int left = 0; CmpMode mode = CMP_ELEMENTWISE; CmpOp op = CMP_MAX; ElemW ew = EW_32; bool sg = true;
    for (int cyc = 0; cyc < 200000; cyc++)
    {
        if (left == 0) { left = 1 + static_cast<int>(g_rng() % 40); mode = (g_rng() & 1) ? CMP_ACCUMULATE : CMP_ELEMENTWISE; op = (g_rng() & 1) ? CMP_MIN : CMP_MAX; ew = static_cast<ElemW>(g_rng() % 3); sg = g_rng() & 1; }
        left--;
        Vcmp32::In i{};
        i.in_valid = !one_in(4); i.in_cmp_mode = mode; i.in_op = op; i.in_elem_w = ew; i.in_signed = sg;
        i.in_src1_bcast = one_in(3); i.in_first = one_in(5); i.in_last = one_in(4); i.out_ready = !one_in(3);
        i.in_src0.fill(0); i.in_src1.fill(0);
        for (int l = 0; l < 2; l++)
        {
            // values close to each other so that ties and sign changes are frequent
            i.in_src0[static_cast<size_t>(l)] = one_in(3) ? static_cast<uint32_t>(g_rng()) : (0x7FFFFFFEu + static_cast<uint32_t>(g_rng() % 5)) ^ (one_in(2) ? 0x00800080u : 0u);
            i.in_src1[static_cast<size_t>(l)] = one_in(3) ? static_cast<uint32_t>(g_rng()) : (0x7FFFFFFEu + static_cast<uint32_t>(g_rng() % 5)) ^ (one_in(2) ? 0x00800080u : 0u);
        }
        const Vcmp32::Out o = dut.eval(i);
        fprintf(f, "%x %x %x %x %x %x %x %x %08x %08x %08x %08x %x %x %x %08x %08x\n", i.in_valid ? 1 : 0, mode, op, ew, sg ? 1 : 0, i.in_src1_bcast ? 1 : 0, i.in_first ? 1 : 0, i.in_last ? 1 : 0,
                i.in_src0[0], i.in_src0[1], i.in_src1[0], i.in_src1[1], i.out_ready ? 1 : 0, o.in_ready ? 1 : 0, o.out_valid ? 1 : 0, o.out_res[0], o.out_res[1]);
        dut.tick(i);
    }
    fclose(f);
}

static void dump_svrf(const std::string &dir)
{
    StreamVrf dut;
    FILE *f = fopen((dir + "/svrf.txt").c_str(), "w");
    StreamVrf::In i;
    for (int cyc = 0; cyc < 6000; cyc++)
    {
        if (cyc % 40 == 0) { i.i_vint32_0_cfg_mode = static_cast<unsigned>(g_rng() % 4); i.i_vint64_0_cfg_mode = g_rng() & 1; i.i_vint64_2_cfg_mode = g_rng() & 1; }
        auto fill = [&](StreamVrf::Port &q, size_t n) { q.flush = one_in(50); q.valid = !one_in(3); q.ready = !one_in(3); q.data = rnd_bytes(n); };
        for (auto &q : i.vint64) fill(q, 256);
        for (auto &q : i.vint32) fill(q, 128);
        for (auto &q : i.vint16) fill(q, 64);
        for (auto &q : i.vint8) fill(q, 32);
        const StreamVrf::Out o = dut.eval(i);
        fprintf(f, "%x %x %x", i.i_vint32_0_cfg_mode, i.i_vint64_0_cfg_mode ? 1 : 0, i.i_vint64_2_cfg_mode ? 1 : 0);
        auto pin = [&](const StreamVrf::Port &q) { fprintf(f, " %x %x %x ", q.flush ? 1 : 0, q.valid ? 1 : 0, q.ready ? 1 : 0); hex_v(f, q.data); };
        for (const auto &q : i.vint64) pin(q);
        for (const auto &q : i.vint32) pin(q);
        for (const auto &q : i.vint16) pin(q);
        for (const auto &q : i.vint8) pin(q);
        auto pout = [&](const StreamVrf::PortOut &q) { fprintf(f, " %x %x ", q.o_ready ? 1 : 0, q.o_valid ? 1 : 0); hex_v(f, q.o_data); };
        for (const auto &q : o.vint64) pout(q);
        for (const auto &q : o.vint32) pout(q);
        for (const auto &q : o.vint16) pout(q);
        for (const auto &q : o.vint8) pout(q);
        fprintf(f, "\n");
        dut.tick(i);
    }
    fclose(f);
}

// write mask of n bits: all ones, random, or one bit
static SpBytes rnd_mask(int nbits)
{
    SpBytes m(static_cast<size_t>((nbits + 7) / 8), 0);
    const int kind = static_cast<int>(g_rng() % 4);
    for (int k = 0; k < nbits; k++)
    {
        const bool b = kind < 2 ? true : (kind == 2 ? (g_rng() & 1) : false);
        if (b) m[static_cast<size_t>(k / 8)] = static_cast<uint8_t>(m[static_cast<size_t>(k / 8)] | (1u << (k % 8)));
    }
    if (kind == 3) { const int k = static_cast<int>(g_rng() % static_cast<uint64_t>(nbits)); m[static_cast<size_t>(k / 8)] = static_cast<uint8_t>(1u << (k % 8)); }
    return m;
}
static unsigned rnd_addr(int depth) { return static_cast<unsigned>(one_in(4) ? g_rng() % static_cast<uint64_t>(depth) : g_rng() % 8); }

static void dump_scratchpad(const std::string &dir, bool wmask_as_written)
{
    ScratchpadParams P;
    P.wmask_as_written = wmask_as_written;
    GvuScratchpad dut(P);
    FILE *f = fopen((dir + "/spadb.txt").c_str(), "w");
    GvuScratchpad::In i;
    for (int cyc = 0; cyc < 40000; cyc++)
    {
        if (one_in(64)) i.i_sbank_buf_sel = !i.i_sbank_buf_sel;
        if (one_in(64)) i.i_vbank0_buf_sel = !i.i_vbank0_buf_sel;
        if (one_in(64)) i.i_vbank1_buf_sel = !i.i_vbank1_buf_sel;
        if (one_in(64)) i.i_vbank2_buf_sel = !i.i_vbank2_buf_sel;
        i.i_sbank_en_0 = !one_in(3); i.i_sbank_wen_0 = one_in(3); i.i_sbank_addr_0 = rnd_addr(P.SBANK_DEPTH); i.i_sbank_wmask_0 = one_in(2) ? 0xFu : static_cast<unsigned>(g_rng() & 0xF); i.i_sbank_wdata_0 = static_cast<uint32_t>(g_rng());
        i.i_sbank_en_1 = !one_in(3); i.i_sbank_wen_1 = one_in(3); i.i_sbank_addr_1 = (rnd_addr(P.SBANK_DEPTH) << 2) | static_cast<unsigned>(g_rng() & 3); i.i_sbank_wmask_1 = !one_in(4); i.i_sbank_wdata_1 = static_cast<uint8_t>(g_rng());
        i.i_vbank0_en = !one_in(3); i.i_vbank0_wen = one_in(3); i.i_vbank0_addr = rnd_addr(P.VBANK0_DEPTH); i.i_vbank0_wmask = rnd_mask(32); i.i_vbank0_wdata = rnd_bytes(32);
        i.i_vbank1_port_sel = static_cast<unsigned>(g_rng() % 4);
        i.i_vbank1_en_0 = !one_in(3); i.i_vbank1_wen_0 = one_in(3); i.i_vbank1_addr_0 = rnd_addr(P.VBANK1_DEPTH); i.i_vbank1_wmask_0 = rnd_mask(128); i.i_vbank1_wdata_0 = rnd_bytes(128);
        i.i_vbank1_en_1 = !one_in(3); i.i_vbank1_wen_1 = one_in(3); i.i_vbank1_addr_1 = (rnd_addr(P.VBANK1_DEPTH) << 1) | static_cast<unsigned>(g_rng() & 1); i.i_vbank1_wmask_1 = rnd_mask(64); i.i_vbank1_wdata_1 = rnd_bytes(64);
        i.i_vbank1_en_2 = !one_in(3); i.i_vbank1_wen_2 = one_in(3); i.i_vbank1_addr_2 = (rnd_addr(P.VBANK1_DEPTH) << 2) | static_cast<unsigned>(g_rng() & 3); i.i_vbank1_wmask_2 = rnd_mask(32); i.i_vbank1_wdata_2 = rnd_bytes(32);
        i.i_vbank2_port_sel = g_rng() & 1;
        i.i_vbank2_en_0 = !one_in(3); i.i_vbank2_wen_0 = one_in(3); i.i_vbank2_addr_0 = rnd_addr(P.VBANK2_DEPTH); i.i_vbank2_wmask_0 = rnd_mask(128); i.i_vbank2_wdata_0 = rnd_bytes(128);
        i.i_vbank2_en_1 = !one_in(3); i.i_vbank2_wen_1 = one_in(3); i.i_vbank2_addr_1 = (rnd_addr(P.VBANK2_DEPTH) << 2) | static_cast<unsigned>(g_rng() & 3); i.i_vbank2_wmask_1 = rnd_mask(32); i.i_vbank2_wdata_1 = rnd_bytes(32);
        const GvuScratchpad::Out o = dut.out(i);
        fprintf(f, "%x %x %x %03x %x %08x %x %x %03x %x %02x ", i.i_sbank_buf_sel ? 1 : 0, i.i_sbank_en_0 ? 1 : 0, i.i_sbank_wen_0 ? 1 : 0, i.i_sbank_addr_0, i.i_sbank_wmask_0, i.i_sbank_wdata_0,
                i.i_sbank_en_1 ? 1 : 0, i.i_sbank_wen_1 ? 1 : 0, i.i_sbank_addr_1, i.i_sbank_wmask_1 ? 1 : 0, i.i_sbank_wdata_1);
        fprintf(f, "%x %x %x %03x ", i.i_vbank0_buf_sel ? 1 : 0, i.i_vbank0_en ? 1 : 0, i.i_vbank0_wen ? 1 : 0, i.i_vbank0_addr); hex_v(f, i.i_vbank0_wmask); fprintf(f, " "); hex_v(f, i.i_vbank0_wdata);
        fprintf(f, " %x %x ", i.i_vbank1_buf_sel ? 1 : 0, i.i_vbank1_port_sel);
        fprintf(f, "%x %x %02x ", i.i_vbank1_en_0 ? 1 : 0, i.i_vbank1_wen_0 ? 1 : 0, i.i_vbank1_addr_0); hex_v(f, i.i_vbank1_wmask_0); fprintf(f, " "); hex_v(f, i.i_vbank1_wdata_0);
        fprintf(f, " %x %x %02x ", i.i_vbank1_en_1 ? 1 : 0, i.i_vbank1_wen_1 ? 1 : 0, i.i_vbank1_addr_1); hex_v(f, i.i_vbank1_wmask_1); fprintf(f, " "); hex_v(f, i.i_vbank1_wdata_1);
        fprintf(f, " %x %x %02x ", i.i_vbank1_en_2 ? 1 : 0, i.i_vbank1_wen_2 ? 1 : 0, i.i_vbank1_addr_2); hex_v(f, i.i_vbank1_wmask_2); fprintf(f, " "); hex_v(f, i.i_vbank1_wdata_2);
        fprintf(f, " %x %x ", i.i_vbank2_buf_sel ? 1 : 0, i.i_vbank2_port_sel ? 1 : 0);
        fprintf(f, "%x %x %03x ", i.i_vbank2_en_0 ? 1 : 0, i.i_vbank2_wen_0 ? 1 : 0, i.i_vbank2_addr_0); hex_v(f, i.i_vbank2_wmask_0); fprintf(f, " "); hex_v(f, i.i_vbank2_wdata_0);
        fprintf(f, " %x %x %03x ", i.i_vbank2_en_1 ? 1 : 0, i.i_vbank2_wen_1 ? 1 : 0, i.i_vbank2_addr_1); hex_v(f, i.i_vbank2_wmask_1); fprintf(f, " "); hex_v(f, i.i_vbank2_wdata_1);
        fprintf(f, " %08x %02x ", o.o_sbank_rdata_0, o.o_sbank_rdata_1);
        hex_v(f, o.o_vbank0_rdata); fprintf(f, " "); hex_v(f, o.o_vbank1_rdata_0); fprintf(f, " "); hex_v(f, o.o_vbank1_rdata_1); fprintf(f, " "); hex_v(f, o.o_vbank1_rdata_2);
        fprintf(f, " "); hex_v(f, o.o_vbank2_rdata_0); fprintf(f, " "); hex_v(f, o.o_vbank2_rdata_1); fprintf(f, "\n");
        dut.tick(i);
    }
    fclose(f);
}

static void dump_vlsu(const std::string &dir)
{
    const ScratchpadParams P;
    GvuVlsu dut(P);
    FILE *f = fopen((dir + "/vlsub.txt").c_str(), "w");
    GvuVlsu::In i;
    int burst[5] = {0, 0, 0, 0, 0};
    int *mode[5] = {&i.i_sbank_agu_mode_0, &i.i_sbank_agu_mode_1, &i.i_vbank0_agu_mode, &i.i_vbank1_agu_mode, &i.i_vbank2_agu_mode};
    for (int cyc = 0; cyc < 40000; cyc++)
    {
        // requests come in bursts; the first cycle has none so that every address register is loaded
        for (int k = 0; k < 5; k++)
        {
            if (one_in(25)) *mode[k] = static_cast<int>(g_rng() % 4);    // the address mode may change inside a burst
            if (burst[k] > 0) { burst[k]--; continue; }
            // per burst: mostly AGU_INCR, then AGU_FIXED, AGU_WRAPPED and the unused fourth code
            { const unsigned r = static_cast<unsigned>(g_rng() % 20); *mode[k] = r < 12 ? AGU_INCR : (r < 16 ? AGU_FIXED : (r < 19 ? AGU_WRAPPED : 3)); }
            const bool on = cyc > 0 && one_in(3);
            const bool rd = on && (g_rng() & 1), wr = on && !rd;
            burst[k] = on ? static_cast<int>(g_rng() % 12) : static_cast<int>(g_rng() % 3);
            if (k == 0) { i.i_sbank_rd_0 = rd; i.i_sbank_wr_0 = wr; }
            if (k == 1) { i.i_sbank_rd_1 = rd; i.i_sbank_wr_1 = wr; }
            if (k == 2) { i.i_vbank0_rd = rd; i.i_vbank0_wr = wr; }
            if (k == 3) { i.i_vbank1_rd = rd; i.i_vbank1_wr = wr; i.i_vbank1_port_sel = static_cast<unsigned>(g_rng() % 4); }
            if (k == 4) { i.i_vbank2_rd = rd; i.i_vbank2_wr = wr; i.i_vbank2_port_sel = g_rng() & 1; }
        }
        if (one_in(30)) { i.i_sbank_rd_0 = i.i_sbank_wr_0 = true; }      // both requests at once
        i.i_agu_addr_reset = cyc > 0 && one_in(20);                      // written as bit 1 of the second field of the line
        // start addresses near the end of the range as well, so that the counters wrap
        auto sa = [&](int aw) { const unsigned m = (1u << aw) - 1u; return one_in(4) ? (m - static_cast<unsigned>(g_rng() % 4)) : static_cast<unsigned>(g_rng()) & m; };
        i.i_sbank_start_addr_0 = sa(P.SBANK_AW()); i.i_sbank_wmask_0 = static_cast<unsigned>(g_rng() & 0xF);
        i.i_sbank_start_addr_1 = sa(P.SBANK_AW() + 2); i.i_sbank_wmask_1 = g_rng() & 1;
        i.i_vbank0_start_addr = sa(P.VBANK0_AW()); i.i_vbank0_lane_wmask = one_in(3) ? 0xFFFFFFFFu : static_cast<uint32_t>(g_rng());
        i.i_vbank1_start_addr = sa(P.VBANK1_AW() + 2);
        i.i_vbank1_lane_wmask = one_in(3) ? 0xFFFFFFFFu : static_cast<uint32_t>(g_rng());
        i.i_vbank2_start_addr = sa(P.VBANK2_AW() + 2);
        i.i_vbank2_lane_wmask = one_in(3) ? 0xFFFFFFFFu : static_cast<uint32_t>(g_rng());
        const GvuVlsu::Out o = dut.eval(i);
        fprintf(f, "%x %x %x %03x %x %x %x %x %03x %x %x %x %x %03x %08x ", i.i_sbank_rd_0 ? 1 : 0, (i.i_sbank_wr_0 ? 1 : 0) | (i.i_agu_addr_reset ? 2 : 0), i.i_sbank_agu_mode_0, i.i_sbank_start_addr_0, i.i_sbank_wmask_0,
                i.i_sbank_rd_1 ? 1 : 0, i.i_sbank_wr_1 ? 1 : 0, i.i_sbank_agu_mode_1, i.i_sbank_start_addr_1, i.i_sbank_wmask_1 ? 1 : 0,
                i.i_vbank0_rd ? 1 : 0, i.i_vbank0_wr ? 1 : 0, i.i_vbank0_agu_mode, i.i_vbank0_start_addr, i.i_vbank0_lane_wmask);
        fprintf(f, "%x %x %x %x %02x %08x %x %x %x %x %03x %08x ", i.i_vbank1_port_sel, i.i_vbank1_rd ? 1 : 0, i.i_vbank1_wr ? 1 : 0, i.i_vbank1_agu_mode, i.i_vbank1_start_addr, i.i_vbank1_lane_wmask,
                i.i_vbank2_port_sel ? 1 : 0, i.i_vbank2_rd ? 1 : 0, i.i_vbank2_wr ? 1 : 0, i.i_vbank2_agu_mode, i.i_vbank2_start_addr, i.i_vbank2_lane_wmask);
        fprintf(f, "%x %x %03x %x %x %x %03x %x %x %x %03x %08x ", o.o_sbank_en_0 ? 1 : 0, o.o_sbank_wen_0 ? 1 : 0, o.o_sbank_addr_0, o.o_sbank_wmask_0,
                o.o_sbank_en_1 ? 1 : 0, o.o_sbank_wen_1 ? 1 : 0, o.o_sbank_addr_1, o.o_sbank_wmask_1 ? 1 : 0, o.o_vbank0_en ? 1 : 0, o.o_vbank0_wen ? 1 : 0, o.o_vbank0_addr, o.o_vbank0_wmask);
        fprintf(f, "%x %x %x %02x ", o.o_vbank1_port_sel, o.o_vbank1_en_0 ? 1 : 0, o.o_vbank1_wen_0 ? 1 : 0, o.o_vbank1_addr_0); hex_v(f, o.o_vbank1_wmask_0);
        fprintf(f, " %x %x %02x ", o.o_vbank1_en_1 ? 1 : 0, o.o_vbank1_wen_1 ? 1 : 0, o.o_vbank1_addr_1); hex_v(f, o.o_vbank1_wmask_1);
        fprintf(f, " %x %x %02x %08x ", o.o_vbank1_en_2 ? 1 : 0, o.o_vbank1_wen_2 ? 1 : 0, o.o_vbank1_addr_2, o.o_vbank1_wmask_2);
        fprintf(f, "%x %x %x %03x ", o.o_vbank2_port_sel ? 1 : 0, o.o_vbank2_en_0 ? 1 : 0, o.o_vbank2_wen_0 ? 1 : 0, o.o_vbank2_addr_0); hex_v(f, o.o_vbank2_wmask_0);
        fprintf(f, " %x %x %03x %08x\n", o.o_vbank2_en_1 ? 1 : 0, o.o_vbank2_wen_1 ? 1 : 0, o.o_vbank2_addr_1, o.o_vbank2_wmask_1);
        dut.tick(i);
    }
    fclose(f);
}

// gvu_lut_unit with 32 lanes: every lane has its own input (zero inputs in some lanes only), the handshake outputs are
// those of lane 0.
static void dump_lut32(const std::string &dir)
{
    LutUnit dut;
    std::vector<uint8_t> act(256), ex(256);
    std::vector<uint16_t> recip(256), rsqrt(512);
    for (unsigned k = 0; k < 256; k++) { act[k] = static_cast<uint8_t>(255 - k); ex[k] = static_cast<uint8_t>(k * 7 + 3); }
    for (unsigned k = 0; k < 512; k++)
    {
        const unsigned p = k >> 8, j = k & 255;
        rsqrt[k] = static_cast<uint16_t>(std::lround(std::ldexp(1.0 / std::sqrt((1.0 + j / 256.0) * (1 << p)), 14)));
        if (k < 256) recip[k] = static_cast<uint16_t>(std::lround(std::ldexp(1.0 / (1.0 + j / 256.0), 14)) + ((j % 37 == 5) ? 300 : 0));
    }
    dut.mem.load8(LUT_ACT, act); dut.mem.load8(LUT_EXP, ex); dut.mem.load16(LUT_RECIP, recip); dut.mem.load16(LUT_RSQRT, rsqrt);
    FILE *f = fopen((dir + "/lut32.txt").c_str(), "w");
    const struct { LutType sel; LutIdxMode mode; int zeros; bool compens; unsigned base; } seg[] = {
        {LUT_RECIP, LUT_PWL, 0, false, 7}, {LUT_RSQRT, LUT_PWL, 3, false, 7}, {LUT_RECIP, LUT_LOG_SCALE, 2, false, 5}, {LUT_EXP, LUT_PWL, 0, false, 0}, {LUT_ACT, LUT_PWL, 2, true, 0},
    };
    bool v = false;
    LutUnit::In in;
    for (const auto &s : seg)
        for (int cyc = 0; cyc < 12000; cyc++)
        {
            const bool drain = cyc >= 11980;
            if (!v)
            {
                v = !drain && (g_rng() % 4) != 0;
                const bool same = one_in(3); const uint32_t x0 = rnd_x();
                for (int l = 0; l < GVU_LANES; l++)
                {
                    in.i_32b_lookup_idx[static_cast<size_t>(l)] = (s.zeros && static_cast<int>(g_rng() % 16) < s.zeros) ? 0u : (same ? x0 : rnd_x());
                    in.i_8b_lookup_idx[static_cast<size_t>(l)] = static_cast<uint32_t>(g_rng() & 0xFF);
                }
            }
            if (drain) v = false;
            in.i_lookup_valid = v; in.i_lut_rsel = s.sel; in.i_lut_idx_mode = s.mode; in.i_lut_compens = s.compens; in.i_base = s.base; in.i_W = 16; in.i_lerp_shift = 8;
            in.i_lookup_ready = drain || !one_in(4);
            const LutUnit::Out o = dut.eval(in);
            fprintf(f, "%x %x %x %x %02x %x ", v ? 1 : 0, s.sel, s.mode, s.compens ? 1 : 0, s.base, in.i_lookup_ready ? 1 : 0);
            for (int l = GVU_LANES - 1; l >= 0; l--) fprintf(f, "%02x", in.i_8b_lookup_idx[static_cast<size_t>(l)]);
            fprintf(f, " ");
            for (int l = GVU_LANES - 1; l >= 0; l--) fprintf(f, "%08x", in.i_32b_lookup_idx[static_cast<size_t>(l)]);
            fprintf(f, " %x %x %x ", o.o_lookup_ready ? 1 : 0, o.o_lookup_valid ? 1 : 0, o.o_pipe_almost_done ? 1 : 0);
            for (int l = GVU_LANES - 1; l >= 0; l--) fprintf(f, "%02x", o.o_8b_lookup_data[static_cast<size_t>(l)]);
            fprintf(f, " ");
            for (int l = GVU_LANES - 1; l >= 0; l--) fprintf(f, "%04x", o.o_16b_lookup_data[static_cast<size_t>(l)]);
            fprintf(f, " ");
            for (int l = GVU_LANES - 1; l >= 0; l--) fprintf(f, "%02x", o.o_e_shift[static_cast<size_t>(l)]);
            fprintf(f, " %08x\n", 0u);                               // column kept: the unit has no zero-flag output
            dut.tick(in);
            if (v && o.o_lookup_ready) v = false;
        }
    fclose(f);
}

// Table-load sequence of gvu_top.sv (has/rtl_ref_gvu_ctrl.h): vbank1 rows, then one line per cycle until the first softmax
// state, then the four tables.
static void dump_lutload(const std::string &dir)
{
    LutInitSequence dut;
    FILE *fv = fopen((dir + "/lutload_v1.txt").c_str(), "w");
    for (unsigned a = 0; a < 32; a++) { const SpBytes row = rnd_bytes(128); dut.scratchpad.sram(1, 0).poke(a, row); hex_v(fv, row); fprintf(fv, "\n"); }
    fclose(fv);
    FILE *f = fopen((dir + "/lutload.txt").c_str(), "w");
    for (int cyc = 0; cyc < 200; cyc++)
    {
        const LutInitSequence::Out o = dut.eval();
        fprintf(f, "%x %x %x %x %03x %x %x %02x %x\n", o.state, o.o_lut_init_wr ? 1 : 0, o.o_lut_init_sel, o.i_lut_init_done ? 1 : 0, o.lut_waddr_r, o.wr_cnt_r,
                o.o_vbank1_en_0 ? 1 : 0, o.o_vbank1_addr_0, o.o_softmax_lut_init_done ? 1 : 0);
        if (dut.done()) break;
        dut.tick();
    }
    fclose(f);
    dump_tables(dir + "/lutload_mem.txt", dut.mem);
}

int main(int argc, char **argv)
{
    if (argc < 2) { printf("usage: %s <out dir> [bytemask]\n", argv[0]); return 2; }
    // "bytemask": only spadb.txt, from the Scratchpad port with one mask bit per byte (what the comments of the memory
    // wrappers describe) instead of the mask as the RTL is written; the replay on the RTL then shows the difference.
    if (argc > 2 && std::string(argv[2]) == "bytemask") { dump_scratchpad(argv[1], false); printf("RESULT: DUMP DONE (byte mask)\n"); return 0; }
    dump_lut(argv[1]);
    dump_cmp(argv[1], false);
    dump_cmp(argv[1], true);
    dump_svrf(argv[1]);
    dump_scratchpad(argv[1], false);
    dump_vlsu(argv[1]);
    dump_lut32(argv[1]);
    dump_lutload(argv[1]);
    printf("RESULT: DUMP DONE\n");
    return 0;
}
