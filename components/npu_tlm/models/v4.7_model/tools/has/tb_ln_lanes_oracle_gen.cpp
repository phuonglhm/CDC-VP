// tb_ln_lanes_oracle_gen.cpp -- test vectors of the lane LayerNorm (has/rtl_ref_gvu_ln_lanes.h, the model's proposal P-LN1..P-LN5) for the RTL
// oracle of gvu_ln_lanes.sv: every group is run on GvuLnLanes edge by edge and written as one text file of integers that the
// SystemVerilog bench reads with $fscanf. Groups: H 1, 32, 768, 197 with 32 rows, then random H 1..768, rows 1..32, every round mode;
// random parameters as tools/has/tb_gvu_ln_lanes.cpp; lanes above `rows` hold random bytes that the run must leave as they are.
// Each output is also checked against has::layernorm_row (the functional model) before it is written.
// build: g++ -std=c++17 -O2 -Wall -I. tools/has/tb_ln_lanes_oracle_gen.cpp -o tb_ln_lanes_oracle_gen
// usage: tb_ln_lanes_oracle_gen <out dir> [groups, default 40] [seed, default 1840]
// file g<n>.txt (all integers, whitespace separated):
//   H rows round Pre_Shift M0_var TS_var Z_var M0_7 TS_7 E_bias Z_7 M0_div TS_div Z_div Z_mul Z_out r8_floor r8_z r9_exact r9_lsb M0_mul TS_mul
//   dep_read dep_st1 dep_st2 dep_st3 dep_st4 dep_st5 cycles
//   256 x (r E)                 the reciprocal square root table of LutIndirect for w = 0..255
//   H x (gamma beta)            per channel
//   H x 32 x (input byte)       vbank0 word k, lanes 0..31 (int8)
//   32 x (S V v w r E)          lane statistics at the end of the run (0 for lanes above rows)
//   H x 32 x (output byte)      vbank0 word k after the run
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>
#include "has/rtl_ref_gvu_ln_lanes.h"

namespace
{
    has::LnParams random_params(std::mt19937_64 &g, int H)
    {
        auto u = [&](long long lo, long long hi) { return std::uniform_int_distribution<long long>(lo, hi)(g); };
        has::LnParams p;
        p.H = H; p.Pre_Shift = int(u(0, 10));
        p.M0_var = u(1ll << 30, (1ll << 31) - 1); p.TS_var = int(u(28, 48)); p.Z_var = int(u(-128, -100));
        p.M0_7 = u(1ll << 30, (1ll << 31) - 1); p.TS_7 = int(u(28, 40)); p.E_bias = u(-1000, 1000); p.Z_7 = int(u(-128, -100));
        p.M0_div = u(1ll << 30, (1ll << 31) - 1); p.TS_div = int(u(36, 60)); p.Z_div = int(u(-4, 4));
        p.Z_mul = int(u(-8, 8)); p.Z_out = int(u(-8, 8));
        p.r8_floor_z_out = u(0, 1) != 0; p.r8_z_out = int(u(1, 3));
        p.r9_exact = u(0, 3) != 0; p.r9_lsb_bits = int(u(8, 32));
        const long long m0 = u(1ll << 30, (1ll << 31) - 1);
        const int ts = int(u(28, 40));
        p.gamma.resize(size_t(H)); p.beta.resize(size_t(H)); p.M0_mul.assign(size_t(H), m0); p.TS_mul.assign(size_t(H), ts);
        for (int j = 0; j < H; j++) { p.gamma[size_t(j)] = int(u(-128, 127)); p.beta[size_t(j)] = u(-3000, 3000); }
        return p;
    }
}

// One group of the LayerNorm lane vectors (HW_RTL_ALIGNMENT.md, section 11), files ln_group_*.txt: a CFG line, the GAMMA and BETA lines, then X / Y per token.
static bool read_group(const char *fn, has::LnParams &p, int &rows, int &rm, std::vector<std::vector<int>> &X, std::vector<std::vector<int>> &Y)
{
    std::FILE *f = std::fopen(fn, "r");
    if (!f) return false;
    char tag[16];
    long long v[22];
    if (std::fscanf(f, "%15s", tag) != 1) { std::fclose(f); return false; }
    for (long long &x : v) if (std::fscanf(f, "%lld", &x) != 1) { std::fclose(f); return false; }
    p = has::LnParams();
    p.H = int(v[0]); rows = int(v[1]); rm = int(v[2]); p.Pre_Shift = int(v[3]); p.M0_var = v[4]; p.TS_var = int(v[5]); p.Z_var = int(v[6]);
    p.M0_7 = v[7]; p.TS_7 = int(v[8]); p.E_bias = v[9]; p.Z_7 = int(v[10]); p.M0_div = v[11]; p.TS_div = int(v[12]); p.Z_div = int(v[13]);
    p.Z_mul = int(v[14]); p.Z_out = int(v[15]); p.r8_floor_z_out = v[16] != 0; p.r8_z_out = int(v[17]); p.r9_exact = v[18] != 0; p.r9_lsb_bits = int(v[19]);
    p.M0_mul.assign(size_t(p.H), v[20]); p.TS_mul.assign(size_t(p.H), int(v[21]));
    p.gamma.resize(size_t(p.H)); p.beta.resize(size_t(p.H));
    std::fscanf(f, "%15s", tag);
    for (int &x : p.gamma) std::fscanf(f, "%d", &x);
    std::fscanf(f, "%15s", tag);
    for (int64_t &x : p.beta) { long long t = 0; std::fscanf(f, "%lld", &t); x = t; }
    X.assign(32, std::vector<int>(size_t(p.H), 0)); Y.assign(size_t(rows), std::vector<int>(size_t(p.H), 0));
    for (int l = 0; l < rows; l++)
    {
        std::fscanf(f, "%15s", tag); for (int &x : X[size_t(l)]) std::fscanf(f, "%d", &x);
        std::fscanf(f, "%15s", tag); for (int &x : Y[size_t(l)]) std::fscanf(f, "%d", &x);
    }
    std::fclose(f);
    return true;
}

int main(int argc, char **argv)
{
    using namespace rtl_ref_gvu;
    if (argc < 2) { std::fprintf(stderr, "usage: %s <out dir> [groups] [seed]   |   %s <out dir> vec <ln_group files..>\n", argv[0], argv[0]); return 2; }
    const std::string out = argv[1];
    const bool from_files = argc > 2 && std::string(argv[2]) == "vec";
    const int groups = from_files ? argc - 3 : argc > 2 ? std::atoi(argv[2]) : 40;
    std::mt19937_64 g(!from_files && argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 1840);
    const has::LutIndirect lut(14, true, true);
    const LnLaneDepths d = LnLaneDepths::with_lut();
    long written = 0, lanes = 0, model_bad = 0, file_bad = 0;
    for (int n = 0; n < groups; n++)
    {
        int H = 0, rows = 0, rm = 0;
        has::LnParams p;
        std::vector<std::vector<int>> X, Yf;
        if (from_files)
        {
            if (!read_group(argv[3 + n], p, rows, rm, X, Yf)) { std::fprintf(stderr, "cannot read %s\n", argv[3 + n]); return 2; }
            H = p.H;
        }
        else
        {
            H = n < 4 ? (n == 0 ? 1 : n == 1 ? 32 : n == 2 ? 768 : 197) : int(std::uniform_int_distribution<int>(1, 768)(g));
            rows = n < 4 ? 32 : int(std::uniform_int_distribution<int>(1, 32)(g));
            rm = n % 7;
            p = random_params(g, H);
        }
        if (!ln_lanes_fit(p)) { std::printf("group %d refused by ln_lanes_fit\n", n); continue; }
        if (!from_files)
        {
            X.assign(32, std::vector<int>(size_t(H)));
            for (int l = 0; l < 32; l++)
            {
                const int lo = std::uniform_int_distribution<int>(-128, 0)(g), hi = std::uniform_int_distribution<int>(lo, 127)(g);
                for (int &v : X[size_t(l)]) v = l < rows ? std::uniform_int_distribution<int>(lo, hi)(g) : std::uniform_int_distribution<int>(-128, 127)(g);
            }
        }
        else
            for (int l = rows; l < 32; l++)
                for (int &v : X[size_t(l)]) v = std::uniform_int_distribution<int>(-128, 127)(g);
        GvuScratchpad sp;
        for (int c = 0; c < H; c++)
        {
            SpBytes w(32, 0);
            for (int l = 0; l < 32; l++) w[size_t(l)] = uint8_t(int8_t(X[size_t(l)][size_t(c)]));
            sp.sram(0, 0).poke(unsigned(c), w);
        }
        ln_lanes_load_params(sp, p);
        GvuLnLanes u;
        u.begin(sp, rows, p, lut, rm, d);
        while (!u.tick()) {}
        const LnLaneRun &run = u.result();
        has::Knobs k;
        k.round_mode = rm;
        for (int l = 0; l < rows; l++)
        {
            const has::LnRow o = has::layernorm_row(X[size_t(l)], p, lut, k);
            for (int c = 0; c < H; c++)
            {
                const int got = int8_t(sp.sram(0, 0).word(unsigned(c))[size_t(l)]);
                model_bad += got != o.y[size_t(c)];
                if (from_files) file_bad += got != Yf[size_t(l)][size_t(c)];
            }
        }
        const std::string fn = out + (from_files ? "/v" : "/g") + std::to_string(n) + ".txt";
        std::FILE *f = std::fopen(fn.c_str(), "w");
        if (!f) { std::fprintf(stderr, "cannot write %s\n", fn.c_str()); return 2; }
        std::fprintf(f, "%d %d %d %d %lld %d %d %lld %d %lld %d %lld %d %d %d %d %d %d %d %d %lld %d\n", H, rows, rm, p.Pre_Shift,
                     (long long)p.M0_var, p.TS_var, p.Z_var, (long long)p.M0_7, p.TS_7, (long long)p.E_bias, p.Z_7, (long long)p.M0_div, p.TS_div,
                     p.Z_div, p.Z_mul, p.Z_out, int(p.r8_floor_z_out), p.r8_z_out, int(p.r9_exact), p.r9_lsb_bits, (long long)p.M0_mul[0], p.TS_mul[0]);
        std::fprintf(f, "%d %d %d %d %d %d %llu\n", d.read, d.st1, d.st2, d.st3, d.st4, d.st5, (unsigned long long)run.cycles());
        for (unsigned w = 0; w < 256; w++)
        {
            const has::LutResult lr = lut.rsqrt(w);
            std::fprintf(f, "%d %d\n", int(lr.r), lr.e_shift);
        }
        for (int c = 0; c < H; c++) std::fprintf(f, "%d %lld\n", p.gamma[size_t(c)], (long long)p.beta[size_t(c)]);
        for (int c = 0; c < H; c++)
        {
            for (int l = 0; l < 32; l++) std::fprintf(f, "%d ", X[size_t(l)][size_t(c)]);
            std::fprintf(f, "\n");
        }
        for (int l = 0; l < 32; l++)
            std::fprintf(f, "%lld %lld %d %d %d %d\n", (long long)u.lane_S(l), (long long)u.lane_V(l), u.lane_v(l), u.lane_w(l), u.lane_r(l), u.lane_E(l));
        for (int c = 0; c < H; c++)
        {
            const SpBytes w = sp.sram(0, 0).word(unsigned(c));
            for (int l = 0; l < 32; l++) std::fprintf(f, "%d ", int(int8_t(w[size_t(l)])));
            std::fprintf(f, "\n");
        }
        std::fclose(f);
        written++;
        lanes += long(rows) * H;
        std::printf("g%d H %d rows %d round %d cycles %llu\n", n, H, rows, rm, (unsigned long long)run.cycles());
    }
    std::printf("tb_ln_lanes_oracle_gen: %ld groups written, %ld lanes, depths read %d st %d %d %d %d %d, lane run vs has::layernorm_row differ %ld%s\n",
                written, lanes, d.read, d.st1, d.st2, d.st3, d.st4, d.st5, model_bad,
                from_files ? (", vs the Y of the files differ " + std::to_string(file_bad)).c_str() : "");
    return model_bad || file_bad ? 1 : 0;
}
