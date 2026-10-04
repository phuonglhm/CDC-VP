// Offline tool (no SystemC): uses sauria_model's plain C++ encoder (driver/libsauria_cfg.h, read-only copy in
// tools/sauria_ref/) to compute the EXACT register set of a simple real GEMM/conv shape, instead of deriving
// registers one by one by hand.
//
// Shape: 1x1 pointwise conv, 8 real input channels (input channel = reduction depth), 1 output channel, 1 spatial
// tile, X_used = Y_used = 1 (the simplest self-consistent real configuration).
#include <cstdio>
#include <cstring>
#include "sauria_ref/sauria_targets.h"
#include "sauria_ref/sauria_cfg_layout.h"
#include "sauria_ref/libsauria_cfg.h"

using namespace sauria;

int main()
{
    const SauriaTarget *t = nullptr;
    for (int i = 0; i < SAURIA_NUM_TARGETS; i++)
        if (std::strcmp(SAURIA_TARGETS[i].name, "int8_32x32") == 0) t = &SAURIA_TARGETS[i];

    SauriaLayerDesc d{};
    d.B_w = 1; d.B_h = 1;
    d.d = 1; d.s = 1;
    d.c_til = 8;      // K=8 REAL input channels (reduction depth)
    d.k_til = 1;      // 1 output channel
    d.h_til = 2; d.w_til = 1; // h_til=2: two consecutive real spatial tiles (wei_reps=2)
    d.X_used = 1; d.Y_used = 1;
    d.preload_en = 0;
    d.C_w = 0; d.C_h = 0; d.C_c = 0; d.A_c = 0; // = tile (no further tiling)

    uint64_t f[F_CFG_COUNT];
    sauria_compute_core_fields(d, *t, f);

#define P(name) std::printf("  %-16s = %llu\n", #name, (unsigned long long)f[F_CFG_##name])
    std::printf("=== CONTROL ===\n");
    P(INCNTLIM); P(ACT_REPS); P(WEI_REPS); P(THRES);
    std::printf("=== ACTIVATION ===\n");
    P(XLIM); P(XSTEP); P(YLIM); P(YSTEP); P(CHLIM); P(CHSTEP);
    P(TIL_XLIM); P(TIL_XSTEP); P(TIL_YLIM); P(TIL_YSTEP);
    std::printf("  DIL_PAT (hex) = %llx\n", (unsigned long long)f[F_CFG_DIL_PAT]);
    std::printf("  ROWS_ACTIVE (hex) = %llx\n", (unsigned long long)f[F_CFG_ROWS_ACTIVE]);
    std::printf("=== WEIGHT ===\n");
    P(WLIM); P(WSTEP); P(KLIM); P(KSTEP); P(TIL_KLIM); P(TIL_KSTEP);
    std::printf("  COLS_ACTIVE (hex) = %llx\n", (unsigned long long)f[F_CFG_COLS_ACTIVE]);
    P(WALIGNED);
    std::printf("=== OUTPUT ===\n");
    P(NCONTEXTS); P(CXLIM); P(CXSTEP); P(CKLIM); P(CKSTEP);
    P(TIL_CYLIM); P(TIL_CYSTEP); P(TIL_CKLIM); P(TIL_CKSTEP);
    P(INACTIVE_COLS); P(PRELOAD_EN);
#undef P
    return 0;
}
