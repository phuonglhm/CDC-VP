// gen_case — generate a COMPLETE, runnable demo case entirely in C (no Python):
// random inputs -> initial_dram + gold_dram (via sauria_golden) + GoldenStimuli
// (via sauria_stim) + case.env + sauria_tmp shapes. The emitted folder runs through
// tb_demo exactly like a captured case.
//
// Build: g++ -std=c++17 -I. -Idriver tools/gen_case.cpp -o /tmp/gen_case
// Run:   /tmp/gen_case <version> "<shape>" <out_case_dir> [seed]
//   <shape> = Bw Bh d s Cin Cw Ch Cout Xused Yused preload
#include "sauria_run.h"
#include "sauria_golden.h"
#include "sauria_stim.h"
#include "sauria_targets.h"
#include <cstdio>
#include <string>
#include <vector>
#include <random>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
using namespace sauria;

static void mkdirs(const std::string &p) { ::mkdir(p.c_str(), 0777); }
static void write_bytes_hex(const std::string &path, const std::vector<uint8_t> &b)
{
    std::ofstream f(path);
    for (uint8_t v : b) { char buf[8]; std::snprintf(buf, sizeof(buf), "%X\n", v); f << buf; }
}
static void write_text(const std::string &path, const std::string &s) { std::ofstream(path) << s; }

int main(int argc, char **argv)
{
    if (argc < 4) { printf("usage: gen_case <version> \"<shape>\" <out_dir> [seed]\n"); return 2; }
    std::string ver = argv[1], shape = argv[2], out = argv[3];
    uint32_t seed = argc > 4 ? (uint32_t)std::stoul(argv[4]) : 12345u;
    const SauriaTarget *t = sauria_find_target(ver.c_str());
    if (!t) { printf("[gen_case] unknown target '%s'\n", ver.c_str()); return 2; }

    std::istringstream is(shape);
    int Bw, Bh, d, s, Cin, Cw, Ch, Cout, Xused, Yused, pre;
    is >> Bw >> Bh >> d >> s >> Cin >> Cw >> Ch >> Cout >> Xused >> Yused >> pre;
    const bool fp = (t->op_type == 1);

    // The golden here is emitted in NATURAL [Cout][Ch][Cw] order, which equals the
    // SAURIA gold_dram order only when there is NO W-splitting (Cw == Yused, i.e.
    // ceil(Cw/Yused) == 1). W-splitting reorders the output; that mapping is not yet
    // ported, so reject it rather than emit a wrong golden.
    if (Cw % Yused != 0) { printf("[gen_case] Cw must be a multiple of Yused\n"); return 2; }
    if (Cw / Yused != 1) {
        printf("[gen_case] W-splitting (Cw=%d > Yused=%d) not supported by the natural-order "
               "golden; use Cw==Yused.\n", Cw, Yused);
        return 2;
    }

    SauriaConvShape sh{}; sh.B_w = Bw; sh.B_h = Bh; sh.d = d; sh.s = s;
    sh.C_in = Cin; sh.C_w = Cw; sh.C_h = Ch; sh.C_out = Cout; sh.preload_en = pre;
    sauria_derive_input_dims(sh);

    SauriaLayerDesc desc{}; desc.B_w = Bw; desc.B_h = Bh; desc.d = d; desc.s = s;
    desc.c_til = Cin; desc.k_til = Cout; desc.h_til = Ch; desc.w_til = Cw;
    desc.X_used = Xused; desc.Y_used = Yused; desc.preload_en = pre;
    desc.C_w = Cw; desc.C_h = Ch; desc.C_c = Cout; desc.A_c = Cin;

    // Random inputs, quantized to the datatype so pack and golden are consistent.
    std::mt19937 rng(seed);
    auto quant = [&](double x) -> double {
        if (fp) return (double)fp16_t::half_to_float(
            (uint16_t)sauria_enc_elem(x, true, 16)); // round through SAURIA fp encode
        return (double)(int64_t)llround(x);
    };
    int A_n = Cin * sh.A_h * sh.A_w, B_n = Cout * Cin * Bh * Bw, C_n = Cout * Ch * Cw;
    std::vector<double> A(A_n), B(B_n), Cpre(C_n, 0.0);
    if (fp) {
        std::normal_distribution<double> nd(0.0, 1.0);
        for (auto &v : A) v = quant(nd(rng));
        for (auto &v : B) v = quant(nd(rng));
        if (pre) for (auto &v : Cpre) v = quant(nd(rng));
    } else {
        int lim = (t->ia_w >= 16) ? 4000 : 100; // keep products modest
        std::uniform_int_distribution<int> ud(-lim, lim);
        for (auto &v : A) v = ud(rng);
        for (auto &v : B) v = ud(rng);
        if (pre) for (auto &v : Cpre) v = ud(rng);
    }

    // Prepare (config + input DRAM) and golden.
    SauriaRunInputs in = sauria_prepare(*t, desc, A.data(), Cin, sh.A_h, sh.A_w,
                                        B.data(), Cout, Cin, Bh, Bw, Cpre.data(), Cout, Ch, Cw);
    std::vector<double> gold = sauria_reference_conv(A.data(), B.data(), Cpre.data(), sh, *t);

    // gold_dram = initial_dram with the C region overwritten by the golden.
    std::vector<uint8_t> gdram = in.initial_dram;
    int ob = t->out_bytes;
    for (int i = 0; i < C_n; i++) {
        uint64_t bits = sauria_enc_elem(gold[i], fp, t->oc_w);
        for (int k = 0; k < ob; k++) {
            size_t a = (size_t)in.C_off + (size_t)i * ob + k;
            if (a < gdram.size()) gdram[a] = (uint8_t)((bits >> (8 * k)) & 0xFF);
        }
    }

    // Emit the case folder.
    mkdirs(out); mkdirs(out + "/stimuli"); mkdirs(out + "/sauria_tmp");
    write_bytes_hex(out + "/stimuli/initial_dram.txt", in.initial_dram);
    write_bytes_hex(out + "/stimuli/gold_dram.txt", gdram);
    write_text(out + "/stimuli/GoldenStimuli.txt", sauria_stim_to_text(sauria_emit_stim(in.controller_args)));
    // sauria_tmp shape files (tb reads A_Mat[2] = K = Bw*Bh*Cin as mvm_k).
    int K = Bw * Bh * Cin;
    { std::ostringstream a, b, c;
      a << 1 << "\n" << Cout << "\n" << K << "\n";
      b << 1 << "\n" << K << "\n" << Cout << "\n";
      c << 1 << "\n" << Cout << "\n" << Cw << "\n";
      write_text(out + "/sauria_tmp/sauria_A_Mat_mvm_shape.txt", a.str());
      write_text(out + "/sauria_tmp/sauria_B_Mat_mvm_shape.txt", b.str());
      write_text(out + "/sauria_tmp/sauria_C_compute_mvm_shape.txt", c.str()); }
    // case.env (IDX widths from the target + dtype build flags).
    uint32_t rb = 65536; while (rb < in.initial_dram.size()) rb <<= 1;
    { std::ostringstream e;
      e << "TITLE=\"SELF-GEN " << ver << " " << shape << "\"\n";
      e << "DESC=\"C-generated case (no Python)\"\n";
      e << "EVAL_X=" << t->X << "\nEVAL_Y=" << t->Y << "\n";
      e << "IDX_FLAGS=\"-DSAURIA_ACT_IDX_W=" << t->idx_a << " -DSAURIA_WEI_IDX_W=" << t->idx_w
        << " -DSAURIA_OUT_IDX_W=" << t->idx_o;
      if (t->build_flags[0]) e << " " << t->build_flags;
      e << "\"\n";
      e << "VERSION=\"" << ver << "\"\nSHAPE=\"" << shape << "\"\nREGION_BYTES=" << rb << "\n";
      write_text(out + "/case.env", e.str()); }

    printf("[gen_case] wrote %s  (%s, %s, seed=%u)\n", out.c_str(), ver.c_str(), shape.c_str(), seed);
    return 0;
}
