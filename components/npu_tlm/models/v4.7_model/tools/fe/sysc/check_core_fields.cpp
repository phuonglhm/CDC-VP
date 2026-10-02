// Every tile shape of the frontend
// program must be encodable as SAURIA core configuration. Uses driver/libsauria_cfg.h (sauria_compute_core_fields)
// and sauria_cfg_layout.h (SAURIA_CFG_LAYOUT widths) unmodified, target int8_32x32.
//
// Input TSV (from tools/fe/fe_step6b_core_desc.py): B_w B_h d s c_til k_til h_til w_til X_used Y_used preload_en count job
// Output: one line per violating (shape, field), then a summary. Exit 0 when every field fits its width.
// Build: g++ -std=c++17 -O2 -I. tools/fe/sysc/check_core_fields.cpp -o fe_work/step6/check_core_fields

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "driver/libsauria_cfg.h"

using namespace sauria;

int main(int argc, char *argv[])
{
    const char *path = argc > 1 ? argv[1] : "fe_work/step6/core_desc.tsv";
    const SauriaTarget *t = sauria_find_target("int8_32x32");
    if (!t)
    {
        std::cout << "target int8_32x32 not found" << std::endl;
        return 2;
    }
    CfgWidths w;
    w.idx_a = t->idx_a; w.idx_w = t->idx_w; w.idx_o = t->idx_o; w.X = t->X; w.Y = t->Y;

    std::ifstream f(path);
    std::string line;
    long shapes = 0, tiles = 0, bad_shapes = 0, bad_tiles = 0;
    uint64_t fmax[F_CFG_COUNT] = {0};
    while (std::getline(f, line))
    {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream is(line);
        SauriaLayerDesc c{};
        long count = 0;
        std::string job;
        is >> c.B_w >> c.B_h >> c.d >> c.s >> c.c_til >> c.k_til >> c.h_til >> c.w_til >> c.X_used >> c.Y_used >> c.preload_en >> count >> job;
        if (!is)
        {
            std::cout << "bad line: " << line << std::endl;
            return 2;
        }
        uint64_t fv[F_CFG_COUNT];
        sauria_compute_core_fields(c, *t, fv);
        bool bad = false;
        for (int i = 0; i < SAURIA_CFG_LAYOUT_N; i++)
        {
            const CfgFieldSpec &spec = SAURIA_CFG_LAYOUT[i];
            if (spec.special == SP_SKIP_PER_ROW)
                continue;
            uint32_t width = cfg_resolve_width(spec.wkind, w);
            uint64_t v = fv[spec.field];
            if (v > fmax[spec.field])
                fmax[spec.field] = v;
            if (width < 64 && (v >> width) != 0)
            {
                std::printf("VIOLATION job=%s shape=[%d %d %d %d c%d k%d h%d w%d X%d Y%d] field_id=%d value=%llu width=%u\n",
                            job.c_str(), c.B_w, c.B_h, c.d, c.s, c.c_til, c.k_til, c.h_til, c.w_til, c.X_used, c.Y_used,
                            int(spec.field), (unsigned long long)v, width);
                bad = true;
            }
        }
        shapes++;
        tiles += count;
        if (bad)
        {
            bad_shapes++;
            bad_tiles += count;
        }
    }
    std::printf("[check_core_fields] target %s (idx_a %d, idx_w %d, idx_o %d): shapes %ld, tiles %ld, violating shapes %ld (tiles %ld)\n",
                t->name, t->idx_a, t->idx_w, t->idx_o, shapes, tiles, bad_shapes, bad_tiles);
    std::printf("[check_core_fields] max value per field id:");
    for (int i = 0; i < F_CFG_COUNT; i++)
        std::printf(" %d:%llu", i, (unsigned long long)fmax[i]);
    std::printf("\n[check_core_fields] RESULT: %s\n", (shapes > 0 && bad_shapes == 0) ? "PASS" : "FAIL");
    return (shapes > 0 && bad_shapes == 0) ? 0 : 1;
}
