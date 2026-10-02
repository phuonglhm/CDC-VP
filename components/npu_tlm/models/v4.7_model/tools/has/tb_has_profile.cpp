// tb_has_profile.cpp -- checks the run profiles of has::HasNpuTop: the default constructor gives the Recommended profile,
// each profile sets the expected options and knobs, and the Knobs constructor leaves every option off.
// Build (from the repository root):
//   g++ -std=c++17 -O1 -DSAURIA_DEBUG=0 -DSAURIA_CORE_BACKEND_RTL_REF -DFX1_A3_SRAM_BACKDOOR_LOAD -I. -I/usr/include \
//       tools/has/tb_has_profile.cpp -lsystemc -o tools/has/tb_has_profile
#include <systemc.h>
#include <cstdio>
#include "has/has_npu_top.h"

using namespace has;

static int fails = 0;
static void check(bool ok, const char *what)
{
    std::printf("  %s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}

int sc_main(int, char *[])
{
    sc_report_handler::set_actions("/IEEE_Std_1666/deprecated", SC_DO_NOTHING);
    HasNpuTop def("def");
    HasNpuTop rec("rec", Profile::Recommended);
    HasNpuTop pro("pro", Profile::Proposals);
    HasNpuTop leg("leg", Profile::Legacy);
    HasNpuTop raw("raw", Knobs());

    std::printf("[tb_has_profile] default constructor\n");
    check(def.overlap() && def.obp_inline() && def.desc3d && def.tile_order_auto && def.rce_core, "default = recommended options");
    check(!def.c_bias_bcast && !def.halo_reuse, "default: proposals off");
    check(def.knobs.sp_banked == 1 && def.knobs.lat_obp == 10 && def.knobs.lat_add1 == 8 && def.knobs.lat_add2 == 10,
          "default: banked scratchpad, vector-unit latencies");

    std::printf("[tb_has_profile] recommended / proposals / legacy\n");
    check(rec.overlap() && rec.obp_inline() && rec.desc3d && rec.tile_order_auto && rec.rce_core && !rec.c_bias_bcast &&
          !rec.halo_reuse, "recommended options");
    check(pro.overlap() && pro.obp_inline() && pro.desc3d && pro.tile_order_auto && pro.rce_core && pro.c_bias_bcast &&
          pro.halo_reuse, "proposals options");
    check(!leg.overlap() && !leg.obp_inline() && !leg.desc3d && !leg.tile_order_auto && !leg.rce_core && !leg.c_bias_bcast &&
          !leg.halo_reuse && leg.knobs.sp_banked == 0 && leg.knobs.lat_obp == 6, "legacy options and knobs");

    std::printf("[tb_has_profile] Knobs constructor\n");
    check(!raw.overlap() && !raw.obp_inline() && !raw.desc3d && !raw.tile_order_auto && !raw.rce_core, "all options off");

    std::printf("[tb_has_profile] RESULT: %s (%d failed checks)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
