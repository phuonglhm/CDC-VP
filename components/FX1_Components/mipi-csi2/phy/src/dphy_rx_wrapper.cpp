#include "fx1_csi2_phy/dphy_rx_wrapper.h"

#include <string>

#include "mipirxphy.h"

namespace fx1::csi2 {

// Tie-offs and sinks of the ports that are not part of the wrapper interface.
struct DphyRxWrapper::Private {
#define FX1_DPHY_WRAPPER_DECL
#include "dphy_rx_wrapper_bind.inc"
#undef FX1_DPHY_WRAPPER_DECL
};

DphyRxWrapper::DphyRxWrapper(sc_core::sc_module_name nm)
    : sc_core::sc_module(nm), t_(new Private), phy_(new mipirxphy("phy")) {
    if (sc_core::sc_get_time_resolution() != sc_core::sc_time(1, sc_core::SC_FS))
        SC_REPORT_ERROR("fx1/csi2/dphy", "the D-PHY model requires sc_set_time_resolution(1, SC_FS)");
    auto& t = *t_;
    auto* phy = phy_.get();
#define FX1_DPHY_WRAPPER_BIND
#include "dphy_rx_wrapper_bind.inc"
#undef FX1_DPHY_WRAPPER_BIND
    SC_THREAD(pll_gate_thread);
}

namespace {
// PLL core model instance inside the D-PHY and its enable (mipi_pll_dphy_mpll_ana.cpp:168).
constexpr const char* kPllCore = ".phy.u_mipi_pll_dphy.I01DPHYPLL.I242.ana.I01PLLCOREMODEL";
constexpr const char* kPllOn = ".phy.u_mipi_pll_dphy.I01DPHYPLL.I242.ana.c_I01PLLCOREMODEL_on";

void collect_processes(sc_core::sc_object* o, std::vector<sc_core::sc_process_handle>& out) {
    for (auto* c : o->get_child_objects()) {
        const std::string k = c->kind();
        if (k == "sc_method_process" || k == "sc_thread_process" || k == "sc_cthread_process")
            out.emplace_back(c);
        collect_processes(c, out);
    }
}
}  // namespace

void DphyRxWrapper::start_of_simulation() {
    if (!pll_gating) return;
    auto* core = sc_core::sc_find_object((std::string(name()) + kPllCore).c_str());
    pll_on_ = dynamic_cast<sc_core::sc_signal_resolved*>(sc_core::sc_find_object((std::string(name()) + kPllOn).c_str()));
    if (core) collect_processes(core, pll_procs_);
    if (!core || !pll_on_ || pll_procs_.empty()) {
        SC_REPORT_WARNING("fx1/csi2/dphy", "PLL core not found in the D-PHY model: PLL gating (P13) off, idle simulation is slow");
        pll_procs_.clear();
        return;
    }
    gate_armed_ = true;
}

// After every time-0 evaluation (the VCO's first edge is 1.8 ns away), then on
// each change of the PLL enable.
void DphyRxWrapper::pll_gate_thread() {
    if (!gate_armed_) return;
    wait(1, sc_core::SC_PS);
    for (;;) {
        apply_pll_gate();
        wait(pll_on_->value_changed_event());
    }
}

void DphyRxWrapper::apply_pll_gate() {
    const bool off = pll_on_->read() != sc_dt::SC_LOGIC_1;
    if (off == gated_) return;
    for (auto& p : pll_procs_) {
        if (off) p.suspend();
        else p.resume();
    }
    if (!off)
        SC_REPORT_WARNING("fx1/csi2/dphy", "PLL enabled in the D-PHY model: PLL gating (P13) lifted, idle simulation is slow");
    gated_ = off;
}

DphyRxWrapper::~DphyRxWrapper() = default;

}  // namespace fx1::csi2
