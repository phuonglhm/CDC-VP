// AUTO-GENERATED from sauria_targets.csv by tools/gen_targets.py — DO NOT EDIT.
// SAURIA NPU HW-version manifest: single source of truth for geometry, element
// widths, packed-config index widths, and dtype build flags. Consumed by the C
// encoder (libsauria_cfg) and any tool needing per-target constants.
#ifndef SAURIA_TARGETS_H
#define SAURIA_TARGETS_H

#include <cstdint>
#include <cstring>

namespace sauria {

enum SauriaDtype { SAURIA_DT_INT8 = 0, SAURIA_DT_FP16 = 1, SAURIA_DT_INT16 = 2 };

struct SauriaTarget {
    const char *name;
    int X, Y;                 // systolic array geometry
    int ia_w, ib_w, oc_w;     // element bit widths (act, wei, psum/out)
    int op_type;              // 0=int, 1=FP; also the PE arithmetic_type
    int idx_a, idx_w, idx_o;  // packed-config index widths (ACT/WEI/OUT)
    int in_bytes, out_bytes;  // DRAM element sizes (ia_w/8, oc_w/8)
    SauriaDtype dtype;
    const char *build_flags;  // dtype -D flags (IDX/EVAL derived from fields)
};

static const SauriaTarget SAURIA_TARGETS[] = {
    {"int8_8x16", 16, 8, 8, 8, 32, 0, 15, 16, 14, 1, 4, SAURIA_DT_INT8, ""},
    {"int8_32x32", 32, 32, 8, 8, 32, 0, 17, 17, 16, 1, 4, SAURIA_DT_INT8, ""},
    {"int8_64x64", 64, 64, 8, 8, 32, 0, 18, 18, 17, 1, 4, SAURIA_DT_INT8, ""},
    {"FP16_8x16", 16, 8, 16, 16, 16, 1, 15, 15, 15, 2, 2, SAURIA_DT_FP16, "-DNPU_FP16 -DNPU_DTYPE_IN=fp16_t -DNPU_DTYPE_OUT=fp16_t -DNPU_DTYPE_PSUM=float"},
    {"FP16_32x32", 32, 32, 16, 16, 16, 1, 17, 17, 16, 2, 2, SAURIA_DT_FP16, "-DNPU_FP16 -DNPU_DTYPE_IN=fp16_t -DNPU_DTYPE_OUT=fp16_t -DNPU_DTYPE_PSUM=float"},
    {"FP16_64x64", 64, 64, 16, 16, 16, 1, 18, 18, 17, 2, 2, SAURIA_DT_FP16, "-DNPU_FP16 -DNPU_DTYPE_IN=fp16_t -DNPU_DTYPE_OUT=fp16_t -DNPU_DTYPE_PSUM=float"},
    {"int16_8x16", 16, 8, 16, 16, 64, 0, 15, 16, 14, 2, 8, SAURIA_DT_INT16, "-DNPU_INT16 -DNPU_DTYPE_IN=int16_t -DNPU_DTYPE_OUT=int64_t -DNPU_DTYPE_PSUM=int64_t"},
    {"int16_32x32", 32, 32, 16, 16, 64, 0, 17, 17, 16, 2, 8, SAURIA_DT_INT16, "-DNPU_INT16 -DNPU_DTYPE_IN=int16_t -DNPU_DTYPE_OUT=int64_t -DNPU_DTYPE_PSUM=int64_t"},
};

static const int SAURIA_NUM_TARGETS = (int)(sizeof(SAURIA_TARGETS) / sizeof(SAURIA_TARGETS[0]));

// Look up a target by name; returns nullptr if unknown.
inline const SauriaTarget *sauria_find_target(const char *name) {
    for (int i = 0; i < SAURIA_NUM_TARGETS; i++)
        if (std::strcmp(SAURIA_TARGETS[i].name, name) == 0)
            return &SAURIA_TARGETS[i];
    return nullptr;
}

} // namespace sauria

#endif // SAURIA_TARGETS_H
