// tb_rtl_ref_gvu_bdump_r2.cpp -- the LUT part of tb_rtl_ref_gvu_bdump.cpp for the revision 2 port (has/rtl_ref_gvu_lut_r2.h): writes cycle-by-cycle
// stimulus and the outputs of the ported LUT unit for replay on the RTL revision 2 by tools/has/gvu_oracle/tb_oracle_b.sv (built with +define+NEO_REV2).
// Same files and formats as tb_rtl_ref_gvu_bdump.cpp (lutb_tab.txt, lutb.txt, lutb_wr.txt, lutb_mem.txt, lut32.txt); in lutb_wr.txt the write address
// column is 0 (revision 2 has no write address register) and o_init_done includes the write enable.
// build: g++ -std=c++17 -O2 -Wall -I. tools/has/tb_rtl_ref_gvu_bdump_r2.cpp -o tb_rtl_ref_gvu_bdump_r2 ; run: <binary> <out dir>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include "has/rtl_ref_gvu_lut_r2.h"

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

static void dump_tables(const std::string &path, const LutMemR2 &mem)
{
    FILE *f = fopen(path.c_str(), "w");
    for (unsigned k = 0; k < 512; k++) fprintf(f, "%02x %02x %04x %04x\n", mem.peek8(LUT_ACT, k), mem.peek8(LUT_EXP, k), mem.peek16(LUT_RECIP, k), mem.peek16(LUT_RSQRT, k));
    fclose(f);
}

static void fill_tables(LutMemR2 &mem)
{
    std::vector<uint8_t> act(256), ex(256);
    std::vector<uint16_t> recip(256), rsqrt(512);
    for (unsigned k = 0; k < 256; k++) { act[k] = static_cast<uint8_t>(255 - k); ex[k] = static_cast<uint8_t>(k * 7 + 3); }
    for (unsigned k = 0; k < 512; k++)
    {
        const unsigned p = k >> 8, j = k & 255;
        rsqrt[k] = static_cast<uint16_t>(std::lround(std::ldexp(1.0 / std::sqrt((1.0 + j / 256.0) * (1 << p)), 14)));
        if (k < 256) recip[k] = static_cast<uint16_t>(std::lround(std::ldexp(1.0 / (1.0 + j / 256.0), 14)) + ((j % 37 == 5) ? 300 : 0));
    }
    mem.load8(LUT_ACT, act); mem.load8(LUT_EXP, ex); mem.load16(LUT_RECIP, recip); mem.load16(LUT_RSQRT, rsqrt);
}

// One lane (gvu_lut_unit with NUM_LANE = 1), random valid / ready, segments of one table and one index mode.
static void dump_lut(const std::string &dir)
{
    LutUnitT<1> u;
    fill_tables(u.mem);
    dump_tables(dir + "/lutb_tab.txt", u.mem);
    FILE *f = fopen((dir + "/lutb.txt").c_str(), "w");
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
            LutUnitT<1>::In in;
            in.i_lookup_valid = v; in.i_lut_rsel = s.sel; in.i_lut_idx_mode = s.mode; in.i_lut_compens = s.compens; in.i_base = s.base; in.i_W = s.W; in.i_lerp_shift = s.sh;
            in.i_8b_lookup_idx[0] = static_cast<uint32_t>(g_rng() & 0xFF); in.i_32b_lookup_idx[0] = x;
            in.i_lookup_ready = drain || static_cast<int>(g_rng() % 4) < s.ready4;
            const LutUnitT<1>::Out o = u.eval(in);
            fprintf(f, "%x %x %x %x %02x %02x %02x %02x %08x %x %x %x %x %02x %04x %02x %x\n", v ? 1 : 0, s.sel, s.mode, s.compens ? 1 : 0, s.base, s.W, s.sh, in.i_8b_lookup_idx[0], x,
                    in.i_lookup_ready ? 1 : 0, o.o_lookup_ready ? 1 : 0, o.o_lookup_valid ? 1 : 0, 0 /* column kept: the unit has no zero-flag output */, o.o_8b_lookup_data[0], o.o_16b_lookup_data[0], o.o_e_shift[0],
                    o.o_pipe_almost_done ? 1 : 0);
            u.tick(in);
            if (v && o.o_lookup_ready) v = false;
        }
    fclose(f);

    // table load through the write port: runs of one table mixed with table changes mid-run (INIT_DW = 128, 16 bytes per write)
    FILE *fw = fopen((dir + "/lutb_wr.txt").c_str(), "w");
    LutType wsel = LUT_ACT;
    for (int cyc = 0; cyc < 6000; cyc++)
    {
        if (cyc < 3000 ? one_in(40) : one_in(5)) wsel = static_cast<LutType>(g_rng() % 4);
        const bool wen = cyc < 3000 ? !one_in(6) : (g_rng() & 1);
        const std::vector<uint8_t> d = rnd_bytes(16);
        fprintf(fw, "%x %x ", wen ? 1 : 0, wsel); hex_v(fw, d);
        fprintf(fw, " %x %03x %x\n", u.mem.o_init_done(wsel, wen) ? 1 : 0, 0u, u.mem.wr_cnt());
        LutUnitT<1>::In in; in.i_lut_wen = wen; in.i_lut_wsel = wsel; for (size_t i = 0; i < 16; i++) in.i_lut_wdata[i] = d[i];
        in.i_lookup_valid = false;
        u.tick(in);
    }
    fclose(fw);
    dump_tables(dir + "/lutb_mem.txt", u.mem);
}

// 32 lanes with the default parameters: inputs of every lane | handshake of lane 0 and the outputs of every lane.
static void dump_lut32(const std::string &dir)
{
    LutUnitT<32> dut;
    fill_tables(dut.mem);
    FILE *f = fopen((dir + "/lut32.txt").c_str(), "w");
    const struct { LutType sel; LutIdxMode mode; int zeros; bool compens; unsigned base; } seg[] = {
        {LUT_RECIP, LUT_PWL, 0, false, 7}, {LUT_RSQRT, LUT_PWL, 3, false, 7}, {LUT_RECIP, LUT_LOG_SCALE, 2, false, 5}, {LUT_EXP, LUT_PWL, 0, false, 0}, {LUT_ACT, LUT_PWL, 2, true, 0},
    };
    bool v = false;
    LutUnitT<32>::In in;
    for (const auto &s : seg)
        for (int cyc = 0; cyc < 12000; cyc++)
        {
            const bool drain = cyc >= 11980;
            if (!v)
            {
                v = !drain && (g_rng() % 4) != 0;
                const bool same = one_in(3); const uint32_t x0 = rnd_x();
                for (int l = 0; l < 32; l++)
                {
                    in.i_32b_lookup_idx[static_cast<size_t>(l)] = (s.zeros && static_cast<int>(g_rng() % 16) < s.zeros) ? 0u : (same ? x0 : rnd_x());
                    in.i_8b_lookup_idx[static_cast<size_t>(l)] = static_cast<uint32_t>(g_rng() & 0xFF);
                }
            }
            if (drain) v = false;
            in.i_lookup_valid = v; in.i_lut_rsel = s.sel; in.i_lut_idx_mode = s.mode; in.i_lut_compens = s.compens; in.i_base = s.base; in.i_W = 16; in.i_lerp_shift = 8;
            in.i_lookup_ready = drain || !one_in(4);
            const LutUnitT<32>::Out o = dut.eval(in);
            fprintf(f, "%x %x %x %x %02x %x ", v ? 1 : 0, s.sel, s.mode, s.compens ? 1 : 0, s.base, in.i_lookup_ready ? 1 : 0);
            for (int l = 31; l >= 0; l--) fprintf(f, "%02x", in.i_8b_lookup_idx[static_cast<size_t>(l)]);
            fprintf(f, " ");
            for (int l = 31; l >= 0; l--) fprintf(f, "%08x", in.i_32b_lookup_idx[static_cast<size_t>(l)]);
            fprintf(f, " %x %x %x ", o.o_lookup_ready ? 1 : 0, o.o_lookup_valid ? 1 : 0, o.o_pipe_almost_done ? 1 : 0);
            for (int l = 31; l >= 0; l--) fprintf(f, "%02x", o.o_8b_lookup_data[static_cast<size_t>(l)]);
            fprintf(f, " ");
            for (int l = 31; l >= 0; l--) fprintf(f, "%04x", o.o_16b_lookup_data[static_cast<size_t>(l)]);
            fprintf(f, " ");
            for (int l = 31; l >= 0; l--) fprintf(f, "%02x", o.o_e_shift[static_cast<size_t>(l)]);
            fprintf(f, " %08x\n", 0u);                               // column kept: the unit has no zero-flag output
            dut.tick(in);
            if (v && o.o_lookup_ready) v = false;
        }
    fclose(f);
}

int main(int argc, char **argv)
{
    if (argc < 2) { printf("usage: %s <out dir>\n", argv[0]); return 2; }
    dump_lut(argv[1]);
    dump_lut32(argv[1]);
    printf("RESULT: DUMP DONE\n");
    return 0;
}
