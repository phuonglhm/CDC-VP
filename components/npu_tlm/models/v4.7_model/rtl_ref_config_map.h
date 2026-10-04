// rtl_ref_config_map.h -- faithful port of ~/sauria_model/config_map.h (md5 differs from
// this checkout's own config_map.h: v1/v4 CON/WEI/OUT sub-register layout tables). Namespace
// sauria_rtl (not sauria) + its own header guard, exactly like sram/rtl_ref_sram_top.h's
// pattern, so it never collides with this checkout's own sauria::CfgField/config_map.h symbols
// even if both get included in the same translation unit.
//
// The CFG_*_OFFSET / ACT_XLIM / WEI_KLIM / ... constants this table indexes by are declared in
// sauria_types.h under namespace sauria, and are BYTE-IDENTICAL between this checkout and
// sauria_model (same offsets, same namespace in both trees' sauria_types.h) -- so `using namespace sauria;` below
// resolves them correctly without needing
// a separate ported sauria_types.h. Do NOT re-declare these constants here.
//
// Declarative config-register map. The address->field tables for BOTH profiles live here as
// data; ConfigRegs just looks up (profile, local_addr) -> field and stores it. v1 and v4
// diverged in the CON/WEI/OUT sub-register layout -- the same local address means different
// things per profile (e.g. CFG_OUT+0x10 = v1 ckstep vs v4 til_cylim; v1 puts NCONTEXTS at
// OUT+0x00 shifting the whole OUT region +0x04; v4 puts ncontexts/preload_en in the CON region).

#ifndef SAURIA_RTL_CONFIG_MAP_H
#define SAURIA_RTL_CONFIG_MAP_H

#include <cstdint>
#include <cstddef>
#include "sauria_types.h"
#include "npu_profile.h"

namespace sauria_rtl
{
    using namespace sauria;

    // Union of every config field used by either profile. ConfigRegs keeps a value per
    // field; ports are driven from these. ROWS_ACTIVE / DIL_PAT are bitfields handled
    // specially by ConfigRegs (the map only tags the address).
    enum CfgField : uint32_t
    {
        F_NONE = 0,

        // --- core (shared meaning across profiles) ---
        F_INCNTLIM, F_ACT_REPS, F_WEI_REPS, F_NCONTEXTS, F_PRELOAD_EN,
        F_ROWS_ACTIVE,
        F_ACT_INCNTLIM, F_ACT_INCNTSTEP, F_ACT_OUTCNTLIM, F_ACT_OUTCNTSTEP, F_DIL_PAT,
        F_WEI_INCNTLIM, F_WEI_INCNTSTEP,
        F_CXLIM, F_CXSTEP, F_CKLIM, F_CKSTEP,
        F_TIL_CYLIM, F_TIL_CYSTEP, F_TIL_CKLIM, F_TIL_CKSTEP,

        // --- v1 SAURIA-only extras ---
        F_ACT_XLIM, F_ACT_XSTEP, F_ACT_YLIM, F_ACT_YSTEP, F_ACT_CHLIM, F_ACT_CHSTEP,
        F_ACT_TIL_XLIM, F_ACT_TIL_XSTEP, F_ACT_TIL_YLIM, F_ACT_TIL_YSTEP,
        F_WEI_WLIM, F_WEI_WSTEP, F_WEI_KLIM, F_WEI_KSTEP, F_WEI_TIL_KLIM, F_WEI_TIL_KSTEP,
        F_WEI_COLS_ACTIVE, F_WEI_WALIGNED, F_INACTIVE_COLS,
        F_ACT_BASE_ADDR, F_WEI_BASE_ADDR, F_OUT_BASE_ADDR,
        F_IN_H, F_IN_W, F_IN_C, F_KERNEL_H, F_KERNEL_W, F_STRIDE, F_PADDING, F_DILATION,
        F_TILE_X, F_TILE_Y, F_TILE_K, F_TILE_C, F_X_USED, F_Y_USED,

        F_COUNT
    };

    struct RegMapEntry
    {
        uint32_t local_addr; // addr & ~SAURIA_MEM_ADDR_MASK within the CFG_REGS region
        CfgField field;
    };

    // -------------------- v4 profile (LINEAR) --------------------
    static const RegMapEntry V4_CFG_MAP[] = {
        // CON: v4 packs ncontexts/preload_en here
        {CFG_CON_OFFSET + 0x00, F_INCNTLIM},
        {CFG_CON_OFFSET + 0x04, F_ACT_REPS},
        {CFG_CON_OFFSET + 0x08, F_WEI_REPS},
        {CFG_CON_OFFSET + 0x0C, F_NCONTEXTS},
        {CFG_CON_OFFSET + 0x10, F_PRELOAD_EN},
        // ACT (identical to v1)
        {CFG_ACT_OFFSET + 0x00, F_ROWS_ACTIVE},
        {CFG_ACT_OFFSET + 0x04, F_ACT_INCNTLIM},
        {CFG_ACT_OFFSET + 0x08, F_ACT_INCNTSTEP},
        {CFG_ACT_OFFSET + 0x0C, F_ACT_OUTCNTLIM},
        {CFG_ACT_OFFSET + 0x10, F_ACT_OUTCNTSTEP},
        {CFG_ACT_OFFSET + 0x28, F_DIL_PAT},
        // WEI: v4 starts at +0x00
        {CFG_WEI_OFFSET + 0x00, F_WEI_INCNTLIM},
        {CFG_WEI_OFFSET + 0x04, F_WEI_INCNTSTEP},
        // OUT: v4 starts at +0x00 (no NCONTEXTS here)
        {CFG_OUT_OFFSET + 0x00, F_CXLIM},
        {CFG_OUT_OFFSET + 0x04, F_CXSTEP},
        {CFG_OUT_OFFSET + 0x08, F_CKLIM},
        {CFG_OUT_OFFSET + 0x0C, F_CKSTEP},
        {CFG_OUT_OFFSET + 0x10, F_TIL_CYLIM},
        {CFG_OUT_OFFSET + 0x14, F_TIL_CYSTEP},
        {CFG_OUT_OFFSET + 0x18, F_TIL_CKLIM},
        {CFG_OUT_OFFSET + 0x1C, F_TIL_CKSTEP},
    };

    // -------------------- v1 profile (SAURIA superset) --------------------
    static const RegMapEntry V1_CFG_MAP[] = {
        // CON
        {CFG_CON_OFFSET + 0x00, F_INCNTLIM},
        {CFG_CON_OFFSET + 0x04, F_ACT_REPS},
        {CFG_CON_OFFSET + 0x08, F_WEI_REPS},
        // ACT (shared subset)
        {CFG_ACT_OFFSET + 0x00, F_ROWS_ACTIVE},
        {CFG_ACT_OFFSET + 0x04, F_ACT_INCNTLIM},
        {CFG_ACT_OFFSET + 0x08, F_ACT_INCNTSTEP},
        {CFG_ACT_OFFSET + 0x0C, F_ACT_OUTCNTLIM},
        {CFG_ACT_OFFSET + 0x10, F_ACT_OUTCNTSTEP},
        {CFG_ACT_OFFSET + 0x28, F_DIL_PAT},
        // ACT SAURIA addr-gen
        {ACT_XLIM, F_ACT_XLIM},   {ACT_XSTEP, F_ACT_XSTEP},
        {ACT_YLIM, F_ACT_YLIM},   {ACT_YSTEP, F_ACT_YSTEP},
        {ACT_CHLIM, F_ACT_CHLIM}, {ACT_CHSTEP, F_ACT_CHSTEP},
        {ACT_TIL_XLIM, F_ACT_TIL_XLIM}, {ACT_TIL_XSTEP, F_ACT_TIL_XSTEP},
        {ACT_TIL_YLIM, F_ACT_TIL_YLIM}, {ACT_TIL_YSTEP, F_ACT_TIL_YSTEP},
        // WEI: v1 starts at +0x04
        {CFG_WEI_OFFSET + 0x04, F_WEI_INCNTLIM},
        {CFG_WEI_OFFSET + 0x08, F_WEI_INCNTSTEP},
        {WEI_WLIM, F_WEI_WLIM},   {WEI_WSTEP, F_WEI_WSTEP},
        {WEI_KLIM, F_WEI_KLIM},   {WEI_KSTEP, F_WEI_KSTEP},
        {WEI_TIL_XLIM, F_WEI_TIL_KLIM}, {WEI_TIL_XSTEP, F_WEI_TIL_KSTEP},
        {WEI_COLS_ACTIVE, F_WEI_COLS_ACTIVE}, {WEI_WALIGNED, F_WEI_WALIGNED},
        // OUT: v1 has NCONTEXTS at +0x00, the rest shifted +0x04
        {NCONTEXTS, F_NCONTEXTS},
        {CFG_OUT_OFFSET + 0x04, F_CXLIM},
        {CFG_OUT_OFFSET + 0x08, F_CXSTEP},
        {CFG_OUT_OFFSET + 0x0C, F_CKLIM},
        {CFG_OUT_OFFSET + 0x10, F_CKSTEP},
        {TIL_CYLIM, F_TIL_CYLIM}, {TIL_CYSTEP, F_TIL_CYSTEP},
        {TIL_CKLIM, F_TIL_CKLIM}, {TIL_CKSTEP, F_TIL_CKSTEP},
        {INACTIVE_COLS, F_INACTIVE_COLS}, {PRELOAD_EN, F_PRELOAD_EN},
        // base addresses + layer descriptor (v1 only)
        {CFG_ACT_BASE_ADDR, F_ACT_BASE_ADDR},
        {CFG_WEI_BASE_ADDR, F_WEI_BASE_ADDR},
        {CFG_OUT_BASE_ADDR, F_OUT_BASE_ADDR},
        {IN_H, F_IN_H}, {IN_W, F_IN_W}, {IN_C, F_IN_C},
        {KERNEL_H, F_KERNEL_H}, {KERNEL_W, F_KERNEL_W},
        {STRIDE, F_STRIDE}, {PADDING, F_PADDING}, {DILATION, F_DILATION},
        {TILE_X, F_TILE_X}, {TILE_Y, F_TILE_Y}, {TILE_K, F_TILE_K}, {TILE_C, F_TILE_C},
        {X_USED, F_X_USED}, {Y_USED, F_Y_USED},
    };

    // (profile, local_addr) -> field. Returns F_NONE if the address is not mapped.
    inline CfgField cfg_lookup(NpuProfile prof, uint32_t local_addr)
    {
        const RegMapEntry *map;
        size_t n;
        if (prof == PROFILE_V4_LINEAR)
        {
            map = V4_CFG_MAP;
            n = sizeof(V4_CFG_MAP) / sizeof(RegMapEntry);
        }
        else
        {
            map = V1_CFG_MAP;
            n = sizeof(V1_CFG_MAP) / sizeof(RegMapEntry);
        }
        for (size_t i = 0; i < n; i++)
        {
            if (map[i].local_addr == local_addr)
                return map[i].field;
        }
        return F_NONE;
    }
} // namespace sauria_rtl

#endif // SAURIA_RTL_CONFIG_MAP_H
