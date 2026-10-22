// D-PHY model bring-up tests: elaboration of the wrapper and the
// model-derived startup sequence with the lanes held in LP-11.
//
//   test_fx1_dphy_startup elaborate   bind all ports, run zero time
//   test_fx1_dphy_startup startup     power-up/reset until the power-control
//                                     FSM reaches state 7 with lanes enabled
//   test_fx1_dphy_startup reset       startup, reset, startup again
//   test_fx1_dphy_startup config_fault  negative test: a test-register write
//                                     that cannot read back (address 0xFF is
//                                     not decoded by the tester) must be
//                                     detected and turn the verdict into FAIL
//
// Pass criteria are observations of the model (FSM state, stop-state and
// direction outputs, absence of X/Z on them), not values computed from the
// fixture itself.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>

#include <systemc>

#include "fx1_csi2_phy/dphy_rx_wrapper.h"
#include "fx1_csi2_phy/dphy_startup.h"

using namespace fx1::csi2;

namespace {
int g_fail = 0;
#define CHECK(cond)                                                                         \
    do {                                                                                    \
        if (!(cond)) {                                                                      \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond);    \
            ++g_fail;                                                                       \
        }                                                                                   \
    } while (0)

// Holds every lane pad in LP-11 with pull strength (README driving rule).
struct LpIdle : sc_core::sc_module {
    SC_HAS_PROCESS(LpIdle);
    LpIdle(sc_core::sc_module_name nm, DphyRxWrapper& w) : sc_core::sc_module(nm), w_(w) { SC_METHOD(drive); }
    void drive() {
        vl::wr_s(w_.clkp, vl::L1(), vl::ST_PULL);
        vl::wr_s(w_.clkn, vl::L1(), vl::ST_PULL);
        for (int i = 0; i < 4; ++i) {
            vl::wr_s(w_.dp[i], vl::L1(), vl::ST_PULL);
            vl::wr_s(w_.dn[i], vl::L1(), vl::ST_PULL);
        }
    }
    DphyRxWrapper& w_;
};

const char* lstr(const sc_dt::sc_logic& v) { return v == sc_dt::SC_LOGIC_1 ? "1" : v == sc_dt::SC_LOGIC_0 ? "0" : v == sc_dt::SC_LOGIC_Z ? "Z" : "X"; }

void report_outputs(const DphyRxWrapper& w) {
    std::printf("  stopstateclk=%s stopstatedata=%s%s%s%s direction=%s rxclkactivehs=%s rxulpsclknot=%s rxskewcalhs=%s\n",
                lstr(w.stopstateclk.read()), lstr(w.stopstatedata[0].read()), lstr(w.stopstatedata[1].read()),
                lstr(w.stopstatedata[2].read()), lstr(w.stopstatedata[3].read()), lstr(w.direction.read()),
                lstr(w.rxclkactivehs.read()), lstr(w.rxulpsclknot.read()), lstr(w.rxskewcalhs.read()));
    for (int i = 0; i < 4; ++i)
        std::printf("  lane%d: rxvalidhs=%s rxactivehs=%s rxsynchs=%s errsoths=%s errsotsynchs=%s errcontrol=%s rxulpsesc=%s\n", i,
                    lstr(w.rxvalidhs[i].read()), lstr(w.rxactivehs[i].read()), lstr(w.rxsynchs[i].read()),
                    lstr(w.errsoths[i].read()), lstr(w.errsotsynchs[i].read()), lstr(w.errcontrol[i].read()),
                    lstr(w.rxulpsesc[i].read()));
}

void check_idle_outputs(const DphyRxWrapper& w, unsigned lane_mask) {
    CHECK(w.stopstateclk.read() == sc_dt::SC_LOGIC_1);
    CHECK(w.direction.read() == sc_dt::SC_LOGIC_1);
    CHECK(w.rxclkactivehs.read() == sc_dt::SC_LOGIC_0);
    for (int i = 0; i < 4; ++i) {
        if (!(lane_mask >> i & 1)) continue;
        CHECK(w.stopstatedata[i].read() == sc_dt::SC_LOGIC_1);
        CHECK(w.rxvalidhs[i].read() == sc_dt::SC_LOGIC_0);
        CHECK(w.rxactivehs[i].read() == sc_dt::SC_LOGIC_0);
    }
}
}  // namespace

int sc_main(int argc, char** argv) {
    sc_core::sc_set_time_resolution(1, sc_core::SC_FS);
    const std::string which = argc > 1 ? argv[1] : "startup";
    sc_core::sc_report_handler::set_actions(sc_core::SC_WARNING, sc_core::SC_DISPLAY);
    const auto t0 = std::chrono::steady_clock::now();

    DphyRxWrapper phy("dut");
    if (which == "elaborate") {
        sc_core::sc_start(sc_core::SC_ZERO_TIME);
        std::printf("elaborate: PASS (t=%s)\n", sc_core::sc_time_stamp().to_string().c_str());
        return 0;
    }

    DphyStartupConfig cfg;
    if (which == "config_fault") cfg.test_regs.emplace_back(0xFF, 0xA5);
    // Debug only (not used by ctest): shorten the startup timeout to check
    // that a timed-out startup fails every case.
    if (const char* t = std::getenv("FX1_DPHY_STARTUP_TIMEOUT_US"))
        cfg.timeout = sc_core::sc_time(std::stod(t), sc_core::SC_US);
    DphyStartup st("startup", phy, cfg);
    LpIdle idle("lp_idle", phy);

    auto run_until_done = [&](const char* tag) {
        while (!st.done() && !st.timed_out() && sc_core::sc_time_stamp() < st.release_time() + cfg.timeout + sc_core::sc_time(1, sc_core::SC_US))
            sc_core::sc_start(10, sc_core::SC_US);
        std::printf("%s: release at %.3f us, %s at %.3f us (%.3f us after release)\n", tag,
                    st.release_time().to_seconds() * 1e6, st.done() ? "done" : "TIMEOUT",
                    sc_core::sc_time_stamp().to_seconds() * 1e6,
                    ((st.done() ? st.done_time() : sc_core::sc_time_stamp()) - st.release_time()).to_seconds() * 1e6);
        std::printf("FSM trace:\n%s", st.fsm_trace_text().c_str());
    };

    run_until_done("startup");
    CHECK(st.done());
    if (which == "config_fault") {
        // Pass only if the fixture caught exactly the injected mismatch; the
        // normal startup verdict (below) must then be FAIL.
        // Any other failed CHECK (e.g. startup timeout) also fails the case.
        const bool detected = st.test_reg_mismatches() == 1 && !st.config_ok();
        const bool pass = detected && g_fail == 0;
        std::printf("config_fault: mismatches=%u -> %s, other failures=%d\n", st.test_reg_mismatches(),
                    detected ? "detected" : "NOT detected", g_fail);
        std::printf("config_fault: %s\n", pass ? "PASS" : "FAIL");
        return pass ? 0 : 1;
    }
    CHECK(st.config_ok());
    // Let the lane state machines settle in LP-11 before sampling outputs.
    sc_core::sc_start(5, sc_core::SC_US);
    report_outputs(phy);
    check_idle_outputs(phy, cfg.lane_enable);

    if (which == "reset") {
        const size_t mark = st.fsm_trace().size();
        st.request_reset();
        sc_core::sc_start(1, sc_core::SC_US);
        run_until_done("reset_recovery");
        CHECK(st.done());
        CHECK(st.config_ok());  // test registers rewritten after the second testclr
        bool left7 = false;
        for (size_t i = mark; i < st.fsm_trace().size(); ++i) left7 |= st.fsm_trace()[i].second != 7;
        CHECK(left7);  // the reset really restarted the FSM
        sc_core::sc_start(5, sc_core::SC_US);
        report_outputs(phy);
        check_idle_outputs(phy, cfg.lane_enable);
    }

    const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("sim_time=%.3f us wall=%.2f s\n", sc_core::sc_time_stamp().to_seconds() * 1e6, wall);
    std::printf("%s: %s\n", which.c_str(), g_fail ? "FAIL" : "PASS");
    return g_fail ? 1 : 0;
}
