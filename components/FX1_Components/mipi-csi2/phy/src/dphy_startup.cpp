#include "fx1_csi2_phy/dphy_startup.h"

namespace fx1::csi2 {

DphyAdapterConfig DphyStartup::to_adapter(const DphyStartupConfig& c) {
    DphyAdapterConfig a;
    a.cfg_clk_period = c.cfg_clk_period;
    a.cfgclkfreqrange = c.cfgclkfreqrange;
    a.hsfreqrange = c.hsfreqrange;
    a.basedir = c.basedir;
    a.testclr_cycles = c.testclr_cycles;
    a.test_regs = c.test_regs;
    a.testclk_half_period = c.testclk_half_period;
    a.startup_timeout = c.timeout;
    return a;
}

DphyStartup::DphyStartup(sc_core::sc_module_name nm, DphyRxWrapper& phy, DphyStartupConfig cfg)
    : sc_core::sc_module(nm), cfg_(cfg), adapter_("adapter", phy, to_adapter(cfg)) {
    SC_THREAD(run);
}

void DphyStartup::run() {
    adapter_.set_control(true, cfg_.enableclk, cfg_.lane_enable, 0);
    unsigned target = 1;  // number of the adapter startup being waited for
    for (;;) {
        done_ = timed_out_ = false;
        bool reset = false;
        while (!(adapter_.startups() >= target && (adapter_.init_done() || adapter_.startup_timed_out()))) {
            wait(adapter_.status_event() | reset_req_ev_);
            if ((reset = reset_req_ev_.triggered())) break;
        }
        if (!reset) {
            done_ = adapter_.init_done();
            timed_out_ = adapter_.startup_timed_out();
            done_ev_.notify(sc_core::SC_ZERO_TIME);
            wait(reset_req_ev_);
        }
        target = adapter_.startups() + 1;
        adapter_.request_reset();
    }
}

}  // namespace fx1::csi2
