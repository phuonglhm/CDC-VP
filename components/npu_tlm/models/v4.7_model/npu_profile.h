// SAURIA NPU - Unified model
// Runtime PROFILE selector: one binary runs both the v1 (SAURIA bit-exact) and the
// v4 (legacy LINEAR) testbench suites, chosen at runtime by a config register.
//
// The driver/testbench writes the PROFILE register FIRST (before any other config),
// so config_regs knows which address map to use and the modules know which behavior
// (LINEAR vs SAURIA addressing) to apply. Delivered downstream as o_profile.

#ifndef SAURIA_UNIFIED_NPU_PROFILE_H
#define SAURIA_UNIFIED_NPU_PROFILE_H

#include <cstdint>

namespace sauria
{
    enum NpuProfile : uint32_t
    {
        PROFILE_V1_SAURIA = 0, // full SAURIA: im2col/dilation addr-gen, tiling, bit-exact
        PROFILE_V4_LINEAR = 1  // legacy v4: linear feeders (addr_reg += incntstep) + v4 reg map
    };

    // PROFILE register: local address inside the CFG_REGS region (below CFG_CON_OFFSET=0x200,
    // so it never collides with CON/ACT/WEI/OUT/LAYER sub-registers). Identical in both maps.
    // Host write: addr = CORE_base | CFG_PROFILE_ADDR, data = NpuProfile value.
    //
    // Single source of truth, OVERRIDABLE at build time so the SW team can relocate it to
    // match the real hardware memory map without editing this header:
    //   g++ ... -DSAURIA_CFG_PROFILE_ADDR=0x000000F0
#ifndef SAURIA_CFG_PROFILE_ADDR
#define SAURIA_CFG_PROFILE_ADDR 0x00000004u
#endif
    static const uint32_t CFG_PROFILE_ADDR = SAURIA_CFG_PROFILE_ADDR;

    // Default profile when the PROFILE register is never written: keep current v1 (SAURIA)
    // semantics so existing v1 testbenches behave identically. Overridable at build time.
#ifndef SAURIA_DEFAULT_PROFILE
#define SAURIA_DEFAULT_PROFILE PROFILE_V1_SAURIA
#endif

    inline const char *profile_name(NpuProfile p)
    {
        return (p == PROFILE_V4_LINEAR) ? "V4_LINEAR" : "V1_SAURIA";
    }
}

#endif // SAURIA_UNIFIED_NPU_PROFILE_H
