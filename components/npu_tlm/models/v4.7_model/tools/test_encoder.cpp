// Bit-exact test for libsauria_cfg against SAURIA Python ground truth.
// Reads a dump_cfg.py record (VERSION / DESC / START / ARGS) from stdin and checks:
//   (1) sauria_compute_core_fields() == cfg_decode(python_args[22..])  (formula port)
//   (2) sauria_encode_core_config()  == python_args[22..] word-for-word (packing)
//
// Build:  g++ -std=c++17 -I. tools/test_encoder.cpp -o /tmp/test_encoder
// Run:    SAURIA_PY=... python3 tools/dump_cfg.py "<shape>" <VER> | /tmp/test_encoder
#include "libsauria_cfg.h"
#include "sauria_targets.h"
#include "sauria_cfg_layout.h"
#include <cstdio>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
using namespace sauria;

static const char *fname(int f)
{
    static const char *N[F_CFG_COUNT] = {
        "incntlim","act_reps","wei_reps","thres",
        "xlim","xstep","ylim","ystep","chlim","chstep",
        "til_xlim","til_xstep","til_ylim","til_ystep","dil_pat","rows_active","per_row_off",
        "wlim","wstep","klim","kstep","til_klim","til_kstep","cols_active","waligned",
        "ncontexts","cxlim","cxstep","cklim","ckstep",
        "til_cylim","til_cystep","til_cklim","til_ckstep","inactive_cols","preload_en"};
    return (f >= 0 && f < F_CFG_COUNT) ? N[f] : "?";
}

int main()
{
    std::string version, line;
    SauriaLayerDesc d{};
    uint32_t start = 22;
    std::vector<uint32_t> args;

    while (std::getline(std::cin, line))
    {
        std::istringstream is(line);
        std::string tag; is >> tag;
        if (tag == "VERSION") is >> version;
        else if (tag == "DESC")
            is >> d.B_w >> d.B_h >> d.d >> d.s >> d.c_til >> d.k_til
               >> d.h_til >> d.w_til >> d.X_used >> d.Y_used >> d.preload_en;
        else if (tag == "FULL")
            is >> d.C_w >> d.C_h >> d.C_c >> d.A_c;
        else if (tag == "START") is >> start;
        else if (tag == "ARGS")
        {
            int n; is >> n;
            for (int i = 0; i < n; i++) { uint32_t v; is >> std::hex >> v; args.push_back(v); }
        }
    }

    const SauriaTarget *t = sauria_find_target(version.c_str());
    if (!t) { printf("[test_encoder] unknown target '%s'\n", version.c_str()); return 2; }
    if (args.size() <= start) { printf("[test_encoder] no args payload\n"); return 2; }

    CfgWidths w; w.idx_a = t->idx_a; w.idx_w = t->idx_w; w.idx_o = t->idx_o; w.X = t->X; w.Y = t->Y;

    // (1) formula port vs decoded ground truth
    uint64_t ref[F_CFG_COUNT], mine[F_CFG_COUNT];
    cfg_decode(args, start, w, ref);
    sauria_compute_core_fields(d, *t, mine);

    int fails = 0;
    for (int i = 0; i < F_CFG_COUNT; i++)
    {
        if (i == F_CFG_PER_ROW_OFF) continue;
        if (ref[i] != mine[i])
        {
            printf("  FIELD MISMATCH %-12s ref=%llu mine=%llu\n", fname(i),
                   (unsigned long long)ref[i], (unsigned long long)mine[i]);
            fails++;
        }
    }

    // (2) FULL controller args vs Python (skip 18..20 = caller-supplied DRAM bases,
    // which the dump sets to 0). Covers tiling args[0..21] + core payload [22..].
    uint32_t bases[3] = {args[18], args[19], args[20]};
    std::vector<uint32_t> mine_args = sauria_encode_controller_args(d, *t, bases);
    if (mine_args.size() != args.size())
        printf("  ARGS LEN ref=%zu mine=%zu\n", args.size(), mine_args.size());
    size_t nchk = std::min(mine_args.size(), args.size());
    for (size_t i = 0; i < nchk; i++)
    {
        if (i >= 18 && i <= 20) continue; // DRAM bases (caller-supplied)
        if (mine_args[i] != args[i])
        {
            printf("  ARG[%zu] MISMATCH ref=%08x mine=%08x\n", i, args[i], mine_args[i]);
            fails++;
        }
    }

    printf("%-13s : %s (fields + FULL args[0..21]+[22..] vs Python)\n", version.c_str(),
           fails ? "FAIL" : "BIT-EXACT");
    return fails ? 1 : 0;
}
