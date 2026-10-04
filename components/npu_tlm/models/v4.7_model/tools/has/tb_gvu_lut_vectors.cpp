// tb_gvu_lut_vectors.cpp -- has/gvu_lut.h against the vector-unit golden vectors (tools/fe/fe_ref_gvu.py) at the default knobs (q14s tables, floor interpolation, zero input saturated).
//   log-scale: lut_log_recip.txt / lut_log_rsqrt.txt  (x r E_shift)         -> has::LutIndirect
//   PWL:       lut_pwl_seg_recip.txt / lut_pwl_uniform_recip.txt (x idx frac r E_shift) -> has::LutPwl with the table exported
//              from gvu_tables.json by tools/has/export_gvu_tables.py (one value per line)
// usage: tb_gvu_lut_vectors <vectors dir> <exported tables dir>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "has/gvu_lut.h"

static std::vector<int32_t> read_table(const std::string &path)
{
    std::vector<int32_t> t;
    std::ifstream f(path);
    long v;
    while (f >> v) t.push_back(int32_t(v));
    return t;
}

static int header_int(const std::string &line, const std::string &key)
{
    const size_t p = line.find(key + "=");
    if (p == std::string::npos) { std::fprintf(stderr, "header has no %s: %s\n", key.c_str(), line.c_str()); std::exit(2); }
    return std::atoi(line.c_str() + p + key.size() + 1);
}

static long check_log(const std::string &path, bool rsqrt, long &n)
{
    has::LutIndirect lut(14);
    std::ifstream f(path);
    std::string line;
    long bad = 0;
    while (std::getline(f, line))
    {
        if (line.empty() || line[0] == '#') continue;
        unsigned long x; long r, e;
        std::istringstream(line) >> x >> r >> e;
        const has::LutResult res = rsqrt ? lut.rsqrt(uint32_t(x)) : lut.recip(uint32_t(x));
        n++;
        if (res.r != r || res.e_shift != e)
        {
            if (bad < 5) std::printf("  mismatch %s x=%lu: got (%d,%d) want (%ld,%ld)\n", path.c_str(), x, res.r, res.e_shift, r, e);
            bad++;
        }
    }
    return bad;
}

static long check_pwl(const std::string &path, const std::string &table, has::LutPwl::Form form, long &n)
{
    std::ifstream f(path);
    std::string line;
    std::getline(f, line);   // "# LUT_RCE PWL recip, pwl_form=seg Base=7 W=6 E_shift(instr)=7; ..."
    const int base = header_int(line, "Base"), w = header_int(line, "W"), es = header_int(line, "E_shift(instr)");
    has::LutPwl lut(read_table(table), base, w, es, form);
    long bad = 0;
    while (std::getline(f, line))
    {
        if (line.empty() || line[0] == '#') continue;
        unsigned long x; long idx, frac, r, e;
        std::istringstream(line) >> x >> idx >> frac >> r >> e;
        int gi, gf;
        lut.index(uint32_t(x), gi, gf);
        const has::LutResult res = lut.recip(uint32_t(x));
        n++;
        if (gi != idx || gf != frac || res.r != r || res.e_shift != e)
        {
            if (bad < 5)
                std::printf("  mismatch %s x=%lu: got (%d,%d,%d,%d) want (%ld,%ld,%ld,%ld)\n", path.c_str(), x, gi, gf, res.r, res.e_shift,
                            idx, frac, r, e);
            bad++;
        }
    }
    return bad;
}

int main(int argc, char **argv)
{
    if (argc < 3) { std::fprintf(stderr, "usage: %s <vectors dir> <tables dir>\n", argv[0]); return 2; }
    const std::string v = argv[1], t = argv[2];
    long total_bad = 0;
    struct { const char *name; long bad, n; } rows[4] = {};
    rows[0].name = "log recip";   rows[0].bad = check_log(v + "/lut_log_recip.txt", false, rows[0].n);
    rows[1].name = "log rsqrt";   rows[1].bad = check_log(v + "/lut_log_rsqrt.txt", true, rows[1].n);
    rows[2].name = "pwl seg";     rows[2].bad = check_pwl(v + "/lut_pwl_seg_recip.txt", t + "/pwl_seg_recip.txt", has::LutPwl::SEG, rows[2].n);
    rows[3].name = "pwl uniform"; rows[3].bad = check_pwl(v + "/lut_pwl_uniform_recip.txt", t + "/pwl_uniform_recip.txt", has::LutPwl::UNIFORM, rows[3].n);
    for (auto &r : rows)
    {
        std::printf("[tb_gvu_lut_vectors] %-12s %7ld vectors, %ld mismatches\n", r.name, r.n, r.bad);
        total_bad += r.bad + (r.n == 0 ? 1 : 0);
    }
    std::printf("[tb_gvu_lut_vectors] RESULT: %s\n", total_bad == 0 ? "PASS" : "FAIL");
    return total_bad == 0 ? 0 : 1;
}
