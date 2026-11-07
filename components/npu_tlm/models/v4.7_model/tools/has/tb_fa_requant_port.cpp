// tb_fa_requant_port.cpp -- the requant of the attention products as the model runs it on GvuTop (GEMM_FUSED stream, skip activation,
// has/gvu_top_stream.h requant_rows) against the vector files of tools/has/export_fa_requant_vec.py, under the two rules:
//   rule A (RTL): round half away, narrow WRAP   -- what gvu_pipe_controller.sv hardwires; the port must equal the file's "A" lines
//   rule U (model default): round half up, narrow SATURATE, zero point added without wrap (functional model, zp_add_wrap16 = 0)
// The pass criterion is rule A only. For rule U the port still adds the zero point with the 16-bit wrapping adder of the RTL, so it
// differs from the functional model where the narrowed value saturates (only the directed sets with sums over the whole int32 range;
// never on the attention sums of ViT-B). The line also prints how many lanes the two rules differ in.
// build: g++ -std=c++17 -O2 -Wall -I. tools/has/tb_fa_requant_port.cpp -o tb_fa_requant_port
// usage: tb_fa_requant_port <vector file>...
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "has/rtl_ref_gvu_top.h"

using namespace rtl_ref_gvu;

struct Phase
{
    int ph{0}, Z{0}, TS{0}, beats{0};
    long long M{0};
    std::vector<std::vector<long long>> d, A, U;
    std::vector<unsigned> mask;
};

static bool load(const char *fn, Phase &p)
{
    std::FILE *f = std::fopen(fn, "r");
    if (!f) return false;
    char tag[8];
    if (std::fscanf(f, "%7s %d %lld %d %d %d", tag, &p.ph, &p.M, &p.TS, &p.Z, &p.beats) != 6) { std::fclose(f); return false; }
    p.d.assign(size_t(p.beats), std::vector<long long>(32)); p.A = p.U = p.d; p.mask.assign(size_t(p.beats), 0);
    for (int b = 0; b < p.beats; b++)
    {
        unsigned m = 0;
        if (std::fscanf(f, "%7s %u", tag, &m) != 2) { std::fclose(f); return false; }
        p.mask[size_t(b)] = m;
        for (long long &v : p.d[size_t(b)]) if (std::fscanf(f, "%lld", &v) != 1) { std::fclose(f); return false; }
    }
    for (int k = 0; k < 2; k++)
        for (int b = 0; b < p.beats; b++)
        {
            if (std::fscanf(f, "%7s", tag) != 1) { std::fclose(f); return false; }
            for (long long &v : (tag[0] == 'A' ? p.A : p.U)[size_t(b)]) if (std::fscanf(f, "%lld", &v) != 1) { std::fclose(f); return false; }
        }
    std::fclose(f);
    return true;
}

// the stream loop of GvuTopStream::requant_rows, one beat per entry of p.d
static bool run(const Phase &p, RoundMode rm, NarrowMode nm, std::vector<std::vector<int>> &got)
{
    GvuTop top;
    GvuTop::In in;
    in.i_gemm_round = rm; in.i_gemm_narrow = nm; in.i_gemm_zp16 = static_cast<int8_t>(static_cast<uint8_t>(p.Z & 0xFF)); in.i_gemm_skip_act = true;
    in.i_gvu_op = GVU_OP_GEMM_FUSED; in.i_gemm_stream = true; in.i_pipeline_en = true;
    for (int k = 0; k < 100 && top.state() != GEMM_FUSED_ST1; k++) top.tick(in, true);
    if (top.state() != GEMM_FUSED_ST1) return false;
    got.assign(size_t(p.beats), std::vector<int>(32, 0));
    size_t sent = 0, recv = 0, guard = 4 * size_t(p.beats) + 400;
    while (recv < size_t(p.beats) && guard--)
    {
        if (sent < size_t(p.beats))
        {
            for (size_t l = 0; l < 32; l++) in.i_psum_vec[l] = static_cast<int32_t>(p.d[sent][l]);
            in.i_psum_valid = true; in.i_psum_scale = static_cast<int32_t>(p.M); in.i_psum_shift = static_cast<uint8_t>(p.TS);
            in.i_psum_mask = p.mask[sent]; in.i_psum_addr = static_cast<uint32_t>(sent);
        }
        else in.i_psum_valid = false;
        const GvuTop::Out o = top.tick(in, true);
        if (in.i_psum_valid && o.o_psum_ready) sent++;
        if (o.o_gemm_fused_valid)
        {
            for (size_t l = 0; l < 32; l++) if ((o.o_gemm_fused_mask >> l) & 1u) got[o.o_gemm_fused_addr][l] = o.o_gemm_fused_data[l];
            recv++;
        }
    }
    return recv == size_t(p.beats);
}

int main(int argc, char **argv)
{
    long files = 0, lanes = 0, bad_a = 0, bad_u = 0, a_vs_u = 0, fail = 0;
    for (int n = 1; n < argc; n++)
    {
        Phase p;
        if (!load(argv[n], p)) { std::fprintf(stderr, "cannot read %s\n", argv[n]); return 2; }
        std::vector<std::vector<int>> ga, gu;
        if (!run(p, RND_NEAR_AWAY, NARROW_WRAP, ga) || !run(p, RND_NEAR_UP, NARROW_SATURATE, gu)) { fail++; continue; }
        long ba = 0, bu = 0, au = 0, ln = 0;
        for (int b = 0; b < p.beats; b++)
            for (int l = 0; l < 32; l++)
            {
                if (!((p.mask[size_t(b)] >> l) & 1u)) continue;
                ln++;
                ba += ga[size_t(b)][size_t(l)] != p.A[size_t(b)][size_t(l)];
                bu += gu[size_t(b)][size_t(l)] != p.U[size_t(b)][size_t(l)];
                au += p.A[size_t(b)][size_t(l)] != p.U[size_t(b)][size_t(l)];
            }
        std::printf("%-28s phase %d M %lld TS %d Z %d beats %d lanes %ld: port(RTL rule) vs A differ %ld, port(model rule) vs U (info) differ %ld, A vs U differ %ld\n",
                    argv[n], p.ph, p.M, p.TS, p.Z, p.beats, ln, ba, bu, au);
        files++; lanes += ln; bad_a += ba; bad_u += bu; a_vs_u += au;
    }
    std::printf("tb_fa_requant_port: %ld files, %ld lanes, port vs A differ %ld, port vs U (info) differ %ld, rule A vs rule U differ %ld, failed runs %ld\n",
                files, lanes, bad_a, bad_u, a_vs_u, fail);
    std::printf("RESULT: %s\n", (bad_a == 0 && fail == 0 && files > 0) ? "PASS" : "FAIL");
    return bad_a || fail ? 1 : 0;
}
