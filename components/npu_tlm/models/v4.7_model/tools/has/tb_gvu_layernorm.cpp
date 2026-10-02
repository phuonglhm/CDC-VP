// tb_gvu_layernorm.cpp -- has/gvu_layernorm.h against the vector-unit golden
// (fe_work/has/vectors/gvu/ln_<case>.txt, per row: "X x..", "R S V v f w r E", "N n..", "Y y.."). Parameters from
// tools/has/export_gvu_ln.py. Checks every intermediate of stages 1-4 and the stage-5 output.
// usage: tb_gvu_layernorm <vectors dir> <tables dir>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "has/gvu_layernorm.h"

template <typename T> static std::vector<T> read_line(std::istream &f)
{
    std::string line;
    std::getline(f, line);
    std::istringstream in(line);
    std::vector<T> v;
    long long x;
    while (in >> x) v.push_back(T(x));
    return v;
}

int main(int argc, char **argv)
{
    if (argc < 3) { std::fprintf(stderr, "usage: %s <vectors dir> <tables dir>\n", argv[0]); return 2; }
    const std::string vdir = argv[1], tdir = argv[2];
    has::Knobs k;
    has::LutIndirect lut(14);
    std::ifstream cf(tdir + "/ln_cases.txt");
    std::string name;
    long cases = 0, bad_total = 0;
    while (cf >> name)
    {
        std::ifstream pf(tdir + "/ln_params_" + name + ".txt");
        const std::vector<long long> s = read_line<long long>(pf);
        has::LnParams p;
        p.H = int(s[0]); p.Pre_Shift = int(s[1]); p.M0_var = s[2]; p.TS_var = int(s[3]); p.Z_var = int(s[4]);
        p.M0_7 = s[5]; p.TS_7 = int(s[6]); p.E_bias = s[7]; p.Z_7 = int(s[8]); p.M0_div = s[9]; p.TS_div = int(s[10]);
        p.Z_div = int(s[11]); p.Z_mul = int(s[12]); p.Z_out = int(s[13]); p.r8_floor_z_out = s[14] != 0; p.r8_z_out = int(s[15]);
        p.r9_exact = s[16] != 0; p.r9_lsb_bits = int(s[17]); p.out_int16 = s[18] != 0;
        p.gamma = read_line<int>(pf); p.beta = read_line<int64_t>(pf); p.M0_mul = read_line<int64_t>(pf); p.TS_mul = read_line<int>(pf);
        std::ifstream vf(vdir + "/ln_" + name + ".txt");
        std::string line;
        std::vector<int> x;
        long rows = 0, bad_rows = 0, bad_y = 0;
        has::LnRow o;
        while (std::getline(vf, line))
        {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream in(line);
            std::string tag;
            in >> tag;
            std::vector<long long> v;
            long long t;
            while (in >> t) v.push_back(t);
            if (tag == "X") { x.assign(v.begin(), v.end()); o = has::layernorm_row(x, p, lut, k); rows++; }
            else if (tag == "R")
            {
                const bool ok = v.size() == 7 && o.S == v[0] && o.V == v[1] && o.v == v[2] && o.f == v[3] && o.w == v[4] && o.r == v[5] && o.E == v[6];
                if (!ok) { if (bad_rows < 3) std::printf("  %s row %ld stats: got S %lld V %lld v %d f %d w %d r %d E %d\n", name.c_str(), rows,
                                                    (long long)o.S, (long long)o.V, o.v, o.f, o.w, o.r, o.E); bad_rows++; }
            }
            else if (tag == "N") { for (size_t j = 0; j < v.size() && j < o.n.size(); j++) bad_y += o.n[j] != v[j]; }
            else if (tag == "Y") { for (size_t j = 0; j < v.size() && j < o.y.size(); j++) bad_y += o.y[j] != v[j]; }
        }
        std::printf("[tb_gvu_layernorm] %-12s H=%4d rows %3ld | stage 1-4 stats mismatching rows %ld | n/y element mismatches %ld\n",
                    name.c_str(), p.H, rows, bad_rows, bad_y);
        bad_total += bad_rows + bad_y + (rows == 0);
        cases++;
    }
    std::printf("[tb_gvu_layernorm] RESULT: %s (%ld cases)\n", bad_total == 0 && cases > 0 ? "PASS" : "FAIL", cases);
    return bad_total == 0 && cases > 0 ? 0 : 1;
}
