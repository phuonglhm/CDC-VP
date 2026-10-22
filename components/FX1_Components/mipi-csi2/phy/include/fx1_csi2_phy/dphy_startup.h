// Standalone startup fixture of the D-PHY RX model for PHY-level tests: a
// DphyAdapter (the single writer of the PHY controls) switched on at t = 0
// with a fixed lane configuration, as if the controller had written
// PHY_CONFIG.phy_enable = 1.
//
// The sequence is model-derived (docs/PHY_ADAPTER_CONTRACT.md §3), not vendor
// confirmed. The lane pads are not driven here.
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <systemc>

#include "fx1_csi2_phy/dphy_adapter.h"
#include "fx1_csi2_phy/dphy_rx_wrapper.h"

namespace fx1::csi2 {

struct DphyStartupConfig {
    sc_core::sc_time cfg_clk_period{40, sc_core::SC_NS};  // 25 MHz
    uint8_t cfgclkfreqrange = 0x20;   // 4 * 25 - 68, see contract §2.2
    uint8_t hsfreqrange = 0x09;       // see contract §2.3
    uint8_t lane_enable = 0xF;        // data lanes 3..0
    bool enableclk = true;
    bool basedir = true;
    unsigned testclr_cycles = 4;      // testclr has no minimum width in the model
    sc_core::sc_time timeout{2, sc_core::SC_MS};
    // Test-interface register writes {address (8-bit), value}, applied after
    // testclr and before reset release; each is read back through testdout.
    // Default 0x09 = 0x10 (noalign_error=1): each lane reports only its own
    // SoT alignment instead of failing the whole link when another lane leads
    // lane 0; the controller deskews lanes itself (PHY_ADAPTER_CONTRACT §4.3).
    std::vector<std::pair<uint8_t, uint8_t>> test_regs{{0x09, 0x10}};
    sc_core::sc_time testclk_half_period{50, sc_core::SC_NS};
};

class DphyStartup : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(DphyStartup);
    DphyStartup(sc_core::sc_module_name nm, DphyRxWrapper& phy, DphyStartupConfig cfg = {});

    // Notified when the power-control FSM reaches state 7 with lanes enabled,
    // or when the sequence times out.
    const sc_core::sc_event& done_event() const { return done_ev_; }
    // The sequence completed (FSM state 7, lanes enabled). Says nothing about
    // the requested test-register configuration: check config_ok() as well.
    bool done() const { return done_; }
    // Every test-register write so far (all startups) read back as written.
    bool config_ok() const { return adapter_.config_ok(); }
    bool timed_out() const { return timed_out_; }
    sc_core::sc_time done_time() const { return adapter_.done_time(); }
    sc_core::sc_time release_time() const { return adapter_.release_time(); }
    const std::vector<std::pair<sc_core::sc_time, unsigned>>& fsm_trace() const { return adapter_.fsm_trace(); }
    std::string fsm_trace_text() const { return adapter_.fsm_trace_text(); }
    // Number of test-register writes whose read-back differed.
    unsigned test_reg_mismatches() const { return adapter_.test_reg_mismatches(); }

    // Reset pulse, then the full startup again.
    void request_reset() { reset_req_ev_.notify(sc_core::SC_ZERO_TIME); }

    DphyAdapter& adapter() { return adapter_; }

private:
    static DphyAdapterConfig to_adapter(const DphyStartupConfig& c);
    void run();

    DphyStartupConfig cfg_;
    DphyAdapter adapter_;
    bool done_ = false, timed_out_ = false;
    sc_core::sc_event done_ev_, reset_req_ev_;
};

}  // namespace fx1::csi2
