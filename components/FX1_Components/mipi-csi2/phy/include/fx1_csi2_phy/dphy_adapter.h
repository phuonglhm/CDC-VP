// Control side of the PHY adapter: drives every control input of the
// mipirxphy model from the controller's PHY_CONFIG state and the PHY reset
// sequencer, and reports PHY readiness.
//
// Contract: docs/PHY_ADAPTER_CONTRACT.md §3 (startup), spec_decisions P04, P07,
// P08–P10. The adapter is the only writer of the wrapper control signals
// (cfg_clk, rstz, shutdownz, testclr, testclk, testen, testdin, enableclk,
// enable, cfgclkfreqrange, hsfreqrange, basedir, forcerxmode); callers in any
// process change its state through the methods below, which take effect in
// the adapter's own thread at the current simulation time.
//
//  phy_enable = 0      PHY held in shutdown (shutdownz = rstz = 0), cfg_clk stopped
//  phy_enable 0 -> 1   full model-derived startup with the lane enables of that moment
//  request_reset()     reset pulse max(min_reset_pulse, 4 cfg_clk); then startup again
//                      if phy_enable = 1
//  lane disabled while running   applied at once
//  lane enabled while running    not applied (the PHY calibrates only the lanes
//                                enabled at startup); warning, effective after the
//                                next reset or phy_enable 0 -> 1
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <systemc>

#include "fx1_csi2_phy/dphy_rx_wrapper.h"

namespace fx1::csi2 {

struct DphyAdapterConfig {
    sc_core::sc_time cfg_clk_period{40, sc_core::SC_NS};  // 25 MHz
    uint8_t cfgclkfreqrange = 0x20;   // 4 * 25 - 68 (contract §2.2)
    uint8_t hsfreqrange = 0x09;       // contract §2.3
    bool basedir = true;              // lane 0 must be a receiver (contract §4.1)
    unsigned testclr_cycles = 4;      // testclr has no minimum width in the model
    // Test-interface writes {address, value} after every testclr, each read
    // back through testdout. 0x09 = 0x10: noalign_error, per-lane SoT (P07).
    std::vector<std::pair<uint8_t, uint8_t>> test_regs{{0x09, 0x10}};
    sc_core::sc_time testclk_half_period{50, sc_core::SC_NS};
    sc_core::sc_time min_reset_pulse{200, sc_core::SC_NS};  // P04
    sc_core::sc_time startup_timeout{2, sc_core::SC_MS};    // for startup_timed_out()
};

class DphyAdapter : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(DphyAdapter);
    DphyAdapter(sc_core::sc_module_name nm, DphyRxWrapper& phy, DphyAdapterConfig cfg = {});

    // ---- control (callable from any process)
    // dl_mask bit n = data lane n; forcerx_mask likewise.
    void set_control(bool phy_enable, bool cl_enable, uint8_t dl_mask, uint8_t forcerx_mask);
    void request_reset();

    // ---- status
    bool reset_pulse_active() const { return pulse_active_; }
    // Power-control FSM in state 7 with lanes enabled (observability only, P05).
    bool init_done() const;
    bool startup_timed_out() const { return timed_out_; }
    bool config_ok() const { return test_reg_mismatches_ == 0; }
    unsigned test_reg_mismatches() const { return test_reg_mismatches_; }
    uint8_t applied_lane_mask() const { return applied_dl_; }   // lanes enabled in the PHY now
    bool applied_clock_lane() const { return applied_cl_; }
    sc_core::sc_time release_time() const { return release_time_; }
    sc_core::sc_time done_time() const { return done_time_; }
    unsigned startups() const { return startups_; }
    // Notified when a reset pulse ends, a startup completes or times out.
    const sc_core::sc_event& status_event() const { return status_ev_; }
    // (time, state_main) on every FSM state change, X shown as 0xFF.
    const std::vector<std::pair<sc_core::sc_time, unsigned>>& fsm_trace() const { return trace_; }
    std::string fsm_trace_text() const;

    DphyRxWrapper& phy() { return w_; }

    static constexpr const char* kStateMain = "phy.u_mipi_4_rx_dphy_ns.mipi_if_inst.state_main";
    static constexpr const char* kEnableLanes = "phy.u_mipi_4_rx_dphy_ns.mipi_if_inst.enable_lanes_int";

private:
    void clock_gen();
    void control_thread();
    void observe();
    void power_down();
    void startup();
    void apply_running_changes();
    uint8_t write_test_reg(uint8_t addr, uint8_t value);
    void cycles(unsigned n) { wait(cfg_.cfg_clk_period * n); }

    DphyRxWrapper& w_;
    DphyAdapterConfig cfg_;
    sc_core::sc_signal_rv<4>* state_main_ = nullptr;
    sc_core::sc_signal_resolved* enable_lanes_ = nullptr;

    // requested state (written by callers)
    bool req_phy_enable_ = false, req_cl_ = false;
    uint8_t req_dl_ = 0, req_forcerx_ = 0;
    bool reset_req_ = false;
    sc_core::sc_event ctrl_ev_;

    // applied state (adapter thread only)
    bool running_ = false;
    bool applied_cl_ = false;
    uint8_t applied_dl_ = 0, applied_forcerx_ = 0;
    bool pulse_active_ = false;
    bool timed_out_ = false;
    unsigned test_reg_mismatches_ = 0;
    unsigned startups_ = 0;
    sc_core::sc_time release_time_, done_time_;

    bool clk_on_ = false, clk_level_ = false;
    sc_core::sc_event clk_start_ev_, status_ev_, progress_ev_;
    std::vector<std::pair<sc_core::sc_time, unsigned>> trace_;
};

}  // namespace fx1::csi2
