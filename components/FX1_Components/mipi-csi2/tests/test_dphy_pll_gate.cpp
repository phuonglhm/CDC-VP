// P13: the wrapper suspends the D-PHY model's PLL core while the PLL is off.
// Delta cycles are deterministic, so the cost of an idle PHY is checked by
// count, not by wall time.
//
//   test_fx1_dphy_pll_gate on    gating (default): PLL core suspended, idle cheap
//   test_fx1_dphy_pll_gate off   control, pll_gating = false: the free-running
//                                VCO costs ~100x the delta cycles
#include <cstdio>
#include <string>

#include <systemc>

#include "fx1_csi2_phy/dphy_rx_wrapper.h"

int sc_main(int argc, char** argv) {
    sc_core::sc_set_time_resolution(1, sc_core::SC_FS);
    const std::string which = argc > 1 ? argv[1] : "on";
    fx1::csi2::DphyRxWrapper phy("phy");
    phy.pll_gating = which != "off";
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_US));
    const auto d0 = sc_core::sc_delta_count();
    sc_core::sc_start(sc_core::sc_time(100, sc_core::SC_US));
    const auto deltas = sc_core::sc_delta_count() - d0;
    std::printf("%s: %llu delta cycles in 100 us idle, PLL core %s\n", which.c_str(), (unsigned long long)deltas,
                phy.pll_gated() ? "suspended" : "running");
    // Measured: about 10.5k gated, about 1.06M with the VCO running.
    const bool ok = which == "off" ? (!phy.pll_gated() && deltas > 500000) : (phy.pll_gated() && deltas < 50000);
    std::printf("%s: %s\n", which.c_str(), ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
