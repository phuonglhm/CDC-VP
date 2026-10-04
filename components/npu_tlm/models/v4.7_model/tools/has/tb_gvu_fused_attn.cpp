// tb_gvu_fused_attn.cpp -- has/gvu_fused_attn.h against the vector-unit golden heads
// (fe_work/has/vectors/gvu/attn_<name>.txt: sections Q K V M X S A O, one matrix row per line).
// Parameters / exp tables from tools/has/export_gvu_attn.py. Checks X (phase 1), row stats m sigma r E, A (phase 2), O (phase 3).
// usage: tb_gvu_fused_attn <vectors dir> <tables dir>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include "has/gvu_fused_attn.h"

typedef std::vector<std::vector<long>> LMat;

static std::map<std::string, LMat> read_head(const std::string &path)
{
    std::map<std::string, LMat> s;
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line))
    {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream in(line);
        std::string tag;
        in >> tag;
        std::vector<long> row;
        long v;
        while (in >> v) row.push_back(v);
        s[tag].push_back(row);
    }
    return s;
}

static has::Mat to_int(const LMat &m)
{
    has::Mat r(m.size());
    for (size_t i = 0; i < m.size(); i++) r[i].assign(m[i].begin(), m[i].end());
    return r;
}

static long diff(const has::Mat &a, const LMat &b)
{
    long d = 0;
    if (a.size() != b.size()) return 1 << 30;
    for (size_t i = 0; i < a.size(); i++)
    {
        if (a[i].size() != b[i].size()) return 1 << 30;
        for (size_t j = 0; j < a[i].size(); j++) d += a[i][j] != b[i][j];
    }
    return d;
}

int main(int argc, char **argv)
{
    if (argc < 3) { std::fprintf(stderr, "usage: %s <vectors dir> <tables dir>\n", argv[0]); return 2; }
    const std::string vdir = argv[1], tdir = argv[2];
    has::Knobs k;
    has::LutIndirect recip(14);
    std::ifstream pf(tdir + "/attn_params.txt");
    std::string line;
    long heads = 0, bad_total = 0;
    while (std::getline(pf, line))
    {
        std::istringstream in(line);
        std::string name;
        has::AttnParams p;
        long Mqk, Mav;
        int mz, zc, az;
        in >> name >> p.Zq >> p.Zk >> p.Zv >> p.Zqk >> p.Zav >> Mqk >> p.TSqk >> Mav >> p.TSav >> mz >> zc >> az;
        p.Mqk = Mqk; p.Mav = Mav; p.mask_e_zero = mz; p.av_zv_corr = zc; p.asym_zp = az;
        std::vector<int32_t> exp;
        { std::ifstream ef(tdir + "/attn_exp_" + name + ".txt"); long v; while (ef >> v) exp.push_back(int32_t(v)); }
        auto s = read_head(vdir + "/attn_" + name + ".txt");
        const has::AttnResult r = has::fused_attn(to_int(s["Q"]), to_int(s["K"]), to_int(s["V"]), to_int(s["M"]), p, exp.data(), recip, k);
        long dstat = 0;
        for (size_t i = 0; i < r.stats.size() && i < s["S"].size(); i++)
        {
            const auto &g = s["S"][i];
            dstat += (r.stats[i].m != g[0]) + (r.stats[i].sigma != g[1]) + (r.stats[i].r != g[2]) + (r.stats[i].E != g[3]);
        }
        if (r.stats.size() != s["S"].size()) dstat += 1 << 20;
        const long dx = diff(r.X, s["X"]), da = diff(r.A, s["A"]), dout = diff(r.O, s["O"]);
        std::printf("[tb_gvu_fused_attn] %-18s L=%3zu d=%2zu | X %ld  stats %ld  A %ld  O %ld mismatches | ovf16 %llu\n", name.c_str(),
                    s["K"].size(), s["Q"][0].size(), dx, dstat, da, dout, (unsigned long long)r.ovf16);
        bad_total += dx + dstat + da + dout;
        heads++;
    }
    std::printf("[tb_gvu_fused_attn] RESULT: %s (%ld heads)\n", bad_total == 0 && heads > 0 ? "PASS" : "FAIL", heads);
    return bad_total == 0 && heads > 0 ? 0 : 1;
}
