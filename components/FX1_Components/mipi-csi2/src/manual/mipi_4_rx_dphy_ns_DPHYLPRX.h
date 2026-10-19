#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYLPRX_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYLPRX_H

#include <memory>
#include "vl/vl_rt.h"
#include "rx_cells.h"
#include "mipi_4_rx_dphy_ns_DPHYLPRX_ULP.h"

// Low-power receiver of one lane: LP comparators, contention-detect comparators
// and ULP receivers.
//
// The LP and CD comparators separate LP signalling from HS signalling by the
// drive strength seen on the pad (the Verilog model uses a resistive rtran into
// a weakly driven trireg).  A pad driven with strong/supply strength (HS driver)
// reads as 0 on the LP/CD outputs; a pad driven with pull strength or weaker
// (LP driver) is passed through.  The strength is taken from the pad channel
// when it is a vl::sres; any other channel counts as strong, like an undeclared
// strength in Verilog.
class mipi_4_rx_dphy_ns_DPHYLPRX : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> dn{"dn"};
    sc_core::sc_out<sc_dt::sc_lv<2>> doutcd{"doutcd"};
    sc_core::sc_out<sc_dt::sc_lv<2>> doutlp{"doutlp"};
    sc_core::sc_out<sc_dt::sc_lv<2>> doutulp{"doutulp"};
    sc_core::sc_in<sc_dt::sc_logic> dp{"dp"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> ibiascd{"ibiascd"};
    sc_core::sc_in<sc_dt::sc_logic> ibiaslp{"ibiaslp"};
    sc_core::sc_in<sc_dt::sc_logic> poncd{"poncd"};
    sc_core::sc_in<sc_dt::sc_logic> ponlp{"ponlp"};
    sc_core::sc_in<sc_dt::sc_logic> ponulp{"ponulp"};
    sc_core::sc_in<sc_dt::sc_logic> prg_on{"prg_on"};
    sc_core::sc_in<sc_dt::sc_logic> vdd{"vdd"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    sc_core::sc_in<sc_dt::sc_logic> vrefcd{"vrefcd"};
    sc_core::sc_in<sc_dt::sc_logic> vreflp{"vreflp"};
    sc_core::sc_in<sc_dt::sc_logic> vss{"vss"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYLPRX);
    explicit mipi_4_rx_dphy_ns_DPHYLPRX(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1e6;  // time unit (fs): 1ns
    static constexpr double TP_ = 1e6;  // time precision (fs): 1ns
    static constexpr double LPRX_IO_DELAY_RISE = 26.0;
    static constexpr double LPRX_IO_DELAY_FALL = 26.0;

    // level-shifted enables and supporting logic
    sc_core::sc_signal_resolved ponlph{"ponlph", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ponlphz{"ponlphz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ponulph{"ponulph", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ponulphz{"ponulphz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved poncdh{"poncdh", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved poncdhz{"poncdhz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved poncmph{"poncmph", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved poncmphz{"poncmphz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved prg_onz{"prg_onz", sc_dt::SC_LOGIC_Z};
    // ESD-protected pad copies feeding the ULP receivers
    sc_core::sc_signal_resolved dp_intl{"dp_intl", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved dn_intl{"dn_intl", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ulp_1{"ulp_1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ulp_0{"ulp_0", sc_dt::SC_LOGIC_Z};

    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_vp_det_soc> I107, I140, I141;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_nand2x> sctg_I133;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_inv4x> sctg_I134, sctg_I135;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPRX_ULP> ULP_1, ULP_0;

    // comparator output buffers: two inertial stages of LPRX_IO_DELAY/2 each.
    // index: 0 = CD dn side, 1 = CD dp side, 2 = LP dn side, 3 = LP dp side
    struct stage {
        vl::logic<1> out = vl::LX();
        vl::logic<1> pend = vl::LX();
        bool pending = false;
        sc_core::sc_time due;
        sc_core::sc_event ev;
    };
    stage z_int_[4];
    stage z_out_[4];
    sc_core::sc_event_or_list trig_;

    // value with drive strength
    struct sv {
        vl::logic<1> v;
        int s;
    };
    static sv nmos_s(const sv& d, const vl::logic<1>& g);
    static sv resolve_s(const sv& a, const sv& b);

    void end_of_elaboration() override;
    void eval();
    void stage1();
    void stage2();
    void merge_ulp();
    void schedule(stage& st, const vl::logic<1>& v);
};

#endif
