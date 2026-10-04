// tb_gvu_softmax.cpp -- has/gvu_softmax.h against the vector-unit golden rows (softmax_log.txt,
// softmax_pwl.txt; tools/fe/fe_ref_gvu.py softmax_row, default knobs). Tables from tools/has/export_gvu_tables.py.
// Row format: "g L | x_0..x_L-1 | m sigma r E | a_0..a_L-1". PWL recip: pwl_form seg, Base 7, W 6, E_shift 7 (gvu_knobs.json).
// usage: tb_gvu_softmax <vectors dir> <tables dir>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "has/gvu_softmax.h"

static std::vector<int32_t> read_table(const std::string &path)
{
    std::vector<int32_t> t;
    std::ifstream f(path);
    long v;
    while (f >> v) t.push_back(int32_t(v));
    if (t.empty()) { std::fprintf(stderr, "empty table %s\n", path.c_str()); std::exit(2); }
    return t;
}

static long run(const std::string &file, const std::string &tdir, bool pwl, long &rows, long &elems)
{
    has::Knobs k;   // HAS defaults: HALF_UP, SAT16 -- the golden's defaults
    has::LutIndirect log_lut(14);
    std::vector<std::vector<int32_t>> exp(3), rec(3);
    std::vector<has::LutPwl *> pwl_lut(3, nullptr);
    for (int g = 0; g < 3; g++)
    {
        const std::string pre = tdir + (pwl ? "/softmax_pwl_g" : "/softmax_log_g") + std::to_string(g);
        exp[g] = read_table(pre + "_exp.txt");
        if (pwl) { rec[g] = read_table(pre + "_recip.txt"); pwl_lut[g] = new has::LutPwl(rec[g], 7, 6, 7, has::LutPwl::SEG); }
    }
    std::ifstream f(file);
    std::string line;
    long bad = 0;
    while (std::getline(f, line))
    {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream in(line);
        int g, L; std::string bar;
        in >> g >> L >> bar;
        std::vector<int> x(L);
        for (int j = 0; j < L; j++) in >> x[j];
        long m, sigma, r, E;
        in >> bar >> m >> sigma >> r >> E >> bar;
        std::vector<int> a(L);
        for (int j = 0; j < L; j++) in >> a[j];
        has::SoftmaxCfg c;
        c.exp = exp[g].data();
        if (pwl) c.pwl_recip = pwl_lut[g]; else c.log_recip = &log_lut;
        const has::SoftmaxRow o = has::softmax_row(x, c, k);
        rows++; elems += L;
        long diff = (o.m != m) + (o.sigma != sigma) + (o.r != r) + (o.E != E);
        for (int j = 0; j < L; j++) diff += (o.a[j] != a[j]);
        if (diff)
        {
            if (bad < 5) std::printf("  mismatch %s row %ld (L=%d): m %d/%ld sigma %lld/%ld r %d/%ld E %d/%ld\n", file.c_str(), rows, L, o.m, m,
                                     (long long)o.sigma, sigma, o.r, r, o.E, E);
            bad++;
        }
    }
    for (auto *p : pwl_lut) delete p;
    return bad;
}

int main(int argc, char **argv)
{
    if (argc < 3) { std::fprintf(stderr, "usage: %s <vectors dir> <tables dir>\n", argv[0]); return 2; }
    long total = 0;
    for (int pwl = 0; pwl < 2; pwl++)
    {
        long rows = 0, elems = 0;
        const long bad = run(std::string(argv[1]) + (pwl ? "/softmax_pwl.txt" : "/softmax_log.txt"), argv[2], pwl != 0, rows, elems);
        std::printf("[tb_gvu_softmax] %-4s %5ld rows / %7ld elements, %ld rows mismatching\n", pwl ? "pwl" : "log", rows, elems, bad);
        total += bad + (rows == 0 ? 1 : 0);
    }
    std::printf("[tb_gvu_softmax] RESULT: %s\n", total == 0 ? "PASS" : "FAIL");
    return total == 0 ? 0 : 1;
}
