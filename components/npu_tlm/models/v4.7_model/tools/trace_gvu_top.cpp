// trace_gvu_top.cpp -- detailed cycle trace of rtl_ref_gvu::GvuTop (debug tool, not part of the regression).
// Same reset / stimulus sequence as tb_gvu_top.cpp (softmax), tb_gvu_top_gemm_fused.cpp (gemm_fused), and
// tb_gvu_top_residual_add.cpp (residual), but logs every cycle everything GvuTop makes observable through its public interface:
//   - Out ports (o_psum_ready, *_lut_init_done, *_pipe_done, o_gvu_pipe_ctrl_state)
//   - controller beat counters start_d_cnt / end_d_cnt
//   - LUT memory write pointer (lut_waddr, wr_cnt) and table contents at stage boundaries
//   - Stream VRF plain registers vint64[1], vint32[1..3] (valid + lane data)
//   - every Scratchpad SRAM row (accelerator buffer 0 of vbank0/1/2 and sbank) whose contents change in the cycle
// Internal wires (VLSU strobes, FU outputs) are private to GvuTop and are NOT logged.
//
// Build (from model_vp):  g++ -std=c++17 -O2 -Wall -I. tools/trace_gvu_top.cpp -o trace_gvu_top
// Usage:                  ./trace_gvu_top softmax|gemm_fused|residual [--preload] [--byte-mask]
//   --preload    fills the Scratchpad with a deterministic pattern before the run so data movement becomes visible
//                (the official testbenches run with all-zero memory, where writes of zero cannot be seen as changes).
//   --byte-mask  ScratchpadParams::wmask_as_written = false (one mask bit per byte) instead of the RTL-as-written mask,
//                where mask bit i writes bits [i, i + 8) so only the low DW/8 + 7 bits of a word can change.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>
#include "has/rtl_ref_gvu_top.h"

using namespace rtl_ref_gvu;

static const char *st_name(int s)
{
    static const char *n[] = {"IDLE", "LUT_ACT_INIT", "LUT_EXP_INIT", "LUT_RECIP_INIT", "LUT_RSQRT_INIT",
                              "SOFTMAX_ST1", "SOFTMAX_ST2", "SOFTMAX_ST3", "LAYERNORM_ST1", "LAYERNORM_ST2",
                              "LAYERNORM_ST3", "LAYERNORM_ST4", "LAYERNORM_ST5", "GEMM_FUSED_ST1",
                              "RESIDUAL_ST1", "RESIDUAL_ST2"};
    return (s >= 0 && s <= 15) ? n[s] : "UNKNOWN";
}
static const char *bank_name(int b) { return b == 0 ? "vbank0" : b == 1 ? "vbank1" : b == 2 ? "vbank2" : "sbank"; }

static int32_t rd32(const SpBytes &d, size_t lane)
{
    uint32_t v = 0;
    for (size_t b = 0; b < 4; b++) if (4 * lane + b < d.size()) v |= static_cast<uint32_t>(d[4 * lane + b]) << (8 * b);
    return static_cast<int32_t>(v);
}

// Prints a row: vbank0 as 32 x int8, vbank1/vbank2 as 32 x int32 (port-0 view of the 1024-bit row), sbank as int32.
static void print_row(int bank, unsigned addr, const SpBytes &d, const char *prefix)
{
    printf("%s%s[%3u]:", prefix, bank_name(bank), addr);
    if (bank == 0)
    {
        printf(" i8 ");
        for (size_t l = 0; l < d.size(); l++) printf("%4d", static_cast<int8_t>(d[l]));
    }
    else if (bank == 3)
    {
        printf(" i32 %d (bytes %02x %02x %02x %02x)", rd32(d, 0), d[0], d[1], d[2], d[3]);
    }
    else
    {
        printf(" i32");
        for (size_t l = 0; l < d.size() / 4; l++) printf(" %d", rd32(d, l));
    }
    printf("\n");
}

struct MemSnap
{
    std::vector<SpBytes> rows[4];
};

static int depth(const ScratchpadParams &p, int b)
{
    return b == 0 ? p.VBANK0_DEPTH : b == 1 ? p.VBANK1_DEPTH : b == 2 ? p.VBANK2_DEPTH : p.SBANK_DEPTH;
}

static MemSnap snap(GvuTop &dut)
{
    MemSnap s;
    const ScratchpadParams &p = dut.scratchpad().params();
    for (int b = 0; b < 4; b++)
        for (int a = 0; a < depth(p, b); a++) s.rows[b].push_back(dut.scratchpad().sram(b, 0).word(static_cast<unsigned>(a)));
    return s;
}

static void preload(GvuTop &dut, unsigned H, bool is_res = false)
{
    // vbank0 rows 0..H-1 (one row per beat; lane l is element `row` of vector l):
    // x = ((row*32 + lane) * 7 % 64) - 32, int8 in [-32, 31]
    for (unsigned r = 0; r < H; r++)
    {
        SpBytes d(32);
        for (unsigned l = 0; l < 32; l++) d[l] = static_cast<uint8_t>(static_cast<int8_t>(((r * 32 + l) * 7) % 64 - 32));
        dut.scratchpad().sram(0, 0).poke(r, d);
    }
    if (is_res)
    {
        const unsigned vbank1_lines = (H + 3) / 4;
        for (unsigned line = 0; line < vbank1_lines; line++)
        {
            SpBytes d(128);
            for (unsigned q = 0; q < 4; q++)
            {
                unsigned r = line * 4 + q;
                if (r < H)
                    for (unsigned l = 0; l < 32; l++)
                        d[q * 32 + l] = static_cast<uint8_t>(static_cast<int8_t>(((r * 31 + l) * 5) % 64 - 32));
            }
            dut.scratchpad().sram(1, 0).poke(line, d);
        }
    }
    else
    {
        // vbank1 rows 0..7 (LUT init source): decreasing byte ramp 255, 254, ...
        for (unsigned r = 0; r < 8; r++)
        {
            SpBytes d(128);
            for (unsigned b = 0; b < 128; b++) d[b] = static_cast<uint8_t>(255 - ((r * 128 + b) & 0xFF));
            dut.scratchpad().sram(1, 0).poke(r, d);
        }
    }
    // sbank: every word = 1 (beta = 1 on port 0; gamma byte = 1 / 0 on port 1)
    for (unsigned a = 0; a < 128; a++) dut.scratchpad().sram(3, 0).poke(a, SpBytes{1, 0, 0, 0});
}

static void dump_luts(GvuTop &dut)
{
    const LutUnitSel &m = dut.vlut();   // peek8 / peek16 of the table memory in use
    printf("    LUT_act  [0..15]:");
    for (unsigned i = 0; i < 16; i++) printf(" %u", m.peek8(LUT_ACT, i));
    printf("\n    LUT_exp  [0..15]:");
    for (unsigned i = 0; i < 16; i++) printf(" %u", m.peek8(LUT_EXP, i));
    printf("\n    LUT_recip[0..15]:");
    for (unsigned i = 0; i < 16; i++) printf(" %u", m.peek16(LUT_RECIP, i));
    printf("\n    LUT_rsqrt[0..15]:");
    for (unsigned i = 0; i < 16; i++) printf(" %u", m.peek16(LUT_RSQRT, i));
    unsigned nz_act = 0, nz_exp = 0, nz_recip = 0, nz_rsqrt = 0;
    for (unsigned i = 0; i < 256; i++) { nz_act += m.peek8(LUT_ACT, i) != 0; nz_exp += m.peek8(LUT_EXP, i) != 0; nz_recip += m.peek16(LUT_RECIP, i) != 0; }
    for (unsigned i = 0; i < 512; i++) nz_rsqrt += m.peek16(LUT_RSQRT, i) != 0;
    printf("\n    non-zero entries: LUT_act %u/256, LUT_exp %u/256, LUT_recip %u/256, LUT_rsqrt %u/512\n", nz_act, nz_exp, nz_recip, nz_rsqrt);
}

static void dump_vrf(GvuTop &dut)
{
    const StreamVrf &v = dut.stream_vrf();
    printf("    vint64[1] valid=%d lane0..3 (i64):", v.int64_1.valid);
    for (size_t l = 0; l < 4; l++)
    {
        int64_t x = 0;
        for (size_t b = 0; b < 8; b++) x |= static_cast<int64_t>(v.int64_1.data[8 * l + b]) << (8 * b);
        printf(" %lld", static_cast<long long>(x));
    }
    printf("\n");
    for (size_t k = 0; k < 3; k++)
    {
        printf("    vint32[%zu] valid=%d lane0..7 (i32):", k + 1, v.int32_1_3[k].valid);
        for (size_t l = 0; l < 8; l++) printf(" %d", rd32(v.int32_1_3[k].data, l));
        printf("\n");
    }
}

int main(int argc, char **argv)
{
    if (argc < 2 || (strcmp(argv[1], "softmax") && strcmp(argv[1], "gemm_fused") && strcmp(argv[1], "residual")))
    {
        fprintf(stderr, "usage: %s softmax|gemm_fused|residual [--preload] [--byte-mask]\n", argv[0]);
        return 2;
    }
    const bool is_ln = false;
    const bool is_gemm = !strcmp(argv[1], "gemm_fused");
    const bool is_res = !strcmp(argv[1], "residual");
    bool do_preload = false, byte_mask = false;
    for (int a = 2; a < argc; a++)
    {
        if (!strcmp(argv[a], "--preload")) do_preload = true;
        else if (!strcmp(argv[a], "--byte-mask")) byte_mask = true;
        else { fprintf(stderr, "unknown option %s\n", argv[a]); return 2; }
    }
    const unsigned H = 128, MaxCycle = 10000;
    const char *tag = is_res ? "trace_residual" : (is_gemm ? "trace_gemm_fused" : "trace_softmax");

    GvuTopParams params;
    params.sp_params.wmask_as_written = !byte_mask;
    GvuTop dut(params);
    GvuTop::In in;
    printf("[%s] op=%s token_len=%u preload=%s\n", tag,
           is_res ? "GVU_OP_RESIDUAL" : (is_gemm ? "GVU_OP_GEMM_FUSED" : (is_ln ? "GVU_OP_LAYERNORM" : "GVU_OP_SOFTMAX")), H,
           do_preload ? "yes" : "no (same as official test: all-zero Scratchpad)");
    printf("[%s] Scratchpad write mask: %s\n", tag,
           byte_mask ? "one bit per byte (--byte-mask)" : "RTL as written (mask bit i writes bits [i, i+8); default of the model)");
    const ScratchpadParams &sp = dut.scratchpad().params();
    printf("[%s] Scratchpad depths: vbank0=%d x 32B, vbank1=%d x 128B, vbank2=%d x 128B, sbank=%d x 4B; read latency vbank=%d cycles\n",
           tag, sp.VBANK0_DEPTH, sp.VBANK1_DEPTH, sp.VBANK2_DEPTH, sp.SBANK_DEPTH, sp.read_latency(0));

    for (int c = 0; c < 5; c++) dut.tick(in, false); // reset cycles 1..5
    dut.tick(in, true);                              // idle cycle 6
    if (do_preload) preload(dut, H, is_res);

    in.i_token_len = H;
    in.i_gvu_op = is_res ? GVU_OP_RESIDUAL : (is_gemm ? GVU_OP_GEMM_FUSED : (is_ln ? GVU_OP_LAYERNORM : GVU_OP_SOFTMAX));
    in.i_attn_z = 0;
    in.i_attn_scale = (is_ln || is_gemm || is_res) ? 0 : 35;
    in.i_attn_shift = (is_ln || is_gemm || is_res) ? 0 : 8;
    if (is_gemm)
    {
        in.i_gemm_scale = 3;
        in.i_gemm_shift = 1;
        in.i_gemm_zp = -2;
        in.i_gemm_per_channel = false;
        in.i_gemm_skip_act = false;
        in.i_gemm_skip_zp = false;
        in.i_gemm_out_int16 = false;
    }
    if (is_res)
    {
        in.i_res_scale_a = 0x40000000;
        in.i_res_shift_a = 30;
        in.i_res_zp_a = -5;
        in.i_res_scale_b = 0x40000000;
        in.i_res_shift_b = 30;
        in.i_res_zp_b = 3;
        in.i_res_scale_out = 0x40000000;
        in.i_res_shift_out = 30;
        in.i_res_zp_out = -2;
        in.i_res_round = RND_NEAR_UP;
        in.i_res_out_int16 = false;
        in.i_res_a_row = 0;
        in.i_res_b_row = 0;
        in.i_res_sum_row = 0;
    }
    in.i_pipeline_en = true;
    printf("[%s] stimulus at cycle 6: i_pipeline_en=1 attn_scale=%d attn_shift=%u attn_z=%d", tag, in.i_attn_scale,
           static_cast<unsigned>(in.i_attn_shift), static_cast<int>(in.i_attn_z));
    if (is_ln)
        printf("\n[%s] ln params: h=%d pre_shift=%d m0_var=%lld ts_var=%d z_var=%d m0_7=%lld ts_7=%d e_bias=%lld z_7=%d m0_div=%lld ts_div=%d z_div=%d m0_mul=%lld ts_mul=%d z_mul=%d z_out=%d out_int16=%d",
               tag, in.i_ln_h, in.i_ln_pre_shift, (long long)in.i_ln_m0_var, in.i_ln_ts_var, in.i_ln_z_var, (long long)in.i_ln_m0_7, in.i_ln_ts_7,
               (long long)in.i_ln_e_bias, in.i_ln_z_7, (long long)in.i_ln_m0_div, in.i_ln_ts_div, in.i_ln_z_div, (long long)in.i_ln_m0_mul,
               in.i_ln_ts_mul, in.i_ln_z_mul, in.i_ln_z_out, in.i_ln_out_int16);
    if (is_gemm)
        printf("\n[%s] gemm params: scale=%d shift=%u zp=%d per_ch=%d skip_act=%d skip_zp=%d out_i16=%d",
               tag, in.i_gemm_scale, (unsigned)in.i_gemm_shift, in.i_gemm_zp, in.i_gemm_per_channel,
               in.i_gemm_skip_act, in.i_gemm_skip_zp, in.i_gemm_out_int16);
    if (is_res)
        printf("\n[%s] res params: scale_a=0x%x shift_a=%u zp_a=%d scale_b=0x%x shift_b=%u zp_b=%d scale_out=0x%x shift_out=%u zp_out=%d",
               tag, in.i_res_scale_a, (unsigned)in.i_res_shift_a, in.i_res_zp_a,
               in.i_res_scale_b, (unsigned)in.i_res_shift_b, in.i_res_zp_b,
               in.i_res_scale_out, (unsigned)in.i_res_shift_out, in.i_res_zp_out);
    printf("\n\n");

    printf("cycle |    ns  | state           | st_cnt end_cnt | rdy smxLUT smxDone lnLUT lnDone gDone | lutWA wc | v64_1 v32_1 v32_2 v32_3 | vint32[2].lane0 | SRAM rows changed\n");

    MemSnap prev = snap(dut), run_start = prev;
    int last_state = -1;
    uint64_t state_enter = 7, done_cycle = 0, post = 0;
    unsigned stage_writes[4] = {0, 0, 0, 0};
    unsigned psum_streamed = 0;
    bool done = false;

    for (uint64_t cyc = 7; cyc <= MaxCycle; cyc++)
    {
        if (is_gemm && last_state == GEMM_FUSED_ST1 && psum_streamed < H)
        {
            in.i_psum_valid = true;
            for (size_t l = 0; l < 32; l++)
                in.i_psum_vec[l] = static_cast<int32_t>((psum_streamed * 32 + l) % 100 - 50);
            psum_streamed++;
        }
        else if (is_gemm && psum_streamed >= H)
        {
            in.i_psum_valid = false;
        }

        const unsigned sd = dut.start_d_cnt(), ed = dut.end_d_cnt();
        const GvuTop::Out o = dut.tick(in, true);
        const int st = static_cast<int>(o.o_gvu_pipe_ctrl_state);

        if (st != last_state)
        {
            if (last_state >= 0)
            {
                printf("---- leave %s after %llu cycles; SRAM row writes in stage: vbank0=%u vbank1=%u vbank2=%u sbank=%u; LUT wr_cnt=%u\n",
                       st_name(last_state), (unsigned long long)(cyc - state_enter), stage_writes[0], stage_writes[1], stage_writes[2],
                       stage_writes[3], dut.vlut().wr_cnt());
                dump_vrf(dut);
                if (last_state >= 1 && last_state <= 4) dump_luts(dut);
            }
            printf("==== cycle %llu (%llu ns): enter %s\n", (unsigned long long)cyc, (unsigned long long)cyc * 10, st_name(st));
            last_state = st;
            state_enter = cyc;
            for (unsigned &w : stage_writes) w = 0;
        }

        // SRAM change detection (contents after this edge vs. before)
        MemSnap cur = snap(dut);
        std::string chg;
        std::vector<std::pair<int, unsigned>> changed;
        for (int b = 0; b < 4; b++)
            for (size_t a = 0; a < cur.rows[b].size(); a++)
                if (cur.rows[b][a] != prev.rows[b][a])
                {
                    changed.push_back({b, static_cast<unsigned>(a)});
                    stage_writes[b]++;
                    char buf[32];
                    snprintf(buf, sizeof buf, " %s[%zu]", bank_name(b), a);
                    chg += buf;
                }

        const StreamVrf &v = dut.stream_vrf();
        printf("%5llu | %6llu | %-15s | %6u %7u | %3d %6d %7d %5d %6d %5d | %5u %2u | %5d %5d %5d %5d | %15d |%s\n",
               (unsigned long long)cyc, (unsigned long long)cyc * 10, st_name(st), sd, ed, o.o_psum_ready, o.o_softmax_lut_init_done,
               o.o_softmax_pipe_done, o.o_layernorm_lut_init_done, o.o_layernorm_pipe_done, o.o_gemm_fused_pipe_done, dut.vlut().lut_waddr(),
               dut.vlut().wr_cnt(), v.int64_1.valid, v.int32_1_3[0].valid, v.int32_1_3[1].valid, v.int32_1_3[2].valid,
               rd32(v.int32_1_3[1].data, 0), chg.c_str());
        for (auto &bc : changed) print_row(bc.first, bc.second, cur.rows[bc.first][bc.second], "        new ");
        prev = std::move(cur);

        const bool pd = is_res ? o.o_residual_pipe_done : (is_gemm ? o.o_gemm_fused_pipe_done : (is_ln ? (o.o_layernorm_pipe_done || o.o_softmax_pipe_done) : o.o_softmax_pipe_done));
        if (pd && !done)
        {
            done = true;
            done_cycle = cyc;
            in.i_pipeline_en = false;
            in.i_psum_valid = false;
            printf("==== cycle %llu (%llu ns): %s asserted\n", (unsigned long long)cyc, (unsigned long long)cyc * 10,
                   is_res ? "o_residual_pipe_done" : (is_gemm ? "o_gemm_fused_pipe_done" : (is_ln ? "o_layernorm_pipe_done" : "o_softmax_pipe_done")));
        }
        if (done && ++post >= 15) break;
    }

    printf("\n================ SUMMARY (%s) ================\n", tag);
    if (!done)
    {
        printf("RESULT: FAIL (timeout after %u cycles)\n", MaxCycle);
        return 1;
    }
    printf("pipe_done at cycle %llu (%llu ns)\n", (unsigned long long)done_cycle, (unsigned long long)done_cycle * 10);
    printf("Final LUT state:\n");
    dump_luts(dut);
    printf("Final Stream VRF plain registers:\n");
    dump_vrf(dut);
    printf("Scratchpad rows that differ from their value at run start (final contents):\n");
    MemSnap fin = snap(dut);
    unsigned n = 0;
    for (int b = 0; b < 4; b++)
        for (size_t a = 0; a < fin.rows[b].size(); a++)
            if (fin.rows[b][a] != run_start.rows[b][a]) { print_row(b, static_cast<unsigned>(a), fin.rows[b][a], "    "); n++; }
    if (n == 0) printf("    (none -- every write stored the value already present)\n");
    printf("RESULT: PASS (pipe_done reached; data values are not checked against a golden model)\n");
    return 0;
}
