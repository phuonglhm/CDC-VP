// Wrapper around the detailed D-PHY RX model (class mipirxphy).
//
// Binds all 272 top-level ports. Lane pads, the clock/reset/configuration
// inputs and the PPI outputs are public channels; every other port is tied
// off or sunk privately, as classified in tools/gen_dphy_wrapper_bind.py with
// the evidence in docs/PHY_ADAPTER_CONTRACT.md.
//
// Requirements of the model: SystemC time resolution 1 fs set before any
// sc_time exists; lane pads are vl::sres so LP (ST_PULL) and HS (strong)
// driving can be told apart.
//
// Each public control signal must have exactly one writer process (the
// startup fixture, later the controller PPI adapter). Values are 4-state
// sc_logic so X/Z stay visible to diagnostics.
//
// PLL gating (decision P13): the model's PLL core keeps its VCO toggling while
// the PLL is off (pllcoremodel: half_period = 1.8 when on = 0, and the VCO loop
// runs whenever half_period > 0), about 10^7 delta cycles per simulated ms with
// nothing driven. The PLL is off in RX mode (PHY_ADAPTER_CONTRACT §3.3), so the
// wrapper suspends the PLL core's processes after time 0 and resumes them, with
// a warning, if its `on` input ever rises. The model sources are not changed.
#pragma once

#include <memory>
#include <systemc>
#include <vector>

#include "vl/vl_rt.h"

class mipirxphy;

namespace fx1::csi2 {

class DphyRxWrapper : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(DphyRxWrapper);
    explicit DphyRxWrapper(sc_core::sc_module_name nm);
    ~DphyRxWrapper() override;

    // ---- lane pads (bind the transmitter BFM here)
    vl::sres clkp{"clkp"};
    vl::sres clkn{"clkn"};
    sc_core::sc_vector<vl::sres> dp{"dp", 4};
    sc_core::sc_vector<vl::sres> dn{"dn", 4};

    // ---- clocks, resets, configuration (inputs of the model)
    sc_core::sc_signal<sc_dt::sc_logic> cfg_clk{"cfg_clk", sc_dt::SC_LOGIC_0};      // mipi_pclk
    sc_core::sc_signal<sc_dt::sc_logic> refclk{"refclk", sc_dt::SC_LOGIC_0};        // mipi_clk (PLL only)
    sc_core::sc_signal<sc_dt::sc_logic> rstz{"rstz", sc_dt::SC_LOGIC_0};            // dphy_reset_n
    sc_core::sc_signal<sc_dt::sc_logic> shutdownz{"shutdownz", sc_dt::SC_LOGIC_0};
    sc_core::sc_signal<sc_dt::sc_logic> testclr{"testclr", sc_dt::SC_LOGIC_0};
    sc_core::sc_signal<sc_dt::sc_logic> testclk{"testclk", sc_dt::SC_LOGIC_0};
    sc_core::sc_signal<sc_dt::sc_logic> testen{"testen", sc_dt::SC_LOGIC_0};
    sc_core::sc_signal<sc_dt::sc_lv<8>> testdin{"testdin", sc_dt::sc_lv<8>(0)};
    sc_core::sc_signal<sc_dt::sc_logic> enableclk{"enableclk", sc_dt::SC_LOGIC_0};
    sc_core::sc_signal<sc_dt::sc_lv<4>> enable{"enable", sc_dt::sc_lv<4>(0)};       // data lanes 3..0
    sc_core::sc_signal<sc_dt::sc_lv<8>> cfgclkfreqrange{"cfgclkfreqrange", sc_dt::sc_lv<8>(0)};
    sc_core::sc_signal<sc_dt::sc_lv<7>> hsfreqrange{"hsfreqrange", sc_dt::sc_lv<7>(0)};
    sc_core::sc_signal<sc_dt::sc_logic> basedir{"basedir", sc_dt::SC_LOGIC_0};
    sc_core::sc_vector<sc_core::sc_signal<sc_dt::sc_logic>> forcerxmode{
        "forcerxmode", 4, [](const char* n, size_t) { return new sc_core::sc_signal<sc_dt::sc_logic>(n, sc_dt::SC_LOGIC_0); }};

    // ---- PPI and status outputs of the model
    sc_core::sc_signal_resolved rxbyteclkhs{"rxbyteclkhs"};
    sc_core::sc_signal_resolved rxclkactivehs{"rxclkactivehs"};
    sc_core::sc_signal_resolved stopstateclk{"stopstateclk"};
    sc_core::sc_signal_resolved rxulpsclknot{"rxulpsclknot"};
    sc_core::sc_signal_resolved rxskewcalhs{"rxskewcalhs"};  // one, common to all lanes
    sc_core::sc_signal_resolved direction{"direction"};
    sc_core::sc_signal_rv<8> testdout{"testdout"};
    sc_core::sc_vector<sc_core::sc_signal_resolved> stopstatedata{"stopstatedata", 4};
    sc_core::sc_vector<sc_core::sc_signal_rv<8>> rxdatahs{"rxdatahs", 4};
    sc_core::sc_vector<sc_core::sc_signal_resolved> rxvalidhs{"rxvalidhs", 4};
    sc_core::sc_vector<sc_core::sc_signal_resolved> rxactivehs{"rxactivehs", 4};
    sc_core::sc_vector<sc_core::sc_signal_resolved> rxsynchs{"rxsynchs", 4};
    sc_core::sc_vector<sc_core::sc_signal_resolved> errsoths{"errsoths", 4};
    sc_core::sc_vector<sc_core::sc_signal_resolved> errsotsynchs{"errsotsynchs", 4};
    sc_core::sc_vector<sc_core::sc_signal_resolved> errcontrol{"errcontrol", 4};
    sc_core::sc_vector<sc_core::sc_signal_resolved> erresc{"erresc", 4};
    sc_core::sc_vector<sc_core::sc_signal_resolved> errsyncesc{"errsyncesc", 4};
    sc_core::sc_vector<sc_core::sc_signal_resolved> rxulpsesc{"rxulpsesc", 4};

    mipirxphy& phy() { return *phy_; }

    // P13. Set before sc_start; true by default. pll_gated(): the PLL core's
    // processes are suspended now.
    bool pll_gating = true;
    bool pll_gated() const { return gated_; }

private:
    void start_of_simulation() override;
    void pll_gate_thread();
    void apply_pll_gate();

    struct Private;
    std::unique_ptr<Private> t_;
    std::unique_ptr<mipirxphy> phy_;
    std::vector<sc_core::sc_process_handle> pll_procs_;
    sc_core::sc_signal_resolved* pll_on_ = nullptr;
    bool gated_ = false, gate_armed_ = false;
};

}  // namespace fx1::csi2
