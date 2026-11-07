// tb_has_profile.cpp -- checks the run profiles of has::HasNpuTop: the default constructor gives the Recommended profile,
// each profile sets the expected options and knobs, and the Knobs constructor leaves every option off.
// The profiles neo and neo_as_rtl (has/has_profile_neo.h) set environment variables that the parts read once, so each is checked in a
// process of its own:
//   tb_has_profile                run the checks of the older profiles (HAS_PROFILE must not be set)
//   tb_has_profile --neo          HAS_PROFILE=neo set in the process; the default constructor gives Neo
//   tb_has_profile --neo-as-rtl   HAS_PROFILE=neo_as_rtl
//   tb_has_profile --neo-override HAS_PROFILE=neo with HAS_GVU_RESIDUAL_HW=0 and HAS_DMA_ND=1 set by the caller: they win over the table
// Build (from the repository root):
//   g++ -std=c++17 -O1 -DSAURIA_DEBUG=0 -DSAURIA_CORE_BACKEND_RTL_REF -DFX1_A3_SRAM_BACKDOOR_LOAD -I. -I/usr/include \
//       tools/has/tb_has_profile.cpp -lsystemc -o tools/has/tb_has_profile
#include <systemc.h>
#include <cstdio>
#include <cstring>
#include <string>
#include "has/has_npu_top.h"

using namespace has;

static int fails = 0;
static void check(bool ok, const char *what)
{
    std::printf("  %s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}

// the variables of a profile table must be in the environment with the table value
static void check_table(const profile_neo::EnvKnob *t, size_t n, bool skip_caller_set)
{
    for (size_t i = 0; i < n; i++)
    {
        if (skip_caller_set && (!std::strcmp(t[i].name, "HAS_GVU_RESIDUAL_HW") || !std::strcmp(t[i].name, "HAS_DMA_ND"))) continue;
        const char *e = std::getenv(t[i].name);
        const std::string what = std::string(t[i].name) + "=" + t[i].value;
        check(e && !std::strcmp(e, t[i].value), what.c_str());
    }
}

static int check_neo(const std::string &mode)
{
    const bool as_rtl = mode == "--neo-as-rtl", over = mode == "--neo-override";
    profile_neo::set_env("HAS_PROFILE", as_rtl ? "neo_as_rtl" : "neo");
    if (over) { profile_neo::set_env("HAS_GVU_RESIDUAL_HW", "0"); profile_neo::set_env("HAS_DMA_ND", "1"); }
    std::printf("[tb_has_profile] %s\n", mode.c_str());
    check(resolve_profile(Profile::Recommended) == (as_rtl ? Profile::NeoAsRtl : Profile::Neo), "resolve_profile(Recommended) follows HAS_PROFILE");
    check(resolve_profile(Profile::Legacy) == Profile::Legacy && resolve_profile(Profile::Proposals) == Profile::Proposals, "Legacy / Proposals are never changed");
    HasNpuTop dut("dut");   // the default constructor: Recommended -> neo profile
    check(dut.overlap() && dut.obp_inline() && dut.tile_order_auto && dut.rce_core, "options: overlap, inline epilogue, S4 order, attention on the core");
    check(!dut.desc3d && !dut.c_bias_bcast && !dut.halo_reuse, "options: no 3-D descriptors, no proposals");
    check(dut.knobs.sp_banked == 1 && dut.knobs.lat_obp == 10, "knobs: banked scratchpad, vector-unit latencies");
    check(dut.dma_nd == over && !dut.out_transpose && !dut.dma_nd_merge && !dut.fa_overlap && !dut.feeder_pad && dut.bus_strict,
          "members: dma_nd as the caller / table, no transpose / merge / overlap / feeder pad, bus strict on");
    check(dut.mmio.dma_nd == over && !dut.mmio.dma_tensor, "front end: slice / concat follow HAS_DMA_ND, COPY off");
    check_table(profile_neo::kNeoKnobs, sizeof(profile_neo::kNeoKnobs) / sizeof(profile_neo::kNeoKnobs[0]), over);
    if (as_rtl) check_table(profile_neo::kNeoAsRtlExtra, sizeof(profile_neo::kNeoAsRtlExtra) / sizeof(profile_neo::kNeoAsRtlExtra[0]), false);
    // what the parts have cached
    check(neo_rev() == 1 && GvuRegsNeoKnobs::on() && GvuRegsNeoKnobs::pp_on() && GvuRegsNeoKnobs::cdc_lat() == 4, "parts: REV 1, neo register map, ping-pong, CDC 4");
    check(rtl_ref_gvu::vbank1_depth_default() == 200 && DmaDfc1d::on() && DmaDfc1d::out_vbank2(), "parts: vbank1 200, 1-D DFC commands, result through vbank2");
    check(rtl_ref_gvu::ResidualHw::on() == !over, over ? "residual: the caller's HAS_GVU_RESIDUAL_HW=0 wins" : "residual: RESIDUAL unit on");
    check(!GvuTopStream::fa_on() && !GvuTopStream::ln_on() && !GvuTopStream::avg_on() && GvuTopBridge::agu_nd_level() == 0, "parts: FA, LN, AVG, AGU_ND off");
    check(!rtl_ref_gvu::ResidualHw::logical(), "residual shift is arithmetic (as in revision 1)");
    if (as_rtl)
    {
        check(dut.knobs.round_mode == HALF_AWAY && dut.knobs.req_narrow == WRAP16 && dut.knobs.zp_add_wrap16 == 1,
              "knobs: half away from zero, wrapping narrowing, wrapping zero-point adder");
        check(GvuTopStream::softmax_as_rtl() && RtlArith::on(), "parts: Softmax stage 3 as delivered, arithmetic through the ported units");
    }
    else
        check(dut.knobs.round_mode == HALF_UP && dut.knobs.req_narrow == SAT16 && !GvuTopStream::softmax_as_rtl() && !RtlArith::on(), "neo: rounding and Softmax as the model");
    check(verify_env_profile(stdout) == 0, "verify_env_profile: no part read its knob before the profile");
    HasNpuTop explicit_neo("explicit_neo", as_rtl ? Profile::NeoAsRtl : Profile::Neo);
    check(!explicit_neo.desc3d && explicit_neo.overlap(), "explicit Profile constructor gives the same options");
    std::printf("[tb_has_profile] RESULT: %s (%d failed checks)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}

int sc_main(int argc, char *argv[])
{
    sc_report_handler::set_actions("/IEEE_Std_1666/deprecated", SC_DO_NOTHING);
    if (argc > 1)
    {
        const std::string m = argv[1];
        if (m == "--neo" || m == "--neo-as-rtl" || m == "--neo-override") return check_neo(m);
        std::fprintf(stderr, "usage: %s [--neo | --neo-as-rtl | --neo-override]\n", argv[0]);
        return 2;
    }
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

    std::printf("[tb_has_profile] neo profiles without HAS_PROFILE\n");
    check(!std::getenv("HAS_PROFILE"), "HAS_PROFILE is not set (run this part without it)");
    check(resolve_profile(Profile::Recommended) == Profile::Recommended, "no HAS_PROFILE: Recommended stays");
    check(profile_neo::parse("neo") == 1 && profile_neo::parse("neo_as_rtl") == 2 && profile_neo::parse("") == 0 && profile_neo::parse("x") == -1, "profile names");
    check(apply_env_profile("bogus") == -1 && apply_env_profile(nullptr) == 0, "unknown profile name refused, none = nothing");
    check(std::string(profile_name(Profile::Neo)) == "neo" && std::string(profile_name(Profile::NeoAsRtl)) == "neo_as_rtl", "profile names printed");

    std::printf("[tb_has_profile] RESULT: %s (%d failed checks)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
